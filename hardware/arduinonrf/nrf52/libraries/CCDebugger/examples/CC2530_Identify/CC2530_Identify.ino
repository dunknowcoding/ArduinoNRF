/*
  CC2530_Identify - prove ArduinoNRF's built-in CC-Debugger reaches the chip.

  Enters debug mode over the 2-wire interface and reads the CC2530's chip ID.
  This is the first thing to run when wiring up a CC2530 - if you see 0xA5xx,
  the debug link is good and you can flash firmware (see the ArduinoNRF-Zigbee
  library's CC2530_FlashFirmware example).

  WIRING (CC2530 <- ArduinoNRF, 3.3 V):
      CC2530 P2.1 (DD)  <-> D8
      CC2530 P2.2 (DC)  <-> D9
      CC2530 RST        <-> D10
      CC2530 VCC -> 3V3     GND -> GND

  Open the Serial Monitor (USB) at 115200.
*/
#include <CCDebugger.h>

CCDebugger dbg(8, 9, 10);     // DD=D8, DC=D9, RST=D10

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  dbg.begin();
  dbg.enterDebug();
  uint16_t id = dbg.chipID();

  Serial.print("Chip ID: 0x");
  Serial.println(id, HEX);
  if ((id >> 8) == 0xA5) {
    Serial.println("-> CC2530 detected. Debug link OK; ready to flash.");
  } else {
    Serial.println("-> No CC2530. Check DD/DC/RST wiring, power (3.3 V), and GND.");
  }
  dbg.run();                  // release the chip so it runs whatever is flashed
}

void loop() {}
