#pragma once

#include <stddef.h>
#include <stdint.h>

#include "IPAddress.h"
#include "Stream.h"

class UDP : public Stream {
public:
    using Print::write;
    using Stream::read;

    virtual ~UDP() = default;
    virtual uint8_t begin(uint16_t port) = 0;
    virtual int beginPacket(IPAddress ip, uint16_t port) = 0;
    virtual int beginPacket(const char *host, uint16_t port) = 0;
    virtual int endPacket() = 0;
    virtual int parsePacket() = 0;
    virtual IPAddress remoteIP() = 0;
    virtual uint16_t remotePort() = 0;
    virtual void stop() = 0;
    virtual uint16_t localPort() {
        return 0U;
    }
    virtual int beginMulticast(IPAddress ip, uint16_t port) {
        (void)ip;
        return begin(port);
    }
    virtual int beginMulticastPacket() {
        return beginPacket(remoteIP(), remotePort());
    }
    virtual IPAddress destinationIP() {
        return remoteIP();
    }
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
    virtual int read(unsigned char *buffer, size_t len) {
        if (buffer == nullptr) {
            return 0;
        }

        size_t count = 0;
        while (count < len) {
            const int value = read();
            if (value < 0) {
                break;
            }
            buffer[count] = static_cast<unsigned char>(value);
            ++count;
        }
        return static_cast<int>(count);
    }
    virtual int read(char *buffer, size_t len) {
        return read(reinterpret_cast<unsigned char *>(buffer), len);
    }
};