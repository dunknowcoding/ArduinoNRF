#include <Arduino.h>
#include <NordicHardware.h>

void printYesNo(bool value) {
    if (value) {
        Serial.println("yes");
    } else {
        Serial.println("no");
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 500) {
        delay(10);
    }

    Serial.print("qspi-present=");
    printYesNo(NordicHardware.qspiPresent());
    if (NordicHardware.qspiPresent()) {
        uint32_t jedec = NordicHardware.qspiJedecId();
        Serial.print("jedec=0x");
        Serial.println(jedec, HEX);
    }
}

void loop() {
    delay(1000);
}