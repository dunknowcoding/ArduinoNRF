// Applicable boards: native-USB nRF52840 boards built with buildprofile=usbgdbstub.
// Limitations: this example only becomes useful after the firmware-side GDB stub trap handling is implemented.

#include <Arduino.h>
#include <NrfDebug.h>
#include <NrfUsbd.h>
#include <Watchdog.h>

static const unsigned long WAIT_MS = 1000;
static const uint32_t WATCHDOG_TIMEOUT_MS = 10000;

static void printResetReport(uint8_t breadcrumb) {
  Serial.print("gdbstub breadcrumb 0x");
  if (breadcrumb < 0x10U) {
    Serial.print('0');
  }
  Serial.println(breadcrumb, HEX);
  printTrace();
}

static void printTrace() {
  const NrfUsbdPollTrace &trace = nrfUsbdPollTrace();
  Serial.print("usbd poll: ");
  Serial.println(trace.pollCalls);
  Serial.print("usbd irq: ");
  Serial.println(trace.irqCalls);
  Serial.print("ep0 setup: ");
  Serial.println(trace.ep0SetupEvents);
  Serial.print("ep0 data done: ");
  Serial.println(trace.ep0DataDoneEvents);
  Serial.print("cdc out: ");
  Serial.println(trace.cdcOutEvents);
  Serial.print("cdc in: ");
  Serial.println(trace.cdcInEvents);
  Serial.print("usb events: ");
  Serial.println(trace.usbEventEvents);
  Serial.print("event cause: 0x");
  Serial.println(trace.lastEventCause, HEX);
}

void setup() {
  Serial.begin(115200);

  while (!nrfDebugConfig().usbDebugSupported()) {
    delay(WAIT_MS);
  }

  const uint8_t breadcrumb = nrfGdbStubBreadcrumb();
  if (Watchdog.causedReset() && breadcrumb != 0U) {
    while (true) {
      if (USBDevice.connected()) {
        printResetReport(breadcrumb);
      }
      delay(WAIT_MS);
    }
  }

  while (!USBDevice.connected()) {
    delay(WAIT_MS);
  }

  Watchdog.begin(WATCHDOG_TIMEOUT_MS);

  volatile uint32_t *badAddress = reinterpret_cast<volatile uint32_t *>(0xFFFFFFFF);
  *badAddress = 0;
}

void loop() {
  delay(100);
}