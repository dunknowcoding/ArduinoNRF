#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Printable.h"

class String;
class __FlashStringHelper;

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t value) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size);
    size_t write(const char *buffer, size_t size);
    size_t write(const __FlashStringHelper *text);
    virtual int availableForWrite() {
        return 0;
    }
    size_t write(const char *text);
    size_t print(const char *text);
    size_t print(const __FlashStringHelper *text);
    size_t print(const String &text);
    size_t print(const Printable &printable);
    size_t print(bool value);
    size_t print(char value);
    size_t print(unsigned char value, int base = 10);
    size_t print(double value, int digits = 2);
    size_t print(int value, int base = 10);
    size_t print(unsigned int value, int base = 10);
    size_t print(long value, int base = 10);
    size_t print(unsigned long value, int base = 10);
    size_t println(void);
    size_t println(bool value);
    size_t println(char value);
    size_t println(const char *text);
    size_t println(const __FlashStringHelper *text);
    size_t println(const String &text);
    size_t println(double value, int digits = 2);
    size_t println(int value, int base = 10);
    size_t println(unsigned char value, int base = 10);
    size_t println(unsigned int value, int base = 10);
    size_t println(long value, int base = 10);
    size_t println(unsigned long value, int base = 10);
};
