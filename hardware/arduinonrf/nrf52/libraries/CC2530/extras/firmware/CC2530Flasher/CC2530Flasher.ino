/*
  CC2530Flasher - Flash the cc2530_radio firmware into a CC2530 using an
  ArduinoNRF board as the programmer (no CC-Debugger required).

  It bit-bangs TI's CC2530 debug interface, chip-erases, writes the embedded
  firmware (cc2530_fw.h) via the flash DMA, reads it back to verify, and reports
  progress over the USB Serial Monitor. Run this ONCE per module.

  Wiring (ProMicro nRF52840 -> CC2530, debug only):
    D8  -> P2.1 (DD, Debug Data)
    D9  -> P2.2 (DC, Debug Clock)
    D10 -> RST
    GND -> GND,  3V3 -> VCC
  (For other nRF52840 boards, set DD_PIN/DC_PIN/RST_PIN to pins whose absolute
   P-numbers match the constants below, or rewire to D8/D9/D10.)

  Open the Serial Monitor @115200 and watch for "FLASH OK".
*/
#include <Arduino.h>
#include "cc2530_fw.h"   // const unsigned char CC2530_FW[]; CC2530_FW_LEN;

// Absolute nRF52840 GPIO of D8/D9/D10 on the ProMicro variant.
#define DDb 4    /* D8  = P1.04 (DD) */
#define DCb 6    /* D9  = P1.06 (DC) */
#define RSTb 9   /* D10 = P0.09 (RST) */
#define REG(a) (*(volatile uint32_t*)(a))
#define P0_OUTSET REG(0x50000508)
#define P0_OUTCLR REG(0x5000050C)
#define P1_OUTSET REG(0x50000808)
#define P1_OUTCLR REG(0x5000080C)
#define P1_IN     REG(0x50000810)
#define P1_DIRSET REG(0x50000818)
#define P1_DIRCLR REG(0x5000081C)
#define P0_CNF(n) REG(0x50000700 + 4*(n))
#define P1_CNF(n) REG(0x50000A00 + 4*(n))

static const uint32_t PAGE = 2048;
static uint8_t page[2048], rb[2048];

static inline void dcH(){P1_OUTSET=(1u<<DCb);delayMicroseconds(1);}
static inline void dcL(){P1_OUTCLR=(1u<<DCb);delayMicroseconds(1);}
static inline void ddO(){P1_DIRSET=(1u<<DDb);}
static inline void ddI(){P1_DIRCLR=(1u<<DDb);}
static void wr(uint8_t b){ddO();for(int i=0;i<8;i++){ if(b&0x80)P1_OUTSET=(1u<<DDb); else P1_OUTCLR=(1u<<DDb); dcH();dcL();b<<=1;}}
static uint8_t rd(){uint8_t b=0;ddI();for(int i=0;i<8;i++){dcH();b=(b<<1)|((P1_IN>>DDb)&1u);dcL();}return b;}
static uint8_t sw(){ddI();delayMicroseconds(1);uint8_t w=0;while((P1_IN>>DDb)&1u){for(int i=0;i<8;i++){dcH();dcL();}if(++w>=20)return 0xFF;}return w;}
static void enter(){P1_CNF(DCb)=1;P1_CNF(DDb)=0;P0_CNF(RSTb)=1;ddO();
  P0_OUTSET=(1u<<RSTb);P1_OUTCLR=(1u<<DCb);P1_OUTCLR=(1u<<DDb);delay(10);
  P0_OUTCLR=(1u<<RSTb);delayMicroseconds(200);dcH();dcL();dcH();dcL();delayMicroseconds(200);
  P0_OUTSET=(1u<<RSTb);delayMicroseconds(300);}
static uint8_t cmdR(uint8_t c){wr(c);sw();uint8_t a=rd();ddO();return a;}
static uint8_t status(){return cmdR(0x30);}
static uint8_t ex1(uint8_t a){wr(0x51);wr(a);sw();uint8_t r=rd();ddO();return r;}
static uint8_t ex2(uint8_t a,uint8_t b){wr(0x52);wr(a);wr(b);sw();uint8_t r=rd();ddO();return r;}
static uint8_t ex3(uint8_t a,uint8_t b,uint8_t c){wr(0x53);wr(a);wr(b);wr(c);sw();uint8_t r=rd();ddO();return r;}
static uint16_t chipID(){wr(0x68);sw();uint8_t h=rd();uint8_t l=rd();ddO();return (h<<8)|l;}
static uint8_t getCfg(){return cmdR(0x20);}
static void setCfg(uint8_t c){wr(0x18);wr(c);sw();rd();ddO();}
static uint8_t getReg(uint8_t r){return ex2(0xE5,r);}
static void setReg(uint8_t r,uint8_t v){ex3(0x75,r,v);}
static void wX(uint16_t a,const uint8_t*d,int n){ex3(0x90,(a>>8)&0xFF,a&0xFF);for(int i=0;i<n;i++){ex2(0x74,d[i]);ex1(0xF0);ex1(0xA3);}}
static void setDPTR(uint16_t a){ex3(0x90,(a>>8)&0xFF,a&0xFF);}
static void selBank(uint8_t bk){uint8_t a=getReg(0xC7);a=(a&0xF8)|(bk&7);setReg(0xC7,a);}
static void burst(const uint8_t*d,uint32_t n){wr(0x80|((n>>8)&7));wr(n&0xFF);for(uint32_t i=0;i<n;i++)wr(d[i]);sw();rd();ddO();}
static void armDMA(int i){uint8_t a=getReg(0xD6);a|=(1<<i);setReg(0xD6,a);delay(1);}
static void disarmDMA(int i){uint8_t a=getReg(0xD6);a&=~(1<<i);setReg(0xD6,a);}
static bool dmaIRQ(int i){return getReg(0xD1)&(1<<i);}
static void clrIRQ(int i){uint8_t a=getReg(0xD1);a&=~(1<<i);setReg(0xD1,a);}
static void cfgDMA(int idx,uint16_t src,uint16_t dst,uint8_t trig,uint16_t tlen,uint8_t si,uint8_t di,uint8_t prio){
  uint8_t c[8];c[0]=(src>>8)&0xFF;c[1]=src&0xFF;c[2]=(dst>>8)&0xFF;c[3]=dst&0xFF;
  c[4]=((tlen>>8)&0x1F);c[5]=tlen&0xFF;c[6]=(trig&0x1F);c[7]=((si&3)<<6)|((di&3)<<4)|(1<<3)|(prio&3);
  uint16_t mem=0x1000+idx*8;wX(mem,c,8);
  if(idx==0){setReg(0xD4,mem&0xFF);setReg(0xD5,(mem>>8)&0xFF);}else{mem=0x1008;setReg(0xD2,mem&0xFF);setReg(0xD3,(mem>>8)&0xFF);}}
static void faddr(uint16_t wo){uint8_t d[2]={(uint8_t)(wo&0xFF),(uint8_t)((wo>>8)&0xFF)};wX(0x6271,d,2);}
static void clrFlash(){uint8_t a;setDPTR(0x6270);a=ex1(0xE0);a&=0x1F;wX(0x6270,&a,1);}
static void setFlashWrite(){uint8_t a;setDPTR(0x6270);a=ex1(0xE0);a|=0x02;wX(0x6270,&a,1);}

static void writePage(uint16_t pageNo,const uint8_t*data,int n){
  for(int i=0;i<2048;i++) page[i]=(i<n)?data[i]:0xFF;
  clrFlash();clrIRQ(0);clrIRQ(1);disarmDMA(0);disarmDMA(1);
  armDMA(0); burst(page,2048);
  uint32_t w=0; while(!dmaIRQ(0)&&w<20000)w++; clrIRQ(0);
  faddr((uint16_t)(pageNo*512));
  armDMA(1); setFlashWrite();
  w=0; while(!dmaIRQ(1)&&w<400000)w++; clrIRQ(1);
}

void setup(){
  Serial.begin(115200);
  while(!Serial){}
  Serial.println("CC2530Flasher: cc2530_radio firmware");
  enter();
  uint16_t id=chipID();
  Serial.print("  chip id = 0x"); Serial.println(id, HEX);
  if((id & 0xFF00) != 0xA500){ Serial.println("  ERROR: no CC2530 - check DD/DC/RST wiring."); return; }
  Serial.println("  chip erase...");
  cmdR(0x10); uint32_t it=0; while((status()&0x80)&&it<400){delay(5);it++;}
  delay(10); enter();
  uint8_t cf=getCfg(); cf&=~0x04; setCfg(cf);
  cfgDMA(0,0x6260,0x0000,0x1F,2048,0,1,1);
  cfgDMA(1,0x0000,0x6273,0x12,2048,1,0,2);
  uint32_t npages=(CC2530_FW_LEN + PAGE - 1)/PAGE;
  Serial.print("  writing "); Serial.print(CC2530_FW_LEN); Serial.print(" bytes ("); Serial.print(npages); Serial.println(" page(s))...");
  for(uint32_t p=0; p<npages; p++){
    int n=(int)((CC2530_FW_LEN - p*PAGE) > PAGE ? PAGE : (CC2530_FW_LEN - p*PAGE));
    writePage((uint16_t)p, &CC2530_FW[p*PAGE], n);
  }
  // read back + verify
  Serial.println("  verifying...");
  bool ok=true; uint32_t bad=0;
  for(uint32_t p=0; p<npages && ok; p++){
    selBank((uint8_t)(p/16)); setDPTR((uint16_t)(0x8000 + (p%16)*PAGE));
    int n=(int)((CC2530_FW_LEN - p*PAGE) > PAGE ? PAGE : (CC2530_FW_LEN - p*PAGE));
    for(int i=0;i<n;i++){ if(ex1(0xE0) != CC2530_FW[p*PAGE+i]){ ok=false; bad=p*PAGE+i; break; } ex1(0xA3); }
  }
  // run the firmware (plain reset, release debug pins)
  P0_OUTCLR=(1u<<RSTb);delay(20);P0_OUTSET=(1u<<RSTb);
  P1_DIRCLR=(1u<<DDb);P1_DIRCLR=(1u<<DCb);
  if(ok) Serial.println("FLASH OK - firmware running. Wire UART (D0/D1) + CFG1->GND and use the CC2530 library.");
  else { Serial.print("FLASH VERIFY FAILED at byte "); Serial.println(bad); }
}
void loop(){}
