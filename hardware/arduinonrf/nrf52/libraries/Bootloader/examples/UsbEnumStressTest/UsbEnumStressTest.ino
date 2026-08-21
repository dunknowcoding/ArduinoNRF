// UsbEnumStressTest - regression test that USB-CDC enumeration is immune to a
// user loop() that never returns to the core.
//
// Both the SERVICE CDC (upload/DFU) and the USER CDC (this sketch's Serial) are
// interfaces of one composite device; enumeration is serviced in the USBD ISR,
// so it never depends on loop() yielding. Poll-only USB is intentionally not a
// supported build mode because this loop would starve it and drop the COM ports.
//
// Expected on a native-USB nRF52840 board (e.g. promicro_nrf52840), usbcdc=enabled:
//   * Both COM ports enumerate within ~1s and stay visible indefinitely.
//   * The board is still re-uploadable over USB (the 1200-bps DFU touch is also
//     captured in the ISR and its confirmation timer is completed by SysTick),
//     so a non-yielding sketch never bricks the port.
//   * The LED blinks purely from a busy-wait, proving loop() never yields.

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  // Pathological user code: an infinite loop that NEVER returns to the core and
  // NEVER yields (no delay()/yield()/poll()).
  for (;;) {
    digitalWrite(LED_BUILTIN, HIGH);
    for (volatile uint32_t i = 0; i < 1500000UL; ++i) { (void)i; }
    digitalWrite(LED_BUILTIN, LOW);
    for (volatile uint32_t i = 0; i < 1500000UL; ++i) { (void)i; }
  }
}
