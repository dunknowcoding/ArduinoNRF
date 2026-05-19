#include <NordicHardware.h>

void setup() {
  NordicHardware.startLowFrequencyClock();
  NordicHardware.startHighFrequencyClock();
  NordicHardware.boardInfo();
  NordicHardware.boardSupportStatus();
  NordicHardware.boardPowerInfo();
  NordicHardware.pwmCapabilities();
  NordicHardware.analogCapabilities();
  NordicHardware.spiCapabilities();
  NordicHardware.clockCapabilities();
  NordicHardware.pinInfo(PIN_A0);
  NordicHardware.pinInfo(SCK);
  NordicHardware.deviceIdWord(0);
  NordicHardware.deviceIdWord(1);
  NordicHardware.temperatureC();
  NordicHardware.random32();
}

void loop() {
  delay(100);
}
