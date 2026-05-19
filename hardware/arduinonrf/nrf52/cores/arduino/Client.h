#pragma once

#include <stddef.h>
#include <stdint.h>

#include "IPAddress.h"
#include "Stream.h"

class Client : public Stream {
public:
    using Print::write;
    using Stream::read;

    virtual ~Client() = default;
    virtual int connect(IPAddress ip, uint16_t port) = 0;
    virtual int connect(const char *host, uint16_t port) = 0;
    virtual int availableForWrite() {
        return 0;
    }
    virtual size_t write(const uint8_t *buffer, size_t size) {
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
    virtual int read(uint8_t *buffer, size_t size) {
        if (buffer == nullptr) {
            return 0;
        }

        size_t count = 0;
        while (count < size) {
            const int value = read();
            if (value < 0) {
                break;
            }
            buffer[count] = static_cast<uint8_t>(value);
            ++count;
        }
        return static_cast<int>(count);
    }
    virtual int read(char *buffer, size_t size) {
        return read(reinterpret_cast<uint8_t *>(buffer), size);
    }
    virtual uint8_t status() {
        if (connected() != 0U) {
            return 1U;
        }
        return 0U;
    }
    virtual uint8_t connected() = 0;
    virtual void stop() = 0;
    virtual operator bool() = 0;
};