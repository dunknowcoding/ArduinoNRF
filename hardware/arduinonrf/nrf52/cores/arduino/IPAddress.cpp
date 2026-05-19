#include "IPAddress.h"

#include <stdlib.h>

#include "Print.h"
#include "WString.h"

bool IPAddress::fromString(const char *text) {
    if (text == nullptr) {
        return false;
    }

    uint8_t parsed[4] = {0U, 0U, 0U, 0U};
    const char *cursor = text;
    for (int index = 0; index < 4; ++index) {
        char *end = nullptr;
        unsigned long value = strtoul(cursor, &end, 10);
        if (end == cursor || value > 255UL) {
            return false;
        }
        parsed[index] = static_cast<uint8_t>(value);
        if (index < 3) {
            if (*end != '.') {
                return false;
            }
            cursor = end + 1;
        } else if (*end != '\0') {
            return false;
        }
    }

    for (int index = 0; index < 4; ++index) {
        octets_[index] = parsed[index];
    }
    return true;
}

bool IPAddress::fromString(const String &text) {
    return fromString(text.c_str());
}

size_t IPAddress::printTo(Print &printer) const {
    size_t written = 0U;
    for (int index = 0; index < 4; ++index) {
        if (index != 0) {
            written += printer.write(static_cast<uint8_t>('.'));
        }
        written += printer.print(static_cast<unsigned int>(octets_[index]), 10);
    }
    return written;
}