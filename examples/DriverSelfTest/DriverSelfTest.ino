// DriverSelfTest.ino - on-hardware self-test for the bottom-level drivers.
// Prints a parseable "[PASS]/[FAIL] name" line per check to Serial (USER CDC),
// plus a numeric detail. Validates driver fixes on the real ProMicro nRF52840
// without external hardware.

#include <NrfCrypto.h>
#include <NrfPeripherals.h>
#include <NrfPower.h>
#include <NrfRtc.h>

static int g_pass = 0, g_fail = 0;

// SWD-readable result mirror (serial capture is flaky on this host's USB-CDC).
// Read these by symbol address via J-Link after the board runs.
__attribute__((used)) volatile uint32_t g_results   = 0;          // bit i set => test i PASS
__attribute__((used)) volatile uint32_t g_testIdx   = 0;          // number of tests run
__attribute__((used)) volatile uint32_t g_sleepDtMs = 0;          // measured sleepMs(200) elapsed
__attribute__((used)) volatile uint32_t g_doneMarker = 0xDEADBEEFUL; // -> 0x600DC0DE at end

static void mark(const char* name, bool ok) {
  Serial.print(ok ? F("[PASS] ") : F("[FAIL] "));
  Serial.print(name);
  if (ok) { g_pass++; g_results |= (1UL << g_testIdx); } else { g_fail++; }
  g_testIdx++;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 4000UL) {}
  delay(300);
  Serial.println(F("=== DriverSelfTest begin ==="));

  // 1) ECB AES-128, FIPS-197 known-answer test (definitive AES-hw proof).
  mark("ECB.AES128.FIPS197", NrfEcb::selfTest());
  Serial.println();

  // 2) ECB with the NIST SP800-38A vector, byte-exact.
  {
    uint8_t key[16]   = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
    uint8_t plain[16] = {0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a};
    uint8_t exp[16]   = {0x3a,0xd7,0x7b,0xb4,0x0d,0x7a,0x36,0x60,0xa8,0x9e,0xca,0xf3,0x24,0x66,0xef,0x97};
    uint8_t got[16];
    bool ok = NrfEcb::encrypt(key, plain, got) && memcmp(got, exp, 16) == 0;
    mark("ECB.AES128.NIST", ok);
    Serial.print(F("  ct=")); for (int i=0;i<4;i++){ if(got[i]<16)Serial.print('0'); Serial.print((int)got[i],HEX);} Serial.println(F(".."));
  }

  // 3) TRNG: 32 bytes, must not be all-equal.
  {
    NrfRng::begin();
    uint8_t buf[32]; NrfRng::randomBytes(buf, sizeof(buf));
    bool allSame = true;
    for (size_t i=1;i<sizeof(buf);++i) if (buf[i]!=buf[0]){ allSame=false; break; }
    mark("RNG.entropy", !allSame);
    Serial.print(F("  bytes=")); for(int i=0;i<4;i++){ if(buf[i]<16)Serial.print('0'); Serial.print((int)buf[i],HEX); Serial.print(' ');} Serial.println();
  }

  // 4) Die temperature in a plausible indoor range.
  {
    float t = NrfTemp::readCelsius();
    bool ok = (t > 5.0f && t < 60.0f);
    mark("TEMP.range", ok); Serial.print(F("  ")); Serial.print(t); Serial.println(F(" C"));
  }

  // 5) TIMER3 free-running counter advances.
  {
    NrfTimer& tm = nrfTimer3(); tm.begin(1000000U); tm.start();
    uint32_t a = tm.counter(); delayMicroseconds(500); uint32_t b = tm.counter();
    mark("TIMER3.advances", b != a); Serial.print(F("  ")); Serial.print(a); Serial.print(F(" -> ")); Serial.println(b);
  }

  // 6) RTC2 counter advances (LFCLK).
  {
    NrfRtc& rtc = nrfRtc2(); rtc.begin(1000U); rtc.start();
    uint32_t a = rtc.counter(); delay(30); uint32_t b = rtc.counter();
    mark("RTC2.advances", b != a); Serial.print(F("  ")); Serial.print(a); Serial.print(F(" -> ")); Serial.println(b);
  }

  // 7) NrfPower::sleepMs - validates the RTC1 sleep fix. NOTE: millis() FREEZES
  //    during nRF52 System ON sleep (the CPU clock + SysTick stop), so we time
  //    the sleep with RTC2 instead, which runs off LFCLK and keeps counting.
  //    RTC2 was started at ~1 kHz in test 6, so ~200 ticks == ~200 ms.
  {
    NrfRtc& clk = nrfRtc2();
    uint32_t c0 = clk.counter();
    NrfPower::sleepMs(200);
    uint32_t ticks = (clk.counter() - c0) & 0x00FFFFFFUL;
    g_sleepDtMs = ticks;
    bool ok = (ticks >= 160UL && ticks <= 320UL);
    mark("POWER.sleepMs200", ok); Serial.print(F("  ")); Serial.print(ticks); Serial.println(F(" RTC2-ticks"));
  }

  g_doneMarker = 0x600DC0DEUL;   // setup completed all tests (SWD sentinel)

  Serial.print(F("=== DriverSelfTest done: "));
  Serial.print(g_pass); Serial.print(F(" PASS, "));
  Serial.print(g_fail); Serial.println(F(" FAIL ==="));
}

void loop() {
  delay(2000);
  Serial.print(F("[heartbeat] pass=")); Serial.print(g_pass);
  Serial.print(F(" fail=")); Serial.println(g_fail);
}
