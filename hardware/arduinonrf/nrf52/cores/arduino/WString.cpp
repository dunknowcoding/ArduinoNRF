#include "WString.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
char lowerChar(char value) {
    return static_cast<char>(tolower(static_cast<unsigned char>(value)));
}

char *duplicateText(const char *text, unsigned int &lengthOut) {
    if (text == nullptr) {
        lengthOut = 0U;
        char *empty = static_cast<char *>(malloc(1U));
        if (empty != nullptr) {
            empty[0] = '\0';
        }
        return empty;
    }

    lengthOut = static_cast<unsigned int>(strlen(text));
    char *copy = static_cast<char *>(malloc(static_cast<size_t>(lengthOut) + 1U));
    if (copy != nullptr) {
        memcpy(copy, text, static_cast<size_t>(lengthOut) + 1U);
    } else {
        lengthOut = 0U;
    }
    return copy;
}

char *allocateText(unsigned int length) {
    char *buffer = static_cast<char *>(malloc(static_cast<size_t>(length) + 1U));
    if (buffer != nullptr) {
        buffer[length] = '\0';
    }
    return buffer;
}
}

String::String()
    : buffer_(nullptr), length_(0U) {
    assign("");
}

String::String(char value)
    : buffer_(nullptr), length_(0U) {
    char text[2] = {value, '\0'};
    assign(text);
}

String::String(const char *text)
    : buffer_(nullptr), length_(0U) {
    assign(text);
}

String::String(float value, unsigned int decimalPlaces)
    : buffer_(nullptr), length_(0U) {
    char text[32];
    snprintf(text, sizeof(text), "%.*f", static_cast<int>(decimalPlaces), static_cast<double>(value));
    assign(text);
}

String::String(double value, unsigned int decimalPlaces)
    : buffer_(nullptr), length_(0U) {
    char text[32];
    snprintf(text, sizeof(text), "%.*f", static_cast<int>(decimalPlaces), value);
    assign(text);
}

String::String(int value)
    : buffer_(nullptr), length_(0U) {
    char text[16];
    snprintf(text, sizeof(text), "%d", value);
    assign(text);
}

String::String(unsigned int value)
    : buffer_(nullptr), length_(0U) {
    char text[16];
    snprintf(text, sizeof(text), "%u", value);
    assign(text);
}

String::String(long value)
    : buffer_(nullptr), length_(0U) {
    char text[24];
    snprintf(text, sizeof(text), "%ld", value);
    assign(text);
}

String::String(unsigned long value)
    : buffer_(nullptr), length_(0U) {
    char text[24];
    snprintf(text, sizeof(text), "%lu", value);
    assign(text);
}

String::String(const String &other)
    : buffer_(nullptr), length_(0U) {
    assign(other.c_str());
}

String::String(String &&other) noexcept
    : buffer_(other.buffer_), length_(other.length_) {
    other.buffer_ = nullptr;
    other.length_ = 0U;
}

String::~String() {
    free(buffer_);
}

String &String::operator=(const String &other) {
    if (this != &other) {
        assign(other.c_str());
    }
    return *this;
}

String &String::operator=(String &&other) noexcept {
    if (this != &other) {
        free(buffer_);
        buffer_ = other.buffer_;
        length_ = other.length_;
        other.buffer_ = nullptr;
        other.length_ = 0U;
    }
    return *this;
}

String &String::operator=(const char *text) {
    assign(text);
    return *this;
}

unsigned int String::length() const {
    return length_;
}

const char *String::c_str() const {
    if (buffer_ == nullptr) {
        return "";
    }
    return buffer_;
}

bool String::isEmpty() const {
    return length_ == 0U;
}

void String::clear() {
    assign("");
}

char String::charAt(unsigned int index) const {
    if (index < length_) {
        return buffer_[index];
    }
    return '\0';
}

void String::setCharAt(unsigned int index, char value) {
    if (index < length_) {
        buffer_[index] = value;
    }
}

bool String::equals(const String &other) const {
    return equals(other.c_str());
}

bool String::equals(const char *text) const {
    const char *value = "";
    if (text != nullptr) {
        value = text;
    }
    return strcmp(c_str(), value) == 0;
}

int String::compareTo(const String &other) const {
    return compareTo(other.c_str());
}

int String::compareTo(const char *text) const {
    const char *value = "";
    if (text != nullptr) {
        value = text;
    }
    return strcmp(c_str(), value);
}

int String::indexOf(char value) const {
    return indexOf(value, 0U);
}

int String::indexOf(char value, unsigned int fromIndex) const {
    if (fromIndex >= length_) {
        return -1;
    }
    for (unsigned int index = fromIndex; index < length_; ++index) {
        if (buffer_[index] == value) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int String::indexOf(const String &text) const {
    return indexOf(text.c_str(), 0U);
}

int String::indexOf(const String &text, unsigned int fromIndex) const {
    return indexOf(text.c_str(), fromIndex);
}

int String::indexOf(const char *text) const {
    return indexOf(text, 0U);
}

int String::indexOf(const char *text, unsigned int fromIndex) const {
    const char *candidate = "";
    if (text != nullptr) {
        candidate = text;
    }
    if (candidate[0] == '\0') {
        if (fromIndex <= length_) {
            return static_cast<int>(fromIndex);
        }
        return -1;
    }
    if (fromIndex >= length_) {
        return -1;
    }

    const char *match = strstr(c_str() + fromIndex, candidate);
    if (match == nullptr) {
        return -1;
    }
    return static_cast<int>(match - c_str());
}

int String::lastIndexOf(char value) const {
    unsigned int fromIndex = 0U;
    if (length_ != 0U) {
        fromIndex = length_ - 1U;
    }
    return lastIndexOf(value, fromIndex);
}

int String::lastIndexOf(char value, unsigned int fromIndex) const {
    if (length_ == 0U) {
        return -1;
    }

    unsigned int index = fromIndex;
    if (fromIndex >= length_) {
        index = length_ - 1U;
    }
    do {
        if (buffer_[index] == value) {
            return static_cast<int>(index);
        }
        if (index == 0U) {
            break;
        }
        --index;
    } while (true);

    return -1;
}

bool String::equalsIgnoreCase(const String &other) const {
    return equalsIgnoreCase(other.c_str());
}

bool String::equalsIgnoreCase(const char *text) const {
    const char *other = "";
    if (text != nullptr) {
        other = text;
    }
    if (strlen(other) != length_) {
        return false;
    }

    for (unsigned int index = 0; index < length_; ++index) {
        if (lowerChar(buffer_[index]) != lowerChar(other[index])) {
            return false;
        }
    }
    return true;
}

bool String::startsWith(const String &prefix) const {
    return startsWith(prefix.c_str());
}

bool String::startsWith(const char *prefix) const {
    return startsWith(prefix, 0U);
}

bool String::startsWith(const String &prefix, unsigned int offset) const {
    return startsWith(prefix.c_str(), offset);
}

bool String::startsWith(const char *prefix, unsigned int offset) const {
    const char *candidate = "";
    if (prefix != nullptr) {
        candidate = prefix;
    }
    const size_t prefixLength = strlen(candidate);
    return offset <= length_ && prefixLength <= (length_ - offset) && strncmp(c_str() + offset, candidate, prefixLength) == 0;
}

bool String::endsWith(const String &suffix) const {
    return endsWith(suffix.c_str());
}

bool String::endsWith(const char *suffix) const {
    const char *candidate = "";
    if (suffix != nullptr) {
        candidate = suffix;
    }
    const size_t suffixLength = strlen(candidate);
    return suffixLength <= length_ && strcmp(c_str() + (length_ - suffixLength), candidate) == 0;
}

String String::substring(unsigned int beginIndex) const {
    return substring(beginIndex, length_);
}

String String::substring(unsigned int beginIndex, unsigned int endIndex) const {
    if (beginIndex >= length_ || beginIndex >= endIndex) {
        return String("");
    }
    if (endIndex > length_) {
        endIndex = length_;
    }
    const unsigned int resultLength = endIndex - beginIndex;
    char *text = allocateText(resultLength);
    if (text == nullptr) {
        return String("");
    }
    memcpy(text, c_str() + beginIndex, resultLength);
    text[resultLength] = '\0';
    String result(text);
    free(text);
    return result;
}

long String::toInt() const {
    return strtol(c_str(), nullptr, 10);
}

float String::toFloat() const {
    return static_cast<float>(atof(c_str()));
}

double String::toDouble() const {
    return atof(c_str());
}

bool String::reserve(unsigned int size) {
    if (size <= length_) {
        return true;
    }
    char *nextBuffer = allocateText(size);
    if (nextBuffer == nullptr) {
        return false;
    }
    memcpy(nextBuffer, c_str(), length_ + 1U);
    free(buffer_);
    buffer_ = nextBuffer;
    return true;
}

void String::remove(unsigned int index) {
    unsigned int count = 0U;
    if (index < length_) {
        count = length_ - index;
    }
    remove(index, count);
}

void String::remove(unsigned int index, unsigned int count) {
    if (index >= length_ || count == 0U) {
        return;
    }

    if (index + count >= length_) {
        buffer_[index] = '\0';
        length_ = index;
        return;
    }

    memmove(buffer_ + index, buffer_ + index + count, (length_ - index - count) + 1U);
    length_ -= count;
}

void String::replace(char find, char replaceWith) {
    for (unsigned int index = 0; index < length_; ++index) {
        if (buffer_[index] == find) {
            buffer_[index] = replaceWith;
        }
    }
}

void String::replace(const String &find, const String &replaceWith) {
    (void)replaceText(find.c_str(), replaceWith.c_str());
}

void String::replace(const char *find, const char *replaceWith) {
    const char *findText = "";
    if (find != nullptr) {
        findText = find;
    }
    const char *replaceTextValue = "";
    if (replaceWith != nullptr) {
        replaceTextValue = replaceWith;
    }
    (void)replaceText(findText, replaceTextValue);
}

void String::trim() {
    if (length_ == 0U) {
        return;
    }

    unsigned int start = 0U;
    while (start < length_ && isspace(static_cast<unsigned char>(buffer_[start]))) {
        ++start;
    }

    unsigned int end = length_;
    while (end > start && isspace(static_cast<unsigned char>(buffer_[end - 1U]))) {
        --end;
    }

    if (start > 0U) {
        memmove(buffer_, buffer_ + start, end - start);
    }
    length_ = end - start;
    buffer_[length_] = '\0';
}

void String::toLowerCase() {
    for (unsigned int index = 0; index < length_; ++index) {
        buffer_[index] = lowerChar(buffer_[index]);
    }
}

void String::toUpperCase() {
    for (unsigned int index = 0; index < length_; ++index) {
        buffer_[index] = static_cast<char>(toupper(static_cast<unsigned char>(buffer_[index])));
    }
}

bool String::concat(const String &other) {
    return append(other.c_str(), other.length_);
}

bool String::concat(const char *text) {
    const char *value = "";
    if (text != nullptr) {
        value = text;
    }
    return append(value, static_cast<unsigned int>(strlen(value)));
}

bool String::concat(char value) {
    return append(&value, 1U);
}

String &String::operator+=(const String &other) {
    (void)concat(other);
    return *this;
}

String &String::operator+=(const char *text) {
    (void)concat(text);
    return *this;
}

String &String::operator+=(char value) {
    (void)concat(value);
    return *this;
}

char String::operator[](unsigned int index) const {
    return charAt(index);
}

bool String::operator==(const String &other) const {
    return equals(other);
}

bool String::operator==(const char *text) const {
    return equals(text);
}

bool String::operator!=(const String &other) const {
    return !(*this == other);
}

bool String::operator!=(const char *text) const {
    return !(*this == text);
}

bool String::append(const char *text, unsigned int length) {
    if (text == nullptr || length == 0U) {
        return true;
    }

    const unsigned int nextLength = length_ + length;
    char *nextBuffer = allocateText(nextLength);
    if (nextBuffer == nullptr) {
        return false;
    }
    memcpy(nextBuffer, c_str(), length_);
    memcpy(nextBuffer + length_, text, length);
    nextBuffer[nextLength] = '\0';
    free(buffer_);
    buffer_ = nextBuffer;
    length_ = nextLength;
    return true;
}

void String::assign(const char *text) {
    unsigned int nextLength = 0U;
    char *nextBuffer = duplicateText(text, nextLength);
    if (nextBuffer == nullptr) {
        return;
    }

    free(buffer_);
    buffer_ = nextBuffer;
    length_ = nextLength;
}

bool String::replaceText(const char *find, const char *replaceWith) {
    if (find == nullptr || replaceWith == nullptr || find[0] == '\0') {
        return true;
    }

    const size_t findLength = strlen(find);
    const size_t replaceLength = strlen(replaceWith);
    const char *cursor = c_str();
    size_t matchCount = 0U;

    while ((cursor = strstr(cursor, find)) != nullptr) {
        ++matchCount;
        cursor += findLength;
    }

    if (matchCount == 0U) {
        return true;
    }

    const long long delta = static_cast<long long>(replaceLength) - static_cast<long long>(findLength);
    const size_t nextLength = static_cast<size_t>(static_cast<long long>(length_) + static_cast<long long>(matchCount) * delta);
    char *nextBuffer = allocateText(static_cast<unsigned int>(nextLength));
    if (nextBuffer == nullptr) {
        return false;
    }

    const char *source = c_str();
    char *destination = nextBuffer;
    while (*source != '\0') {
        const char *match = strstr(source, find);
        if (match == nullptr) {
            strcpy(destination, source);
            break;
        }

        const size_t prefixLength = static_cast<size_t>(match - source);
        memcpy(destination, source, prefixLength);
        destination += prefixLength;
        memcpy(destination, replaceWith, replaceLength);
        destination += replaceLength;
        source = match + findLength;
    }

    free(buffer_);
    buffer_ = nextBuffer;
    length_ = static_cast<unsigned int>(nextLength);
    return true;
}