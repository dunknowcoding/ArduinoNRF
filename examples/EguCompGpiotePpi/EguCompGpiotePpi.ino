// EguCompGpiotePpi.ino - demonstrates the four newest small peripherals
// working together with PPI as the glue (zero CPU on the hot path):
//
//   * NrfTimer  - TIMER1 ticks at 100 kHz and fires CC0 every 50 ms.
//   * NrfGpioteOut - allocates a GPIOTE output channel on LED_BUILTIN with
//                     POLARITY=TOGGLE. The CPU never writes the LED again.
//   * NrfPpi    - wires TIMER1's COMPARE[0] event directly to the GPIOTE
//                  toggle task. Result: LED blinks at 10 Hz (50 ms on,
//                  50 ms off) with zero CPU involvement.
//   * NrfEgu    - software-triggered IRQ from the main loop: pressing
//                  "g" + Enter in the Serial monitor fires EGU0 channel 0,
//                  whose ISR prints a heartbeat counter.

#include <NrfPeripherals.h>

volatile uint32_t g_eguTicks = 0;
void onEguTrigger() { ++g_eguTicks; }

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000UL) {}
  Serial.println(F("EguCompGpiotePpi"));

  pinMode(LED_BUILTIN, OUTPUT);

  // 1. Allocate GPIOTE channel for LED_BUILTIN, TOGGLE mode.
  const uint8_t gpioCh = NrfGpioteOut::allocate(LED_BUILTIN, /*initialHigh=*/false,
                                                  NrfGpioteOut::MODE_TOGGLE);
  if (gpioCh == NrfGpioteOut::INVALID_CHANNEL) {
    Serial.println(F("  ! GPIOTE allocate failed"));
    return;
  }
  Serial.print(F("  GPIOTE channel: ")); Serial.println(gpioCh);

  // 2. TIMER1 @ 100 kHz, CC0 every 5000 ticks (50 ms).
  NrfTimer &t = nrfTimer1();
  if (!t.begin(100000UL)) {
    Serial.println(F("  ! TIMER1 begin failed"));
    return;
  }
  t.setCompare(0, 5000U);
  // Hardware auto-clear on CC0 hit:
  // The SHORTS register at TIMER1_BASE + 0x200, bit 0 = COMPARE0_CLEAR.
  *reinterpret_cast<volatile uint32_t *>(0x40009200UL) = 1UL;
  t.start();

  // 3. PPI: TIMER1 COMPARE[0] event -> GPIOTE channel TASK_OUT.
  const uint8_t ppiCh = NrfPpi::wire(t.eventCompareAddr(0),
                                       NrfGpioteOut::taskOutAddr(gpioCh));
  if (ppiCh == NrfPpi::INVALID_CHANNEL) {
    Serial.println(F("  ! PPI wire failed"));
    return;
  }
  Serial.print(F("  PPI channel: ")); Serial.println(ppiCh);
  Serial.println(F("  LED blinking 10 Hz with ZERO CPU involvement"));

  // 4. EGU0 channel 0 -> ISR onEguTrigger.
  nrfEgu0().attachInterrupt(0, onEguTrigger);

  Serial.println(F("Send 'g' + Enter to fire EGU0 from software."));
}

void loop() {
  // Main loop just demonstrates that the LED keeps blinking even while
  // the CPU is busy printing or sleeping.
  if (Serial.available() && Serial.read() == 'g') {
    nrfEgu0().trigger(0);   // synchronous SW-triggered interrupt
  }
  static uint32_t lastReported = 0;
  if (g_eguTicks != lastReported) {
    lastReported = g_eguTicks;
    Serial.print(F("EGU ticks: ")); Serial.println(g_eguTicks);
  }
  delay(100);
}
