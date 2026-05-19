#include <Watchdog.h>

void setup() {
  Watchdog.begin(1500);
}

void loop() {
  Watchdog.feed();
  delay(100);
}
