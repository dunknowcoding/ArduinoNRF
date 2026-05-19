#include <NrfBoard.h>
#include <stdio.h>
#include <string.h>

#include "WiFi.h"

namespace {
NrfWiFiDriverInfo driverInfoFor(NrfWiFiChipset chipset, NrfWiFiTransport transport) {
    NrfWiFiTransport resolvedTransport = transport;
    switch (chipset) {
        case NRF_WIFI_CHIPSET_NRF7002:
            if (resolvedTransport == NRF_WIFI_TRANSPORT_NONE) {
                resolvedTransport = NRF_WIFI_TRANSPORT_SPI;
            }
            return {chipset, resolvedTransport, "Nordic nRF7002 companion", true};
        case NRF_WIFI_CHIPSET_ESP8266_AT:
            if (resolvedTransport == NRF_WIFI_TRANSPORT_NONE) {
                resolvedTransport = NRF_WIFI_TRANSPORT_UART;
            }
            return {chipset, resolvedTransport, "ESP8266 AT firmware", true};
        case NRF_WIFI_CHIPSET_ESP32_AT:
            if (resolvedTransport == NRF_WIFI_TRANSPORT_NONE) {
                resolvedTransport = NRF_WIFI_TRANSPORT_UART;
            }
            return {chipset, resolvedTransport, "ESP32 AT firmware", true};
        case NRF_WIFI_CHIPSET_NINA_W10X:
            if (resolvedTransport == NRF_WIFI_TRANSPORT_NONE) {
                resolvedTransport = NRF_WIFI_TRANSPORT_UART;
            }
            return {chipset, resolvedTransport, "u-blox NINA-W10x", true};
        case NRF_WIFI_CHIPSET_WINC1500:
            if (resolvedTransport == NRF_WIFI_TRANSPORT_NONE) {
                resolvedTransport = NRF_WIFI_TRANSPORT_SPI;
            }
            return {chipset, resolvedTransport, "Microchip WINC1500", true};
        case NRF_WIFI_CHIPSET_WINC3400:
            if (resolvedTransport == NRF_WIFI_TRANSPORT_NONE) {
                resolvedTransport = NRF_WIFI_TRANSPORT_SPI;
            }
            return {chipset, resolvedTransport, "Microchip WINC3400", true};
        case NRF_WIFI_CHIPSET_CYW43XX:
            if (resolvedTransport == NRF_WIFI_TRANSPORT_NONE) {
                resolvedTransport = NRF_WIFI_TRANSPORT_SDIO;
            }
            return {chipset, resolvedTransport, "Infineon CYW43xx", false};
        case NRF_WIFI_CHIPSET_RTL8720:
            if (resolvedTransport == NRF_WIFI_TRANSPORT_NONE) {
                resolvedTransport = NRF_WIFI_TRANSPORT_UART;
            }
            return {chipset, resolvedTransport, "Realtek RTL8720/Ameba", false};
        case NRF_WIFI_CHIPSET_EXTERNAL_UNKNOWN:
            if (resolvedTransport == NRF_WIFI_TRANSPORT_NONE) {
                resolvedTransport = NRF_WIFI_TRANSPORT_UART;
            }
            return {chipset, resolvedTransport, "External WiFi coprocessor", true};
        case NRF_WIFI_CHIPSET_NONE:
        default:
            return {NRF_WIFI_CHIPSET_NONE, NRF_WIFI_TRANSPORT_NONE, "No WiFi coprocessor", false};
    }
}
}

WiFiClass WiFi;

int WiFiClient::connect(IPAddress ip, uint16_t port) {
    remoteIp_ = ip;
    remotePort_ = port;
    localPort_ = 0U;
    connected_ = WiFi.socketApiImplemented() && WiFi.status() == WL_CONNECTED;
    if (connected_) {
        return 1;
    }
    return 0;
}

int WiFiClient::connect(const char *host, uint16_t port) {
    IPAddress resolved;
    if (host != nullptr) {
        (void)WiFi.hostByName(host, resolved);
    }
    return connect(resolved, port);
}

int WiFiClient::available(void) {
    return 0;
}

int WiFiClient::read(void) {
    return -1;
}

int WiFiClient::peek(void) {
    return -1;
}

void WiFiClient::flush(void) {
}

size_t WiFiClient::write(uint8_t value) {
    (void)value;
    return 0U;
}

int WiFiClient::availableForWrite() {
    if (connected_) {
        return 64;
    }
    return 0;
}

uint8_t WiFiClient::status() {
    if (connected_) {
        return WL_CONNECTED;
    }
    return WiFi.status();
}

uint8_t WiFiClient::connected() {
    if (connected_) {
        return 1U;
    }
    return 0U;
}

void WiFiClient::stop() {
    connected_ = false;
    remoteIp_ = IPAddress();
    remotePort_ = 0U;
    localPort_ = 0U;
}

IPAddress WiFiClient::remoteIP() const {
    return remoteIp_;
}

uint16_t WiFiClient::remotePort() const {
    return remotePort_;
}

uint16_t WiFiClient::localPort() const {
    return localPort_;
}

WiFiClient::operator bool() {
    return connected_;
}

int WiFiClass::begin(const char *ssid, const char *password) {
    (void)password;
    ssid_ = "";
    if (ssid != nullptr) {
        ssid_ = ssid;
    }
    if (chipset_ == NRF_WIFI_CHIPSET_NONE && hardwarePresent()) {
        chipset_ = NRF_WIFI_CHIPSET_EXTERNAL_UNKNOWN;
        transport_ = NRF_WIFI_TRANSPORT_UART;
    }
    if (!hardwarePresent() || !supported()) {
        status_ = WL_NO_MODULE;
    } else if (!transportConfigured()) {
        status_ = WL_NO_SHIELD;
    } else {
        status_ = WL_CONNECT_FAILED;
    }
    return status_;
}

void WiFiClass::end() {
    status_ = WL_DISCONNECTED;
    ssid_ = "";
}

int WiFiClass::disconnect() {
    end();
    return WL_DISCONNECTED;
}

void WiFiClass::setChipset(NrfWiFiChipset chipset) {
    chipset_ = chipset;
}

void WiFiClass::setTransport(NrfWiFiTransport transport) {
    transport_ = transport;
}

uint8_t WiFiClass::status() const {
    return status_;
}

const char *WiFiClass::SSID() const {
    return ssid_;
}

int32_t WiFiClass::RSSI() const {
    if (status_ == WL_CONNECTED) {
        return -48;
    }
    return 0;
}

IPAddress WiFiClass::localIP() const {
    if (status_ == WL_CONNECTED) {
        return IPAddress(192, 168, 4, 1);
    }
    return IPAddress();
}

IPAddress WiFiClass::gatewayIP() const {
    if (status_ == WL_CONNECTED) {
        return IPAddress(192, 168, 4, 254);
    }
    return IPAddress();
}

IPAddress WiFiClass::subnetMask() const {
    if (status_ == WL_CONNECTED) {
        return IPAddress(255, 255, 255, 0);
    }
    return IPAddress();
}

IPAddress WiFiClass::dnsIP() const {
    if (status_ == WL_CONNECTED) {
        return IPAddress(192, 168, 4, 254);
    }
    return IPAddress();
}

uint8_t *WiFiClass::macAddress(uint8_t *buffer) {
    if (buffer == nullptr) {
        return nullptr;
    }

    buffer[0] = 0x02;
    buffer[1] = 0x4E;
    buffer[2] = 0x52;
    buffer[3] = 0x46;
    buffer[4] = static_cast<uint8_t>(chipset_);
    buffer[5] = 0x00;
    if (usingExternalCoprocessor()) {
        buffer[5] = 0x40;
    }
    return buffer;
}

uint8_t *WiFiClass::BSSID(uint8_t *buffer) {
    return macAddress(buffer);
}

const char *WiFiClass::BSSIDstr(char *buffer) {
    if (buffer == nullptr) {
        return nullptr;
    }

    uint8_t bytes[6] = {0};
    (void)BSSID(bytes);
    snprintf(buffer, 18, "%02X:%02X:%02X:%02X:%02X:%02X", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
    return buffer;
}

int WiFiClass::hostByName(const char *hostname, IPAddress &result) {
    if (hostname == nullptr || hostname[0] == '\0') {
        result = IPAddress();
        return 0;
    }

    if (result.fromString(hostname)) {
        return 1;
    }

    result = IPAddress();
    return 0;
}

void WiFiClass::setHostname(const char *hostname) {
    const char *value = "";
    if (hostname != nullptr) {
        value = hostname;
    }
    strncpy(hostname_, value, sizeof(hostname_) - 1U);
    hostname_[sizeof(hostname_) - 1U] = '\0';
}

const char *WiFiClass::hostname() const {
    return hostname_;
}

void WiFiClass::setAutoReconnect(bool enabled) {
    autoReconnect_ = enabled;
}

bool WiFiClass::autoReconnect() const {
    return autoReconnect_;
}

NrfWiFiDriverInfo WiFiClass::driverInfo() const {
    return driverInfoFor(chipset_, transport_);
}

bool WiFiClass::hardwarePresent() const {
    return nrfBoardHasWiFiCoprocessor();
}

bool WiFiClass::usingExternalCoprocessor() const {
    return hardwarePresent();
}

bool WiFiClass::supported() const {
    return hardwarePresent() && driverInfo().supported;
}

bool WiFiClass::transportConfigured() const {
    return transport_ != NRF_WIFI_TRANSPORT_NONE || driverInfo().transport != NRF_WIFI_TRANSPORT_NONE;
}

bool WiFiClass::facadeOnly() const {
    return supported();
}

bool WiFiClass::linkLayerImplemented() const {
    return false;
}

bool WiFiClass::socketApiImplemented() const {
    return false;
}

const char *WiFiClass::statusMessage() const {
    switch (status_) {
        case WL_CONNECTED:
            return "connected";
        case WL_CONNECT_FAILED:
            if (supported()) {
                return "hardware recognized, facade only, link not implemented";
            }
            return "connect failed";
        case WL_NO_SHIELD:
            return "transport not configured";
        case WL_NO_MODULE:
            return "no supported WiFi module";
        case WL_DISCONNECTED:
            return "disconnected";
        default:
            return "idle";
    }
}

const char *WiFiClass::firmwareVersion() const {
    if (usingExternalCoprocessor()) {
        return "arduinonrf-external-wifi-0.2";
    }
    return "unsupported";
}
