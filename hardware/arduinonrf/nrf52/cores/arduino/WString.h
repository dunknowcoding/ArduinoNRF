#pragma once

#include <stddef.h>
#include <stdint.h>

class String {
public:
    String();
    String(char value);
    String(const char *text);
    String(float value, unsigned int decimalPlaces = 2U);
    String(double value, unsigned int decimalPlaces = 2U);
    String(int value);
    String(unsigned int value);
    String(long value);
    String(unsigned long value);
    String(const String &other);
    String(String &&other) noexcept;
    ~String();

    String &operator=(const String &other);
    String &operator=(String &&other) noexcept;
    String &operator=(const char *text);

    unsigned int length() const;
    const char *c_str() const;
    bool isEmpty() const;
    void clear();
    char charAt(unsigned int index) const;
    void setCharAt(unsigned int index, char value);
    bool equals(const String &other) const;
    bool equals(const char *text) const;
    int compareTo(const String &other) const;
    int compareTo(const char *text) const;
    int indexOf(char value) const;
    int indexOf(char value, unsigned int fromIndex) const;
    int indexOf(const String &text) const;
    int indexOf(const String &text, unsigned int fromIndex) const;
    int indexOf(const char *text) const;
    int indexOf(const char *text, unsigned int fromIndex) const;
    int lastIndexOf(char value) const;
    int lastIndexOf(char value, unsigned int fromIndex) const;
    bool equalsIgnoreCase(const String &other) const;
    bool equalsIgnoreCase(const char *text) const;
    bool startsWith(const String &prefix) const;
    bool startsWith(const char *prefix) const;
    bool startsWith(const String &prefix, unsigned int offset) const;
    bool startsWith(const char *prefix, unsigned int offset) const;
    bool endsWith(const String &suffix) const;
    bool endsWith(const char *suffix) const;
    String substring(unsigned int beginIndex) const;
    String substring(unsigned int beginIndex, unsigned int endIndex) const;
    long toInt() const;
    float toFloat() const;
    double toDouble() const;
    bool reserve(unsigned int size);
    void remove(unsigned int index);
    void remove(unsigned int index, unsigned int count);
    void replace(char find, char replaceWith);
    void replace(const String &find, const String &replaceWith);
    void replace(const char *find, const char *replaceWith);
    void trim();
    void toLowerCase();
    void toUpperCase();
    bool concat(const String &other);
    bool concat(const char *text);
    bool concat(char value);
    String &operator+=(const String &other);
    String &operator+=(const char *text);
    String &operator+=(char value);
    char operator[](unsigned int index) const;

    bool operator==(const String &other) const;
    bool operator==(const char *text) const;
    bool operator!=(const String &other) const;
    bool operator!=(const char *text) const;

private:
    bool append(const char *text, unsigned int length);
    void assign(const char *text);
    bool replaceText(const char *find, const char *replaceWith);

    char *buffer_;
    unsigned int length_;
};