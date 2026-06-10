// ThreadSmoke - bring up an OpenThread FTD node and form a single-node
// network: with no other nodes on the channel the device promotes itself
// to Leader within a few seconds.
//
// Watchable two ways:
//   * Serial: role transitions + RLOC16.
//   * SWD (J-Link, hands-free): the g[] array below.
//       g[0]=0x7EAD  g[1]=phase  g[2]=role  g[3]=rloc16
//       g[4]=role-change count   g[5]=loop counter  g[6..7]=EUI64 halves
#include <Thread.h>

__attribute__((used)) volatile uint32_t g[10] = {0};

static const uint8_t kNetworkKey[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
};

void setup() {
  Serial.begin(115200);
  g[0] = 0x7EAD;

  g[1] = 1;
  Thread.begin();

  uint8_t eui[8];
  Thread.getEui64(eui);
  g[6] = (uint32_t)eui[0] << 24 | (uint32_t)eui[1] << 16 | (uint32_t)eui[2] << 8 | eui[3];
  g[7] = (uint32_t)eui[4] << 24 | (uint32_t)eui[5] << 16 | (uint32_t)eui[6] << 8 | eui[7];

  g[1] = 2;
  Thread.setNetwork("ArduinoNRF", 11, 0xBEEF, kNetworkKey);

  g[1] = 3;
  Thread.start();
  g[1] = 4;
}

void loop() {
  static ThreadClass::Role lastRole = ThreadClass::ROLE_DISABLED;

  Thread.process();

  ThreadClass::Role now = Thread.role();
  if (now != lastRole) {
    lastRole = now;
    g[4]++;
    if (Serial) {
      Serial.print("[app] role -> ");
      Serial.print(Thread.roleString());
      Serial.print("  rloc16=0x");
      Serial.println(Thread.rloc16(), HEX);
    }
  }

  g[2] = (uint32_t)now;
  g[3] = Thread.rloc16();
  g[5]++;
}
