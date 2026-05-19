#pragma once

#include <Client.h>
#include <IPAddress.h>
#include <stdint.h>

static constexpr uint8_t WL_NO_SHIELD = 255;
static constexpr uint8_t WL_IDLE_STATUS = 0;
static constexpr uint8_t WL_NO_SSID_AVAIL = 1;
static constexpr uint8_t WL_SCAN_COMPLETED = 2;
static constexpr uint8_t WL_CONNECTED = 3;
static constexpr uint8_t WL_CONNECT_FAILED = 4;
static constexpr uint8_t WL_CONNECTION_LOST = 5;
static constexpr uint8_t WL_DISCONNECTED = 6;
static constexpr uint8_t WL_NO_MODULE = 7;

enum NrfWiFiChipset : uint8_t {
    NRF_WIFI_CHIPSET_NONE = 0,
    NRF_WIFI_CHIPSET_EXTERNAL_UNKNOWN = 1,
    NRF_WIFI_CHIPSET_NRF7002 = 2,
    NRF_WIFI_CHIPSET_ESP8266_AT = 3,
    NRF_WIFI_CHIPSET_ESP32_AT = 4,
    NRF_WIFI_CHIPSET_NINA_W10X = 5,
    NRF_WIFI_CHIPSET_WINC1500 = 6,
    NRF_WIFI_CHIPSET_WINC3400 = 7,
    NRF_WIFI_CHIPSET_CYW43XX = 8,
    NRF_WIFI_CHIPSET_RTL8720 = 9
};

enum NrfWiFiTransport : uint8_t {
    NRF_WIFI_TRANSPORT_NONE = 0,
    NRF_WIFI_TRANSPORT_UART = 1,
    NRF_WIFI_TRANSPORT_SPI = 2,
    NRF_WIFI_TRANSPORT_SDIO = 3,
    NRF_WIFI_TRANSPORT_HOSTED = 4
};

struct NrfWiFiDriverInfo {
    NrfWiFiChipset chipset;
    NrfWiFiTransport transport;
    const char *name;
    bool supported;
};

class WiFiClient : public Client {
public:
    using Client::read;
    using Client::write;

    int connect(IPAddress ip, uint16_t port) override;
    int connect(const char *host, uint16_t port) override;
    int available(void) override;
    int read(void) override;
    int peek(void) override;
    void flush(void) override;
    size_t write(uint8_t value) override;
    int availableForWrite() override;
    uint8_t status() override;
    uint8_t connected() override;
    void stop() override;
    IPAddress remoteIP() const;
    uint16_t remotePort() const;
    uint16_t localPort() const;
    operator bool() override;

private:
    IPAddress remoteIp_;
    uint16_t remotePort_ = 0U;
    uint16_t localPort_ = 0U;
    bool connected_ = false;
};

class WiFiClass {
public:
    int begin(const char *ssid, const char *password = nullptr);
    void end();
    int disconnect();
    void setChipset(NrfWiFiChipset chipset);
    void setTransport(NrfWiFiTransport transport);
    uint8_t status() const;
    const char *SSID() const;
    int32_t RSSI() const;
    IPAddress localIP() const;
    IPAddress gatewayIP() const;
    IPAddress subnetMask() const;
    IPAddress dnsIP() const;
    uint8_t *macAddress(uint8_t *buffer);
    uint8_t *BSSID(uint8_t *buffer);
    const char *BSSIDstr(char *buffer);
    int hostByName(const char *hostname, IPAddress &result);
    void setHostname(const char *hostname);
    const char *hostname() const;
    void setAutoReconnect(bool enabled);
    bool autoReconnect() const;
    NrfWiFiDriverInfo driverInfo() const;
    bool hardwarePresent() const;
    bool usingExternalCoprocessor() const;
    bool supported() const;
    bool transportConfigured() const;
    bool facadeOnly() const;
    bool linkLayerImplemented() const;
    bool socketApiImplemented() const;
    const char *statusMessage() const;
    const char *firmwareVersion() const;

private:
    uint8_t status_ = WL_DISCONNECTED;
    const char *ssid_ = "";
    NrfWiFiChipset chipset_ = NRF_WIFI_CHIPSET_NONE;
    NrfWiFiTransport transport_ = NRF_WIFI_TRANSPORT_NONE;
    char hostname_[33] = "arduinonrf-nrf52";
    bool autoReconnect_ = false;
};

extern WiFiClass WiFi;
