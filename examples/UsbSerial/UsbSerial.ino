
// nRF52840 POWER peripheral – read reset reason and diagnostic SRAM marker
#define NRF_POWER_BASE      0x40000000UL
#define NRF_POWER_RESETREAS (*(volatile uint32_t *)(NRF_POWER_BASE + 0x400UL))
// Diagnostic marker written by Reset_Handler to confirm it executed.
// Located in the SoftDevice-reserved SRAM zone (not cleared by bootloader or
// linker), so it survives NVIC_SystemReset() round-trips.
#define DIAG_SRAM (*(volatile uint32_t *)0x20004000UL)

void setup() {
    // --- Service CDC (always present, regardless of usbcdc menu) ----------
    // SerialService is backed by the fixed IF0/1 CDC pair and is active even
    // when the user CDC port is disabled in the boards.txt menu.
    SerialService.begin(115200);
    while (!SerialService) {}
    SerialService.println("=== USB serial example (service port) ===");

    // Print startup diagnostics on the service port so they are visible
    // regardless of the usbcdc menu selection.
    uint32_t diagSram  = DIAG_SRAM;
    uint32_t resetReas = NRF_POWER_RESETREAS;
    NRF_POWER_RESETREAS = resetReas;  // clear after reading (write 1 to clear)

    SerialService.print("diag_sram=0x");
    SerialService.println(diagSram, HEX);
    SerialService.print("resetreas=0x");
    SerialService.println(resetReas, HEX);
    SerialService.print("service_dtr=");
    SerialService.println(SerialService.dtr() ? "yes" : "no");

#if defined(NRF_SYSTEM_HAS_USB_CDC) && (NRF_SYSTEM_HAS_USB_CDC == 1)
    // --- User CDC (optional, enabled when usbcdc=enabled in boards.txt) ---
    Serial.begin(115200);
    while (!Serial) {}
    Serial.println("=== USB serial example (user port) ===");
    Serial.print("user_dtr=");
    Serial.println(Serial.dtr() ? "yes" : "no");
#endif
}

void loop() {
    SerialService.println("tick");
#if defined(NRF_SYSTEM_HAS_USB_CDC) && (NRF_SYSTEM_HAS_USB_CDC == 1)
    Serial.println("tick");
#endif
    delay(250);
}