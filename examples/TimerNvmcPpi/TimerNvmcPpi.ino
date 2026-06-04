// TimerNvmcPpi.ino - exercise the three new bottom-level drivers together.
//
//   1. NrfNvmc: read+write a persistent "boot count" in the last 4 KB of
//      flash (0xFF000). The counter survives reset and increments each boot.
//   2. NrfTimer: TIMER2 ticks at 10 kHz, fires CC0 every 5000 = 500 ms.
//      The ISR increments a counter the main loop reports.
//   3. NrfPpi: optional - if you want to wire TIMER2 CC1 to a GPIOTE task
//      so a pin toggles every 100 ms without any CPU involvement, that
//      lives in setup() as a commented hint (GPIOTE task API is in the
//      upcoming GPIOTE-output milestone).
//
// Skip TIMER0 - it's reserved for NimBLE / Zigbee controllers. TIMER1
// would be fine too; TIMER2 picked for a fresh demo.

#include <NrfPeripherals.h>

constexpr uint32_t BOOT_COUNT_FLASH_ADDR = 0x000FF000UL;   // last page of flash
constexpr uint32_t BOOT_COUNT_MAGIC      = 0xB00C0001UL;

volatile uint32_t g_timerTicks = 0;

void onTimer() {
  ++g_timerTicks;
  // Re-arm CC0 for another 500 ms.
  NrfTimer &t = nrfTimer2();
  t.setCompare(0, t.getCompare(0) + 5000U);
}

uint32_t loadBootCount() {
  const uint32_t *p = reinterpret_cast<const uint32_t *>(BOOT_COUNT_FLASH_ADDR);
  // Layout: magic (1 word) + count (1 word).
  if (p[0] != BOOT_COUNT_MAGIC) {
    return 0;   // uninitialized
  }
  return p[1];
}

void saveBootCount(uint32_t count) {
  // Each save needs a page erase since flash bits only go 1->0 without erase.
  // (For a wear-leveled counter use the EEPROM library instead.)
  if (!NrfNvmc::erasePage(BOOT_COUNT_FLASH_ADDR)) {
    Serial.println(F("  ! erase failed"));
    return;
  }
  uint32_t data[2] = {BOOT_COUNT_MAGIC, count};
  if (!NrfNvmc::writeWords(BOOT_COUNT_FLASH_ADDR, data, 2)) {
    Serial.println(F("  ! write failed"));
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000UL) {}
  Serial.println(F("TimerNvmcPpi"));

  // --- NVMC: persistent boot counter ----
  uint32_t bootCount = loadBootCount();
  ++bootCount;
  Serial.print(F("  boot #")); Serial.println(bootCount);
  saveBootCount(bootCount);

  // --- TIMER2: 10 kHz, ISR every 5000 ticks (500 ms) ----
  NrfTimer &t = nrfTimer2();
  if (!t.begin(10000U)) {
    Serial.println(F("  TIMER2 begin failed"));
    return;
  }
  t.attachCompareInterrupt(0, onTimer);
  t.setCompare(0, 5000U);
  t.start();

  // --- PPI: optional wiring example. Uncomment to allocate a channel
  // and route TIMER2 CC1 events to a GPIOTE-out task (requires GPIOTE
  // channel setup, which the current core does as an internal helper
  // only). The point of showing it here is the address-passing pattern.
  // const uint8_t ch = NrfPpi::wire(t.eventCompareAddr(1), GPIOTE_TASK_OUT_ADDR);
  // t.setCompare(1, 1000U);   // 100 ms

  Serial.print(F("  TIMER2 ticking at "));
  Serial.print(t.tickHz());
  Serial.println(F(" Hz; CC0 every 500 ms"));
}

void loop() {
  static uint32_t lastReported = 0;
  if (g_timerTicks != lastReported) {
    lastReported = g_timerTicks;
    Serial.print(F("ticks="));   Serial.print(g_timerTicks);
    Serial.print(F("  counter=")); Serial.println(nrfTimer2().counter());
  }
  delay(50);
}
