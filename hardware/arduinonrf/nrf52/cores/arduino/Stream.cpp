#include "Stream.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "Arduino.h"
#include "WString.h"

namespace {
bool isTerminated(const char *text) {
    return text == nullptr || text[0] == '\0';
}
}

Stream::Stream() : timeout_(1000UL) {
}

void Stream::setTimeout(unsigned long timeout) {
    timeout_ = timeout;
}

unsigned long Stream::getTimeout() const {
    return timeout_;
}

int Stream::timedRead() {
    const unsigned long start = millis();
    do {
        const int value = read();
        if (value >= 0) {
            return value;
        }
        yield();
    } while (millis() - start < timeout_);
    return -1;
}

int Stream::timedPeek() {
    const unsigned long start = millis();
    do {
        const int value = peek();
        if (value >= 0) {
            return value;
        }
        yield();
    } while (millis() - start < timeout_);
    return -1;
}

size_t Stream::readBytes(char *buffer, size_t length) {
    return readBytes(reinterpret_cast<uint8_t *>(buffer), length);
}

size_t Stream::readBytes(uint8_t *buffer, size_t length) {
    if (buffer == nullptr) {
        return 0U;
    }

    size_t count = 0;
    while (count < length) {
        const int value = timedRead();
        if (value < 0) {
            break;
        }
        buffer[count] = static_cast<uint8_t>(value);
        ++count;
    }
    return count;
}

size_t Stream::readBytesUntil(char terminator, char *buffer, size_t length) {
    return readBytesUntil(terminator, reinterpret_cast<uint8_t *>(buffer), length);
}

size_t Stream::readBytesUntil(char terminator, uint8_t *buffer, size_t length) {
    if (buffer == nullptr || length == 0U) {
        return 0U;
    }

    size_t count = 0;
    while (count < length) {
        const int value = timedRead();
        if (value < 0 || value == static_cast<unsigned char>(terminator)) {
            break;
        }
        buffer[count] = static_cast<uint8_t>(value);
        ++count;
    }
    return count;
}

bool Stream::find(char *target) {
    return find(static_cast<const char *>(target));
}

bool Stream::find(const char *target) {
    return findMulti(target, nullptr);
}

bool Stream::find(const String &target) {
    return find(target.c_str());
}

bool Stream::findUntil(char *target, char *terminator) {
    return findUntil(static_cast<const char *>(target), static_cast<const char *>(terminator));
}

bool Stream::findUntil(const char *target, const char *terminator) {
    return findMulti(target, terminator);
}

bool Stream::findUntil(const String &target, const String &terminator) {
    return findUntil(target.c_str(), terminator.c_str());
}

String Stream::readString() {
    String result;
    while (true) {
        const int value = timedRead();
        if (value < 0) {
            break;
        }
        result += static_cast<char>(value);
    }
    return result;
}

String Stream::readStringUntil(char terminator) {
    String result;
    while (true) {
        const int value = timedRead();
        if (value < 0 || value == static_cast<unsigned char>(terminator)) {
            break;
        }
        result += static_cast<char>(value);
    }
    return result;
}

long Stream::parseInt(char skipChar) {
    return parseInt(SKIP_ALL, skipChar);
}

long Stream::parseInt(LookaheadMode lookahead, char skipChar) {
    bool negative = false;
    long value = 0;

    int next = peekNextDigit(lookahead, false, skipChar);
    if (next < 0) {
        return 0L;
    }

    if (next == '-') {
        negative = true;
        (void)read();
    }

    while (true) {
        const int next = timedPeek();
        if (next < 0) {
            break;
        }

        const char current = static_cast<char>(next);
        if (current >= '0' && current <= '9') {
            value = value * 10L + static_cast<long>(current - '0');
            (void)read();
            continue;
        }

        break;
    }

    if (negative) {
        return -value;
    }
    return value;
}

float Stream::parseFloat(char skipChar) {
    return parseFloat(SKIP_ALL, skipChar);
}

float Stream::parseFloat(LookaheadMode lookahead, char skipChar) {
    String token;
    bool decimalSeen = false;

    int next = peekNextDigit(lookahead, true, skipChar);
    if (next < 0) {
        return 0.0f;
    }

    if (next == '-' || next == '+') {
        token += static_cast<char>(read());
    }

    while (true) {
        const int next = timedPeek();
        if (next < 0) {
            break;
        }

        const char current = static_cast<char>(next);
        if (current >= '0' && current <= '9') {
            token += current;
            (void)read();
            continue;
        }

        if (current == '.' && !decimalSeen) {
            decimalSeen = true;
            token += current;
            (void)read();
            continue;
        }

        break;
    }

    if (token.isEmpty()) {
        return 0.0f;
    }
    return static_cast<float>(atof(token.c_str()));
}

int Stream::peekNextDigit(LookaheadMode lookahead, bool detectDecimal, char skipChar) {
    while (true) {
        const int value = timedPeek();
        if (value < 0) {
            return -1;
        }

        const char current = static_cast<char>(value);
        const bool isDigit = current >= '0' && current <= '9';
        if (isDigit || current == '-' || current == '+' || (detectDecimal && current == '.')) {
            return value;
        }

        const bool skipWhitespace = lookahead == SKIP_ALL || lookahead == SKIP_WHITESPACE;
        const bool skipAny = lookahead == SKIP_ALL;
        if (current == skipChar || (skipWhitespace && isspace(static_cast<unsigned char>(current))) || (skipAny && !isDigit && current != '-' && current != '+')) {
            (void)read();
            continue;
        }

        return -1;
    }
}

bool Stream::findMulti(const char *target, const char *terminator) {
    if (isTerminated(target)) {
        return true;
    }

    const size_t targetLength = strlen(target);
    const bool hasTerminator = !isTerminated(terminator);
    size_t terminatorLength = 0U;
    if (hasTerminator) {
        terminatorLength = strlen(terminator);
    }
    size_t targetIndex = 0U;
    size_t terminatorIndex = 0U;

    while (true) {
        const int value = timedRead();
        if (value < 0) {
            return false;
        }

        const char current = static_cast<char>(value);
        if (current == target[targetIndex]) {
            ++targetIndex;
            if (targetIndex == targetLength) {
                return true;
            }
        } else {
            if (current == target[0]) {
                targetIndex = 1U;
            } else {
                targetIndex = 0U;
            }
        }

        if (hasTerminator) {
            if (current == terminator[terminatorIndex]) {
                ++terminatorIndex;
                if (terminatorIndex == terminatorLength) {
                    return false;
                }
            } else {
                if (current == terminator[0]) {
                    terminatorIndex = 1U;
                } else {
                    terminatorIndex = 0U;
                }
            }
        }
    }
}