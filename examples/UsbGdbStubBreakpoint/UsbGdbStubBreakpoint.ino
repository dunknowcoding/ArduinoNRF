// Applicable boards: native-USB nRF52840 boards built with buildprofile=usbgdbstub.
// Limitations: this example only becomes useful after the firmware-side GDB stub transport is implemented.

#include <Arduino.h>
#include <NrfDebug.h>
#include <NrfUsbd.h>
#include <Watchdog.h>

static const unsigned long WAIT_MS = 1000;
static const uint32_t WATCHDOG_TIMEOUT_MS = 10000;

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

  while (!USBDevice.connected()) {
    delay(WAIT_MS);
  }

  // If a previous debug session left a breadcrumb and the watchdog reset the
  // board, print it for diagnostics, then clear it and CONTINUE to the
  // breakpoint. (We must not park here: the stub now feeds the watchdog while
  // halted, so waiting for the debugger no longer causes a reset loop, and a
  // terminal park would stop the board from ever reaching the breakpoint -
  // leaving GDB with nothing to talk to.)
  const uint8_t breadcrumb = nrfGdbStubBreadcrumb();
  if (Watchdog.causedReset() && breadcrumb != 0U) {
    Serial.print("gdbstub breadcrumb 0x");
    if (breadcrumb < 0x10U) {
      Serial.print('0');
    }
    Serial.println(breadcrumb, HEX);
    printTrace();
    nrfGdbStubClearBreadcrumb();
    nrfUsbdClearPollTrace();
  }

  Watchdog.begin(WATCHDOG_TIMEOUT_MS);

  __asm volatile("bkpt #0");
}

void loop() {
  delay(100);
}