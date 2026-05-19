// Applicable boards: all packaged boards with the shared nRF52 SAADC implementation.
// Limitations: validates ADC configuration and range handling, not board-specific analog accuracy.

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

void setup() {
  Serial.begin(115200);

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
  if (nrfAnalogInputSupported(SCK)) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }

  nrfAdcSetGain(NRF_ADC_GAIN_1_4);
  nrfAdcSetAcquisitionTimeUs(20);
  nrfAdcCalibrateOffset();

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
  const int sample = analogRead(A0);
  Serial.print("first sample: ");
  Serial.println(sample);
  pinMode(LED_BUILTIN, OUTPUT);

  analogReadResolution(12);
}

void loop() {
  int sample = analogRead(A0);
  analogWrite(LED_BUILTIN, sample);
  delay(20);
}
