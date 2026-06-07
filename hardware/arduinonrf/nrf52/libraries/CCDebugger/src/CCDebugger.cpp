#include "CCDebugger.h"

// Arduino-pin -> absolute nRF pin map from the active variant.
extern const uint32_t g_ADigitalPinMap[];

// Raw nRF GPIO access (no device header needed): P0 base 0x50000000, P1 0x50000300.
#define CCG(base, off) (*(volatile uint32_t*)((base) + (off)))
enum { G_OUTSET = 0x508, G_OUTCLR = 0x50C, G_IN = 0x510,
       G_DIRSET = 0x518, G_DIRCLR = 0x51C, G_PINCNF = 0x700 };

static inline void resolve(uint8_t pin, uint32_t* base, uint32_t* bit) {
  uint32_t abs = g_ADigitalPinMap[pin];
  *base = (abs < 32) ? 0x50000000UL : 0x50000300UL;
  *bit = abs & 31u;
}
static inline void ccdelay(volatile uint32_t n) { while (n--) __asm__ volatile("nop"); }

CCDebugger::CCDebugger(uint8_t ddPin, uint8_t dcPin, uint8_t rstPin)
    : ddPin_(ddPin), dcPin_(dcPin), rstPin_(rstPin), ddIsOut_(true) {}

void CCDebugger::begin() {
  resolve(ddPin_, &ddBase_, &ddBit_);
  resolve(dcPin_, &dcBase_, &dcBit_);
  resolve(rstPin_, &rstBase_, &rstBit_);
  CCG(dcBase_, G_PINCNF + 4 * dcBit_) = 1;     // DC output
  CCG(rstBase_, G_PINCNF + 4 * rstBit_) = 1;   // RST output
  CCG(ddBase_, G_PINCNF + 4 * ddBit_) = 0;     // DD input (buffer connected)
  CCG(ddBase_, G_DIRSET) = (1u << ddBit_);     // start as output
  ddIsOut_ = true;
}

// --- bit / byte level ---------------------------------------------------------
inline void CCDebugger::dcHigh() { CCG(dcBase_, G_OUTSET) = (1u << dcBit_); ccdelay(2); }
inline void CCDebugger::dcLow()  { CCG(dcBase_, G_OUTCLR) = (1u << dcBit_); ccdelay(2); }
inline void CCDebugger::ddOut()  { if (!ddIsOut_) { CCG(ddBase_, G_DIRSET) = (1u << ddBit_); ddIsOut_ = true; } }
inline void CCDebugger::ddIn()   { if (ddIsOut_)  { CCG(ddBase_, G_DIRCLR) = (1u << ddBit_); ddIsOut_ = false; } }
inline void CCDebugger::ddWrite(bool v) { CCG(ddBase_, v ? G_OUTSET : G_OUTCLR) = (1u << ddBit_); }
inline bool CCDebugger::ddRead() { return (CCG(ddBase_, G_IN) >> ddBit_) & 1u; }

void CCDebugger::wr(uint8_t b) {
  ddOut();
  for (uint8_t i = 0; i < 8; ++i) {
    ddWrite(b & 0x80);
    dcHigh();
    b <<= 1;
    dcLow();
  }
}
uint8_t CCDebugger::rd() {
  uint8_t b = 0;
  ddIn();
  for (uint8_t i = 0; i < 8; ++i) {
    dcHigh();
    b = (uint8_t)((b << 1) | (ddRead() ? 1 : 0));
    dcLow();
  }
  return b;
}
uint8_t CCDebugger::switchRead() {
  ddIn();
  ccdelay(2);
  uint8_t w = 0;
  while (ddRead()) {                      // target holds DD high until ready
    for (uint8_t i = 0; i < 8; ++i) { dcHigh(); dcLow(); }
    if (++w >= 20) return 0xFF;
  }
  return w;
}

bool CCDebugger::enterDebug() {
  ddOut();
  CCG(rstBase_, G_OUTSET) = (1u << rstBit_);
  CCG(dcBase_, G_OUTCLR) = (1u << dcBit_);
  CCG(ddBase_, G_OUTCLR) = (1u << ddBit_);
  delay(10);
  CCG(rstBase_, G_OUTCLR) = (1u << rstBit_);
  ccdelay(40);
  dcHigh(); dcLow(); dcHigh(); dcLow();   // 2 clocks while RST low = enter debug
  ccdelay(40);
  CCG(rstBase_, G_OUTSET) = (1u << rstBit_);
  ccdelay(60);
  return true;
}

// --- debug commands -----------------------------------------------------------
uint8_t CCDebugger::cmd(uint8_t c) { wr(c); switchRead(); uint8_t r = rd(); ddOut(); return r; }
uint8_t CCDebugger::status() { return cmd(0x30); }
uint8_t CCDebugger::exec1(uint8_t a) { wr(0x51); wr(a); switchRead(); uint8_t r = rd(); ddOut(); return r; }
uint8_t CCDebugger::exec2(uint8_t a, uint8_t b) { wr(0x52); wr(a); wr(b); switchRead(); uint8_t r = rd(); ddOut(); return r; }
uint8_t CCDebugger::exec3(uint8_t a, uint8_t b, uint8_t c) { wr(0x53); wr(a); wr(b); wr(c); switchRead(); uint8_t r = rd(); ddOut(); return r; }
uint8_t CCDebugger::getReg(uint8_t r) { return exec2(0xE5, r); }          // MOV A,direct
void CCDebugger::setReg(uint8_t r, uint8_t v) { exec3(0x75, r, v); }      // MOV direct,#imm
uint8_t CCDebugger::getCfg() { return cmd(0x20); }
void CCDebugger::setCfg(uint8_t v) { wr(0x18); wr(v); switchRead(); rd(); ddOut(); }

uint16_t CCDebugger::chipID() {
  wr(0x68); switchRead();
  uint8_t h = rd(), l = rd();
  ddOut();
  return (uint16_t)((h << 8) | l);
}

bool CCDebugger::chipErase() {
  cmd(0x10);
  uint16_t g = 0;
  while ((status() & 0x80) && ++g < 4000) delay(2);   // CHIP_ERASE_BUSY
  return g < 4000;
}

void CCDebugger::writeXdata(uint16_t a, const uint8_t* d, uint8_t n) {
  exec3(0x90, (uint8_t)(a >> 8), (uint8_t)a);          // MOV DPTR,#a
  for (uint8_t i = 0; i < n; ++i) { exec2(0x74, d[i]); exec1(0xF0); exec1(0xA3); }  // MOV A,#d; MOVX @DPTR,A; INC DPTR
}
void CCDebugger::setDPTR(uint16_t a) { exec3(0x90, (uint8_t)(a >> 8), (uint8_t)a); }
void CCDebugger::selXdataBank(uint8_t bank) { uint8_t m = getReg(0xC7); m = (uint8_t)((m & 0xF8) | (bank & 7)); setReg(0xC7, m); }
void CCDebugger::burst(const uint8_t* d, uint16_t n) {
  wr((uint8_t)(0x80 | ((n >> 8) & 7))); wr((uint8_t)n);
  for (uint16_t i = 0; i < n; ++i) wr(d[i]);
  switchRead(); rd(); ddOut();
}

// --- flash via the CC2530 DMA engine -----------------------------------------
void CCDebugger::cfgDMA(uint8_t idx, uint16_t src, uint16_t dst, uint8_t trig,
                        uint16_t len, uint8_t srcInc, uint8_t dstInc, uint8_t prio) {
  uint8_t c[8];
  c[0] = (uint8_t)(src >> 8); c[1] = (uint8_t)src;
  c[2] = (uint8_t)(dst >> 8); c[3] = (uint8_t)dst;
  c[4] = (uint8_t)((len >> 8) & 0x1F); c[5] = (uint8_t)len;
  c[6] = (uint8_t)(trig & 0x1F);
  c[7] = (uint8_t)(((srcInc & 3) << 6) | ((dstInc & 3) << 4) | (1 << 3) | (prio & 3));
  uint16_t mem = (uint16_t)(0x1000 + idx * 8);
  writeXdata(mem, c, 8);
  if (idx == 0) { setReg(0xD4, (uint8_t)mem); setReg(0xD5, (uint8_t)(mem >> 8)); }
  else { mem = 0x1008; setReg(0xD2, (uint8_t)mem); setReg(0xD3, (uint8_t)(mem >> 8)); }
}
void CCDebugger::armDMA(uint8_t i) { uint8_t a = getReg(0xD6); a |= (1 << i); setReg(0xD6, a); delay(1); }
void CCDebugger::disarmDMA(uint8_t i) { uint8_t a = getReg(0xD6); a &= ~(1 << i); setReg(0xD6, a); }
bool CCDebugger::dmaIRQ(uint8_t i) { return getReg(0xD1) & (1 << i); }
void CCDebugger::clrDMAIRQ(uint8_t i) { uint8_t a = getReg(0xD1); a &= ~(1 << i); setReg(0xD1, a); }
void CCDebugger::faddr(uint16_t wo) { uint8_t d[2] = {(uint8_t)wo, (uint8_t)(wo >> 8)}; writeXdata(0x6271, d, 2); }
void CCDebugger::clrFlashCtl() { setDPTR(0x6270); uint8_t a = (uint8_t)(exec1(0xE0) & 0x1F); writeXdata(0x6270, &a, 1); }
void CCDebugger::setFlashWrite() { setDPTR(0x6270); uint8_t a = (uint8_t)(exec1(0xE0) | 0x02); writeXdata(0x6270, &a, 1); }

void CCDebugger::writePage(uint16_t pageNo, const uint8_t* data, uint16_t n) {
  for (uint16_t i = 0; i < 2048; ++i) page_[i] = (i < n) ? data[i] : 0xFF;
  clrFlashCtl(); clrDMAIRQ(0); clrDMAIRQ(1); disarmDMA(0); disarmDMA(1);
  armDMA(0); burst(page_, 2048);
  uint32_t w = 0; while (!dmaIRQ(0) && ++w < 20000) {} clrDMAIRQ(0);
  faddr((uint16_t)(pageNo * 512));         // flash word address = pageNo*2048/4
  armDMA(1); setFlashWrite();
  w = 0; while (!dmaIRQ(1) && ++w < 400000) {} clrDMAIRQ(1);
}
uint8_t CCDebugger::readCode(uint8_t bank, uint16_t off) {
  selXdataBank(bank); setDPTR((uint16_t)(0x8000 + off)); return exec1(0xE0);
}

bool CCDebugger::flashFirmware(const uint8_t* data, uint32_t len, void (*progress)(uint8_t)) {
  enterDebug();
  if (!chipErase()) return false;
  enterDebug();                            // re-enter to clear lock state
  uint8_t cf = getCfg(); cf &= ~0x04; setCfg(cf);   // clear DMA_PAUSE
  cfgDMA(0, 0x6260, 0x0000, 0x1F, 2048, 0, 1, 1);   // debug -> XRAM
  cfgDMA(1, 0x0000, 0x6273, 0x12, 2048, 1, 0, 2);   // XRAM -> flash, FLASH trigger
  uint32_t pages = (len + 2047) / 2048;
  for (uint32_t p = 0; p < pages; ++p) {
    uint32_t off = p * 2048;
    uint16_t n = (uint16_t)((len - off >= 2048) ? 2048 : (len - off));
    writePage((uint16_t)p, data + off, n);
    if (progress) progress((uint8_t)((p + 1) * 100 / pages));
  }
  // verify by read-back checksum
  uint32_t want = 0, got = 0;
  for (uint32_t i = 0; i < len; ++i) want += data[i];
  for (uint32_t p = 0; p < pages; ++p) {
    uint32_t off = p * 2048;
    uint16_t n = (uint16_t)((len - off >= 2048) ? 2048 : (len - off));
    uint8_t bank = (uint8_t)(off / 0x8000);
    selXdataBank(bank);
    setDPTR((uint16_t)(0x8000 + (off % 0x8000)));
    for (uint16_t i = 0; i < n; ++i) { got += exec1(0xE0); exec1(0xA3); }  // MOVX A,@DPTR; INC DPTR
  }
  return got == want;
}

void CCDebugger::run() {
  CCG(rstBase_, G_OUTCLR) = (1u << rstBit_); delay(20);
  CCG(rstBase_, G_OUTSET) = (1u << rstBit_);    // plain reset (no debug clocks) -> run
  CCG(ddBase_, G_DIRCLR) = (1u << ddBit_);      // release DD/DC
  CCG(dcBase_, G_DIRCLR) = (1u << dcBit_);
}
