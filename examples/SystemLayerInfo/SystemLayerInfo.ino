
static void printFlag(bool condition, const __FlashStringHelper *enabledText, const __FlashStringHelper *disabledText) {
    if (condition) {
        Serial.println(enabledText);
        return;
    }

    Serial.println(disabledText);
}

void setup() {
    Serial.begin(115200);
    const NrfBoardInfo board = nrfBoardInfo();
    const NrfBoardSupportStatus support = nrfBoardSupportStatus();
    const NrfBoardPowerInfo power = nrfBoardPowerInfo();
    const NrfSystemProfile &system = nrfSystemProfile();
    const NrfClockProfile &clock = nrfClockProfile();
    const NrfDebugConfig &debug = nrfDebugConfig();
    Serial.println(system.boardName);
    Serial.println(board.family);
    Serial.println(nrfSerialTopologyName(system.serialTopology));
    Serial.println(nrfRuntimeUsbModeName(system.runtimeUsbMode));
    Serial.println(nrfBootloaderInterfaceName(system.bootloaderInterface));
    Serial.println(nrfMonitorTransportName(system.monitorTransport));
    Serial.println(nrfUploadTransportName(system.uploadTransport));
    Serial.println(nrfUploadTriggerName(system.uploadTrigger));
    Serial.println(nrfDebugTransportName(system.debugTransport));
    printFlag(system.uploadTouch1200Declared, F("touch1200:declared"), F("touch1200:none"));
    printFlag(system.uploadTouch1200Verified, F("touch1200:verified"), F("touch1200:unverified"));
    printFlag(system.uf2UploadSupported, F("uf2:supported"), F("uf2:none"));
    printFlag(system.dfuUtilArgsBoardSpecific, F("dfuutil:board-specific"), F("dfuutil:generic"));
    Serial.println(nrfFlashProfileName(system.flashProfile));
    Serial.println(system.programFlashBytes);
    Serial.println(nrfRamProfileName(system.ramProfile));
    Serial.println(system.dataRamBytes);
    Serial.println(clock.cpuFrequencyHz);
    printFlag(clock.overclockSupported, F("true"), F("false"));
    Serial.println(clock.cpuClockSource);
    Serial.println(clock.lowFrequencyClockSource);
    Serial.println(clock.clockSourceEvidenceLevel);
    printFlag(clock.lowFrequencyClockDeclared, F("lfclk:declared"), F("lfclk:undeclared"));
    printFlag(support.pinMapVerified, F("pinmap:verified"), F("pinmap:unverified"));
    printFlag(support.batteryModelVerified, F("battery:verified"), F("battery:unverified"));
    printFlag(power.batterySenseAvailable, F("battery:present"), F("battery:none"));
    printFlag(power.batterySenseViaVddhDiv5, F("battery:vddhdiv5"), F("battery:pin"));
    Serial.println(nrfBatteryRaw());
    Serial.println(nrfBatteryMillivolts());
    printFlag(power.extVccControlAvailable, F("extvcc:present"), F("extvcc:none"));
    printFlag(debug.available(), F("debug:supported"), F("debug:none"));
    printFlag(debug.ideReady(), F("ide-debug:available"), F("ide-debug:none"));
    printFlag(debug.usbDebugSupported(), F("usb-debug:supported"), F("usb-debug:none"));
    printFlag(debug.probeRequired(), F("debug-probe:required"), F("debug-probe:none"));
    Serial.println(debug.probeName);
}

void loop() {
    delay(1000);
}