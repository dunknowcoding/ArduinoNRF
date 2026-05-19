#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Print.h"

class String;

class Stream : public Print {
public:
    using Print::write;

    enum LookaheadMode : uint8_t {
        SKIP_ALL,
        SKIP_NONE,
        SKIP_WHITESPACE
    };

    Stream();
    virtual int available(void) = 0;
    virtual int read(void) = 0;
    virtual int peek(void) = 0;
    virtual void flush(void) = 0;

    void setTimeout(unsigned long timeout);
    unsigned long getTimeout() const;
    size_t readBytes(char *buffer, size_t length);
    size_t readBytes(uint8_t *buffer, size_t length);
    size_t readBytesUntil(char terminator, char *buffer, size_t length);
    size_t readBytesUntil(char terminator, uint8_t *buffer, size_t length);
    bool find(char *target);
    bool find(const char *target);
    bool find(const String &target);
    bool findUntil(char *target, char *terminator);
    bool findUntil(const char *target, const char *terminator);
    bool findUntil(const String &target, const String &terminator);
    String readString();
    String readStringUntil(char terminator);
    long parseInt(char skipChar = '\0');
    float parseFloat(char skipChar = '\0');
    long parseInt(LookaheadMode lookahead, char skipChar = '\0');
    float parseFloat(LookaheadMode lookahead, char skipChar = '\0');

protected:
    int timedRead();
    int timedPeek();
    int peekNextDigit(LookaheadMode lookahead, bool detectDecimal, char skipChar);

private:
    bool findMulti(const char *target, const char *terminator);
    unsigned long timeout_;
};
