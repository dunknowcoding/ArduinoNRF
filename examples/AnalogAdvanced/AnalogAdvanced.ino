// AnalogAdvanced.ino - the full SAADC feature tour: resolution, reference,
// gain, acquisition time, offset calibration, and per-pin analog capability.
// For a 3-line "just read A0" version, see the Analog example instead.
//
// Applicable boards: all packaged boards with the shared nRF52 SAADC.

void printYesNo(bool value) {
  Serial.println(value ? "yes" : "no");
}

void setup() {
  Serial.begin(115200);

  // Resolution is clamped to what the SAADC supports (8/10/12, oversampled to 14).
  analogReadResolution(8);
  Serial.print("resolution after 8-bit request: ");
  Serial.println(nrfAdcConfiguredResolutionBits());

  analogReadResolution(12);
  analogReference(AR_VDD4);
  Serial.print("resolution after 12-bit request: ");
  Serial.println(nrfAdcConfiguredResolutionBits());

  Serial.print("adc present: ");
  printYesNo(nrfAdcPresent());
  Serial.print("adc channels: ");
  Serial.println(nrfAdcChannelCount());
  Serial.print("native resolution: ");
  Serial.println(nrfAdcNativeResolutionBits());
  Serial.print("A0 supported: ");
  printYesNo(nrfAnalogInputSupported(A0));
  Serial.print("SCK supported as analog: ");
  printYesNo(nrfAnalogInputSupported(SCK));   // a non-analog pin -> "no"

  // Calibrate the SAADC offset at a known gain/acquisition setting.
  nrfAdcSetGain(NRF_ADC_GAIN_1_4);
  nrfAdcSetAcquisitionTimeUs(20);
  nrfAdcCalibrateOffset();

  // Pick a working configuration for real reads.
  nrfAdcSetReference(DEFAULT);
  nrfAdcSetGain(NRF_ADC_GAIN_1_6);
  nrfAdcSetAcquisitionTimeUs(10);
  analogReadResolution(14);
  Serial.print("reference: ");
  Serial.println(nrfAdcReference());
  Serial.print("gain: ");
  Serial.println(nrfAdcGain());
  Serial.print("acquisition time (us): ");
  Serial.println(nrfAdcAcquisitionTimeUs());
  Serial.print("resolution after 14-bit request: ");
  Serial.println(nrfAdcConfiguredResolutionBits());

  pinMode(A0, INPUT);
  Serial.print("first sample: ");
  Serial.println(analogRead(A0));

  pinMode(LED_BUILTIN, OUTPUT);
  analogReadResolution(12);
}

void loop() {
  // Mirror A0 onto the LED brightness as a quick visual.
  analogWrite(LED_BUILTIN, analogRead(A0));
  delay(20);
}
