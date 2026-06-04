// Applicable boards: all packaged nRF52 variants.
// Limitations: validates declared core capabilities only; it does not prove simultaneous peripheral activity on real hardware.

#include <NordicHardware.h>
#include <string.h>

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

void setup() {
  Serial.begin(115200);
  NordicHardware.startLowFrequencyClock();
  NordicHardware.startHighFrequencyClock();

  NordicPwmCapabilities pwm = NordicHardware.pwmCapabilities();
  NordicTimerCapabilities timers = NordicHardware.timerCapabilities();
  NordicAnalogCapabilities analog = NordicHardware.analogCapabilities();
  NordicSpiCapabilities spi = NordicHardware.spiCapabilities();
  NordicClockCapabilities clocks = NordicHardware.clockCapabilities();
  NrfBoardBusInfo buses = nrfBoardBusInfo();
  NrfPinInfo sdaInfo = NordicHardware.pinInfo(SDA);
  NrfPinInfo sckInfo = NordicHardware.pinInfo(SCK);
  NrfPinInfo a0Info = NordicHardware.pinInfo(PIN_A0);

  analogReadResolution(12);
  analogWriteResolution(12);
  SPI.begin();
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));

  NordicPwmCapabilities pwmConfigured = NordicHardware.pwmCapabilities();
  NordicAnalogCapabilities analogConfigured = NordicHardware.analogCapabilities();
  NordicSpiCapabilities spiConfigured = NordicHardware.spiCapabilities();

#if defined(ARDUINO_NRF52_NICENANO_V2) || defined(ARDUINO_NRF52_SUPERMINI) || defined(ARDUINO_NRF52_NRFMICRO)
  const bool expectSecondaryBuses = true;
  const bool expectClockDeclared = true;
#else
  const bool expectSecondaryBuses = false;
  const bool expectClockDeclared = false;
#endif

  Serial.print("expect secondary buses: ");
  printYesNo(expectSecondaryBuses);
  Serial.print("secondary SPI available: ");
  printYesNo(buses.secondarySpiAvailable);
  Serial.print("secondary Wire available: ");
  printYesNo(buses.secondaryWireAvailable);
  Serial.print("PWM channel capacity: ");
  Serial.println(pwm.channelCapacity);
  Serial.print("PWM shared timer: ");
  printYesNo(pwm.sharedTimer);
  Serial.print("tone supported: ");
  printYesNo(timers.toneSupported);
  Serial.print("servo supported: ");
  printYesNo(timers.servoSupported);
  Serial.print("ADC present: ");
  printYesNo(analog.adcPresent);
  Serial.print("ADC channels: ");
  Serial.println(analog.adcChannelCount);
  Serial.print("ADC native resolution: ");
  Serial.println(analog.adcNativeResolutionBits);
  Serial.print("ADC reference: ");
  Serial.println(analog.reference);
  Serial.print("ADC gain: ");
  Serial.println(analog.gain);
  Serial.print("ADC acquisition time: ");
  Serial.println(analog.acquisitionTimeUs);
  Serial.print("DAC present: ");
  printYesNo(analog.dacPresent);
  Serial.print("SPI supported: ");
  printYesNo(spi.supported);
  Serial.print("SPI max frequency: ");
  Serial.println(spi.maxFrequencyHz);
  Serial.print("CPU frequency: ");
  Serial.println(clocks.cpuFrequencyHz);
  Serial.print("overclock supported: ");
  printYesNo(clocks.overclockSupported);
  Serial.print("LF clock source: ");
  Serial.println(clocks.lowFrequencyClockSource);
  Serial.print("LF clock evidence: ");
  Serial.println(clocks.clockSourceEvidenceLevel);
  Serial.print("LF clock declared: ");
  printYesNo(clocks.lowFrequencyClockDeclared);
  Serial.print("SDA pin is Wire SDA: ");
  printYesNo(sdaInfo.wireSda);
  Serial.print("SCK pin is SPI SCK: ");
  printYesNo(sckInfo.spiSck);
  Serial.print("A0 valid: ");
  printYesNo(a0Info.valid);
  Serial.print("A0 analog input: ");
  printYesNo(a0Info.analogInput);
  Serial.print("configured PWM resolution: ");
  Serial.println(pwmConfigured.configuredResolutionBits);
  Serial.print("configured ADC resolution: ");
  Serial.println(analogConfigured.adcConfiguredResolutionBits);
  Serial.print("configured SPI frequency: ");
  Serial.println(spiConfigured.configuredFrequencyHz);

  SPI.endTransaction();
}

void loop() {
  delay(50);
}