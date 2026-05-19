// Applicable boards: all packaged boards; only `pitaya_go_nrf52840` is expected to declare WiFi hardware.
// Limitations: validates capability truth, not a working WiFi link layer or socket stack.

#include <WiFi.h>

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.setChipset(NRF_WIFI_CHIPSET_NRF7002);
  WiFi.setTransport(NRF_WIFI_TRANSPORT_SPI);
  int beginStatus = WiFi.begin("test", "password");
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  NrfWiFiDriverInfo info = WiFi.driverInfo();
  Serial.print("status: ");
  Serial.println(WiFi.statusMessage());
  Serial.print("mac: ");
  for (size_t index = 0; index < sizeof(mac); ++index) {
    if (index > 0) {
      Serial.print(':');
    }
    if (mac[index] < 0x10U) {
      Serial.print('0');
    }
    Serial.print(mac[index], HEX);
  }
  Serial.println();

#if defined(ARDUINO_NRF52_PITAYA_GO)
  Serial.println("board expects WiFi hardware");
#else
  Serial.println("board does not model onboard WiFi hardware");
#endif

  Serial.print("hardware present: ");
  printYesNo(WiFi.hardwarePresent());
  Serial.print("transport configured: ");
  printYesNo(WiFi.transportConfigured());
  Serial.print("supported: ");
  printYesNo(WiFi.supported());
  Serial.print("facade only: ");
  printYesNo(WiFi.facadeOnly());
  Serial.print("link layer implemented: ");
  printYesNo(WiFi.linkLayerImplemented());
  Serial.print("socket API implemented: ");
  printYesNo(WiFi.socketApiImplemented());
  Serial.print("begin status: ");
  Serial.println(beginStatus);
  Serial.print("driver transport: ");
  Serial.println(static_cast<int>(info.transport));
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.localIP();
    WiFi.SSID();
  } else {
    WiFi.gatewayIP();
    WiFi.subnetMask();
    WiFi.RSSI();
  }
  delay(50);
}