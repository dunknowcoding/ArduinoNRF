
namespace {
constexpr uintptr_t DIAG_CAUSE_ADDR = 0x20004004UL;

uint32_t currentDiagCause() {
    return *reinterpret_cast<volatile uint32_t *>(DIAG_CAUSE_ADDR);
}

void clearDiagCause() {
    *reinterpret_cast<volatile uint32_t *>(DIAG_CAUSE_ADDR) = 0UL;
}

void printDiagCause(uint32_t cause) {
    Serial.print(F("diag=0x"));
    Serial.println(cause, HEX);
}
}

// Minimal USB CDC smoke test.
//
// When usbcdc=Disabled (boards.txt option, or the `-MinimalUsb` switch in the
// upload wrappers), the host sees a single SERVICE CDC interface and Arduino
// `Serial` is routed to it. The same single COM is also what `upload.ps1` uses
// for the 1200 bps touch + adafruit-nrfutil serial DFU on reflash.
//
// `Serial` is `true` only while the host has DTR asserted (= a monitor is open
// or adafruit-nrfutil holds the port). The sketch prints reset diagnostic
// info from SRAM 0x20004004 once at boot, then ticks every 500 ms whenever a
// monitor is connected, so the operator can confirm "user mode reached".

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && (millis() - t0) < 15000UL) {
        delay(10);
    }
    Serial.println(F("MinimalUsbSmoke: setup OK (cdc-disabled uses Service CDC / single COM path)"));
    const uint32_t startupDiagCause = currentDiagCause();
    printDiagCause(startupDiagCause);
    clearDiagCause();
}

void loop() {
    static unsigned long lastTickMs;
    static uint32_t lastDiagCause = 0xFFFFFFFFUL;

    const uint32_t diagCause = currentDiagCause();
    if (Serial && diagCause != lastDiagCause) {
        lastDiagCause = diagCause;
        printDiagCause(diagCause);
    }

    if (Serial && (millis() - lastTickMs) >= 500UL) {
        lastTickMs = millis();
        Serial.println(F("tick"));
    }
}
