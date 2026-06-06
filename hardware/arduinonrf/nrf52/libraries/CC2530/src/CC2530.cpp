#include "CC2530.h"

// Protocol (matches extras/firmware/cc2530_radio.c):
//   host -> CC2530:  FE  LEN  CMD  [DATA..]  FCS    (LEN = 1+len(DATA); FCS = XOR of LEN..DATA)
//   CC2530 -> host:  FE  LEN  RESP [DATA..]  FCS
//     RESP 0x80 RESET_IND [verHi verLo]   0x81 PONG [verHi verLo]
//          0x82 OK                         0x83 TXDONE [status]
//          0x84 RX_FRAME [rssi crc|lqi psdu..]
#define CMD_PING      0x01
#define CMD_CHANNEL   0x02
#define CMD_TX        0x03
#define CMD_PROMISC   0x04
#define RESP_RESET    0x80
#define RESP_PONG     0x81
#define RESP_OK       0x82
#define RESP_TXDONE   0x83
#define RESP_RX       0x84
// CC2530 datasheet RSSI offset (raw RSSI register value -> dBm)
#define RSSI_OFFSET   73

CC2530Class CC2530;

void CC2530Class::sendFrame(uint8_t cmd, const uint8_t* data, uint8_t n) {
  uint8_t len = (uint8_t)(n + 1);
  uint8_t fcs = (uint8_t)(len ^ cmd);
  ser_->write((uint8_t)0xFE);
  ser_->write(len);
  ser_->write(cmd);
  for (uint8_t i = 0; i < n; i++) { ser_->write(data[i]); fcs ^= data[i]; }
  ser_->write(fcs);
  ser_->flush();
}

// State machine over the framed protocol. Returns RESP code on a complete,
// FCS-valid frame (data/n = payload after RESP), else 0.
uint8_t CC2530Class::feed(uint8_t b, uint8_t* outData, uint8_t* outN) {
  switch (state_) {
    case 0:
      if (b == 0xFE) state_ = 1;
      break;
    case 1:
      len_ = b;
      idx_ = 0;
      if (len_ == 0 || len_ > sizeof(buf_) - 1) { state_ = 0; }
      else { state_ = 2; }
      break;
    case 2:
      buf_[idx_++] = b;
      if (idx_ >= (uint8_t)(len_ + 1)) {            // RESP+DATA (len_) + FCS
        state_ = 0;
        uint8_t fcs = len_;
        for (uint8_t i = 0; i < len_; i++) fcs ^= buf_[i];
        if (fcs == buf_[len_]) {                    // FCS ok
          *outData = 1;                             // (payload starts at buf_[1])
          *outN = (uint8_t)(len_ - 1);
          return buf_[0];                           // RESP code
        }
      }
      break;
  }
  return 0;
}

void CC2530Class::dispatchRx(const uint8_t* data, uint8_t n) {
  if (!rxcb_ || n < 2) return;                      // need rssi + crc|lqi
  int8_t rssi = (int8_t)((int)(int8_t)data[0] - RSSI_OFFSET);
  uint8_t lqi = data[1] & 0x7F;
  rxcb_(&data[2], (uint8_t)(n - 2), rssi, lqi);
}

bool CC2530Class::waitResp(uint8_t want, uint8_t* outData, uint8_t* outN,
                           uint32_t timeoutMs) {
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (ser_->available()) {
      uint8_t off, n;
      uint8_t resp = feed((uint8_t)ser_->read(), &off, &n);
      if (resp == 0) continue;
      if (resp == want) { if (outData) *outData = off; if (outN) *outN = n; return true; }
      if (resp == RESP_RX) dispatchRx(&buf_[off], n);
    }
  }
  return false;
}

bool CC2530Class::begin(HardwareSerial& serial, uint32_t baud) {
  ser_ = &serial;
  ser_->begin(baud);
  state_ = 0;
  // The module may still be booting (it sends a RESET_IND announce); retry PING
  // a few times, draining stale bytes before each attempt.
  for (uint8_t i = 0; i < 6; i++) {
    delay(60);
    while (ser_->available()) ser_->read();
    if (ping()) return true;
  }
  return false;
}

bool CC2530Class::ping(uint16_t* version) {
  sendFrame(CMD_PING, nullptr, 0);
  uint8_t off, n;
  if (!waitResp(RESP_PONG, &off, &n, 300)) return false;
  if (version && n >= 2) *version = (uint16_t)((buf_[off] << 8) | buf_[off + 1]);
  return true;
}

bool CC2530Class::setChannel(uint8_t channel) {
  uint8_t d = channel;
  sendFrame(CMD_CHANNEL, &d, 1);
  uint8_t off, n;
  return waitResp(RESP_OK, &off, &n, 300);
}

bool CC2530Class::setPromiscuous(bool on) {
  uint8_t d = on ? 1 : 0;
  sendFrame(CMD_PROMISC, &d, 1);
  uint8_t off, n;
  return waitResp(RESP_OK, &off, &n, 300);
}

bool CC2530Class::send(const uint8_t* psdu, uint8_t len) {
  if (len > kMaxPsdu) return false;
  sendFrame(CMD_TX, psdu, len);
  uint8_t off, n;
  if (!waitResp(RESP_TXDONE, &off, &n, 500)) return false;
  return (n >= 1) && (buf_[off] == 0);              // status 0 = ok
}

void CC2530Class::poll() {
  if (!ser_) return;
  while (ser_->available()) {
    uint8_t off, n;
    uint8_t resp = feed((uint8_t)ser_->read(), &off, &n);
    if (resp == RESP_RX) dispatchRx(&buf_[off], n);
  }
}
