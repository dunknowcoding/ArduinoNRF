#include "Arduino.h"

#include "NrfBoard.h"
#include "NrfSystem.h"

extern "C" int main(void) {
    init();
    initVariant();
    setup();
    for (;;) {
        loop();
        yield();
    }
}
