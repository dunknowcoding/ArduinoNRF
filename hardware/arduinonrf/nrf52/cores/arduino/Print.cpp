#include "Print.h"

#include <stdio.h>

#include "WString.h"

namespace {
size_t writeUnsigned(Print &printer, unsigned long value, int base) {
    char buffer[33];
    char *cursor = &buffer[32];
    const char *digits = "0123456789ABCDEF";
    *cursor = '\0';

    if (base < 2 || base > 16) {
        base = 10;
    }

    do {
        --cursor;
        *cursor = digits[value % static_cast<unsigned long>(base)];
        value /= static_cast<unsigned long>(base);
    } while (value != 0);

    size_t written = 0;
    while (*cursor != '\0') {
        written += printer.write(static_cast<uint8_t>(*cursor));
        ++cursor;
    }
    return written;
}

size_t writeSigned(Print &printer, long value, int base) {
    if (base == 10 && value < 0) {
        return printer.write(static_cast<uint8_t>('-')) + writeUnsigned(printer, static_cast<unsigned long>(-value), base);
    }
    return writeUnsigned(printer, static_cast<unsigned long>(value), base);
}
}

size_t Print::write(const uint8_t *buffer, size_t size) {
    if (buffer == nullptr) {
        return 0U;
    }

    size_t written = 0;
    while (written < size) {
        if (write(buffer[written]) != 1U) {
            break;
        }
        ++written;
    }
    return written;
}

size_t Print::write(const char *buffer, size_t size) {
    return write(reinterpret_cast<const uint8_t *>(buffer), size);
}

size_t Print::write(const __FlashStringHelper *text) {
    return write(reinterpret_cast<const char *>(text));
}

size_t Print::write(const char *text) {
    if (text == nullptr) {
        return 0;
    }

    size_t written = 0;
    while (*text != '\0') {
        written += write(static_cast<uint8_t>(*text));
        ++text;
    }
    return written;
}

size_t Print::print(const char *text) {
    return write(text);
}

size_t Print::print(const __FlashStringHelper *text) {
    return write(text);
}

size_t Print::print(const String &text) {
    return write(text.c_str());
}

size_t Print::print(const Printable &printable) {
    return printable.printTo(*this);
}

size_t Print::print(bool value) {
    if (value) {
        return write("true");
    }
    return write("false");
}

size_t Print::print(char value) {
    return write(static_cast<uint8_t>(value));
}

size_t Print::print(unsigned char value, int base) {
    return print(static_cast<unsigned int>(value), base);
}

size_t Print::print(double value, int digits) {
    if (digits < 0) {
        digits = 2;
    }
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "%.*f", digits, value);
    return write(buffer);
}

size_t Print::print(int value, int base) {
    return writeSigned(*this, static_cast<long>(value), base);
}

size_t Print::print(unsigned int value, int base) {
    return writeUnsigned(*this, static_cast<unsigned long>(value), base);
}

size_t Print::print(long value, int base) {
    return writeSigned(*this, value, base);
}

size_t Print::print(unsigned long value, int base) {
    return writeUnsigned(*this, value, base);
}

size_t Print::println(void) {
    return write("\r\n");
}

size_t Print::println(bool value) {
    return print(value) + println();
}

size_t Print::println(char value) {
    return print(value) + println();
}

size_t Print::println(const char *text) {
    return print(text) + println();
}

size_t Print::println(const __FlashStringHelper *text) {
    return print(text) + println();
}

size_t Print::println(const String &text) {
    return print(text) + println();
}

size_t Print::println(double value, int digits) {
    return print(value, digits) + println();
}

size_t Print::println(int value, int base) {
    return print(value, base) + println();
}

size_t Print::println(unsigned char value, int base) {
    return print(value, base) + println();
}

size_t Print::println(unsigned int value, int base) {
    return print(value, base) + println();
}

size_t Print::println(long value, int base) {
    return print(value, base) + println();
}

size_t Print::println(unsigned long value, int base) {
    return print(value, base) + println();
}
