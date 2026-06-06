/* CC2530 802.15.4 radio co-processor (SDCC) for ArduinoNRF.
 *
 * Turns a CC2530 module into a UART-controlled 802.15.4 transceiver. Runs on the
 * 32 MHz XOSC started via CLKCONCMD (the path that works on clone modules where
 * the stock Z-Stack SLEEPCMD sequence hangs). Host protocol over UART0 @115200:
 *
 *   Host -> CC2530:  FE  LEN  CMD  [DATA..]  FCS      (FCS = XOR of LEN..DATA)
 *     0x01 PING                      -> 0x81 PONG [ver_hi ver_lo]
 *     0x02 SET_CHANNEL [ch 11..26]   -> 0x82 OK
 *     0x03 TX [psdu..]               -> 0x83 TXSTAT [0=ok/1=fail]
 *     0x04 SET_PROMISC [0|1]         -> 0x82 OK
 *   CC2530 -> Host (async):
 *     0x80 RESET_IND [ver_hi ver_lo]
 *     0x84 RX_FRAME [rssi lqi psdu..]
 */
#define FW_VER_HI 0
#define FW_VER_LO 1

/* ---- SFRs ---- */
__sfr __at (0xF1) PERCFG;
__sfr __at (0xF3) P0SEL;
__sfr __at (0x86) U0CSR;
__sfr __at (0xC4) U0UCR;
__sfr __at (0xC5) U0GCR;
__sfr __at (0xC2) U0BAUD;
__sfr __at (0xC1) U0DBUF;
__sfr __at (0xC6) CLKCONCMD;
__sfr __at (0x9E) CLKCONSTA;
__sfr __at (0x9D) SLEEPSTA;
__sfr __at (0xE1) RFST;
__sfr __at (0xD9) RFD;
__sfr __at (0x91) RFIRQF1;
__sfr __at (0xE9) RFIRQF0;
/* ---- RF XREGs ---- */
__xdata __at (0x6180) volatile unsigned char FRMFILT0;
__xdata __at (0x6189) volatile unsigned char FRMCTRL0;
__xdata __at (0x618F) volatile unsigned char FREQCTRL;
__xdata __at (0x6190) volatile unsigned char TXPOWER;
__xdata __at (0x6198) volatile unsigned char RSSI;
__xdata __at (0x619B) volatile unsigned char RXFIFOCNT;
__xdata __at (0x61AB) volatile unsigned char RXCTRL;
__xdata __at (0x61AC) volatile unsigned char FSCTRL;
__xdata __at (0x61AE) volatile unsigned char FSCAL1;
__xdata __at (0x61B2) volatile unsigned char AGCCTRL1;
__xdata __at (0x61B5) volatile unsigned char ADCTEST0;
__xdata __at (0x61B6) volatile unsigned char ADCTEST1;
__xdata __at (0x61B7) volatile unsigned char ADCTEST2;
__xdata __at (0x61FA) volatile unsigned char TXFILTCFG;

#define IRQ_FIFOP   0x04   /* RFIRQF0: frame available in RXFIFO */
#define IRQ_TXDONE  0x02   /* RFIRQF1: TX complete */

static __xdata unsigned char rxbuf[140];

static void clock_init(void){
  CLKCONCMD = 0x80;                 /* 32 MHz XOSC, 32 kHz RC */
  { unsigned int g=0; while(!(SLEEPSTA & 0x40) && ++g){} }  /* wait XOSC stable */
}
static void uart_init(void){
  PERCFG=0x00; P0SEL|=0x0C;         /* UART0 alt1, P0.2/P0.3 */
  U0CSR=0xC0;                       /* UART mode + RX enable */
  U0UCR=0x02;                       /* 8N1, no flow control */
  U0GCR=0x0B; U0BAUD=216;           /* 115200 @ 32 MHz */
}
static void utx(unsigned char c){ U0DBUF=c; while(!(U0CSR & 0x02)){} U0CSR &= ~0x02; }
static unsigned char urx_avail(void){ return U0CSR & 0x04; }
static unsigned char urx(void){ while(!(U0CSR & 0x04)){} return U0DBUF; }

static void radio_init(unsigned char ch){
  FRMCTRL0=0x40;                    /* AUTOCRC */
  FRMFILT0=0x00;                    /* promiscuous: receive all frames */
  TXFILTCFG=0x09; AGCCTRL1=0x15; FSCAL1=0x00; RXCTRL=0x3F; FSCTRL=0x55;
  ADCTEST0=0x10; ADCTEST1=0x0E; ADCTEST2=0x03;
  if(ch<11) ch=11; if(ch>26) ch=26;
  FREQCTRL=(unsigned char)(11 + 5*(ch-11));
  TXPOWER=0xF5;
  RFST=0xED;                        /* flush RX */
  RFST=0xE3;                        /* SRXON: enter RX */
}
/* transmit psdu[len]; radio appends FCS. returns 0 ok / 1 fail */
static unsigned char radio_tx(__xdata unsigned char* psdu, unsigned char len){
  unsigned int t; unsigned char i, done=0;
  RFST=0xEE;                        /* flush TX FIFO */
  RFD=(unsigned char)(len+2);       /* PHR = psdu + 2 FCS */
  for(i=0;i<len;i++) RFD=psdu[i];
  RFIRQF1=0;
  RFST=0xE9;                        /* STXON */
  for(t=0;t<60000;t++){ if(RFIRQF1 & IRQ_TXDONE){ done=1; break; } }
  RFST=0xE3;                        /* back to RX */
  return done?0:1;
}
/* poll RXFIFO; if a frame is present read it into rxbuf, return its length (incl
   2 trailing status bytes RSSI,CRC|LQI), else 0 */
static unsigned char radio_rx(void){
  unsigned char len, i;
  if(!(RFIRQF0 & IRQ_FIFOP)) return 0;
  len = RFD;                        /* first FIFO byte = frame length */
  if(len==0 || len>127){ RFST=0xED; RFIRQF0=0; return 0; }  /* bad -> flush */
  for(i=0;i<len;i++) rxbuf[i]=RFD;  /* psdu + RSSI + (CRC|LQI) */
  RFIRQF0=0;
  if(RXFIFOCNT==0){} else { RFST=0xED; }   /* drain leftovers */
  return len;
}
static void send_frame(unsigned char resp, __xdata unsigned char* d, unsigned char n){
  unsigned char i, fcs;
  utx(0xFE); utx((unsigned char)(n+1)); utx(resp);
  fcs=(unsigned char)((n+1) ^ resp);
  for(i=0;i<n;i++){ utx(d[i]); fcs^=d[i]; }
  utx(fcs);
}

void main(void){
  __xdata unsigned char cmd[140];
  unsigned char st=0, ln=0, idx=0, c, rlen;
  __xdata unsigned char tmp[3];
  clock_init();
  uart_init();
  radio_init(11);
  tmp[0]=FW_VER_HI; tmp[1]=FW_VER_LO;
  send_frame(0x80, tmp, 2);         /* RESET_IND boot announce */
  for(;;){
    /* radio RX -> host */
    rlen = radio_rx();
    if(rlen >= 2){
      /* report: 0x84 [rssi][crc|lqi][psdu...]  (psdu = rxbuf[0..rlen-3]) */
      utx(0xFE); utx((unsigned char)(rlen+1)); utx(0x84);
      { unsigned char i, fcs=(unsigned char)((rlen+1)^0x84);
        utx(rxbuf[rlen-2]); fcs^=rxbuf[rlen-2];     /* RSSI */
        utx(rxbuf[rlen-1]); fcs^=rxbuf[rlen-1];     /* CRC|LQI */
        for(i=0;i+2<rlen;i++){ utx(rxbuf[i]); fcs^=rxbuf[i]; }
        utx(fcs);
      }
    }
    /* UART command parser (non-blocking, one byte per loop) */
    if(urx_avail()){
      c=urx();
      switch(st){
        case 0: if(c==0xFE){ st=1; } break;
        case 1: ln=c; idx=0; st=2; if(ln==0||ln>137){ st=0; } break;
        case 2:
          cmd[idx++]=c;
          if(idx>=ln+1){                 /* got LEN payload bytes + FCS */
            unsigned char cc=cmd[0];
            if(cc==0x01){ tmp[0]=FW_VER_HI; tmp[1]=FW_VER_LO; send_frame(0x81,tmp,2); }
            else if(cc==0x02){ radio_init(cmd[1]); tmp[0]=0; send_frame(0x82,tmp,0); }
            else if(cc==0x03){ unsigned char r=radio_tx(&cmd[1],(unsigned char)(ln-1)); tmp[0]=r; send_frame(0x83,tmp,1); }
            else if(cc==0x04){ FRMFILT0 = cmd[1]?0x01:0x00; tmp[0]=0; send_frame(0x82,tmp,0); }
            st=0;
          }
          break;
      }
    }
  }
}
