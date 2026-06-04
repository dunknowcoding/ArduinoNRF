// RtcAlarm.ino - schedule a periodic alarm on the nRF52 RTC2.
//
// Demonstrates the bottom-level NrfRtc driver: 1 kHz tick rate, a compare
// channel fires every 500 ms via the RTC IRQ, the ISR toggles a flag, and the
// main loop pulses LED_BUILTIN and reports timing.
//
// Why use the RTC instead of millis()/SysTick?
//   * Clocked from LFCLK -> the RTC runs through System ON sleep, so a sketch
//     that calls __WFI() between events still wakes on schedule (millis()
//     stops when SysTick stops in deep sleep).
//   * Three independent RTCs with 4 compare channels each = 12 hardware
//     alarms without leaning on the single SysTick.
//
// Peripheral safety:
//   * RTC0 is reserved by the Nordic SoftDevice on with-SD profiles -- pick
//     RTC1 or RTC2 on those. The verified promicroserialnosd path leaves all
//     three free.

#include <NrfRtc.h>

volatile uint32_t g_tickCount = 0;
volatile uint32_t g_overflowCount = 0;

void onAlarm() {
  ++g_tickCount;
  // Re-arm CC0 for another 500 ms from this point.
  NrfRtc &rtc = nrfRtc2();
  rtc.setCompare(0, (rtc.counter() + (rtc.tickHz() / 2U)) & 0x00FFFFFFUL);
}

void onOverflow() {
  ++g_overflowCount;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000UL) {}
  Serial.println(F("NrfRtc demo (RTC2 @ 1 kHz, alarm every 500 ms)"));

  pinMode(LED_BUILTIN, OUTPUT);

  NrfRtc &rtc = nrfRtc2();
  if (!rtc.begin(1000U)) {                       // 1 kHz tick -> 1 ms period
    Serial.println(F("RTC2 begin failed"));
    return;
  }
  Serial.print(F("  configured tick rate: ")); Serial.print(rtc.tickHz());
  Serial.print(F(" Hz, prescaler="));            Serial.print(rtc.prescaler());
  Serial.print(F(", period="));                  Serial.print(rtc.periodUs());
  Serial.println(F(" us"));

  rtc.attachCompareInterrupt(0, onAlarm);
  rtc.attachOverflowInterrupt(onOverflow);
  rtc.setCompare(0, rtc.counter() + 500U);
  rtc.start();
}

void loop() {
  // Lightweight main loop - the alarm work happens entirely in the ISR.
  static uint32_t lastReportedTicks = 0;
  const uint32_t now = g_tickCount;
  if (now != lastReportedTicks) {
    lastReportedTicks = now;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    Serial.print(F("alarm ")); Serial.print(now);
    Serial.print(F(", counter="));     Serial.print(nrfRtc2().counter());
    Serial.print(F(", overflows="));   Serial.println(g_overflowCount);
  }
  delay(10);
}
