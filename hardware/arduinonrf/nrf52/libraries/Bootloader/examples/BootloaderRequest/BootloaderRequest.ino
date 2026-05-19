#include <Bootloader.h>

void setup() {
  if (Bootloader.resetRequested()) {
    Bootloader.clearRequest();
  }
  Bootloader.resetToBootloader();
}

void loop() {
  delay(100);
}
