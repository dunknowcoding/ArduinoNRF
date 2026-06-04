#include <WiFi.h>

static const char kFlashText[] = "flash-text";

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

void setup() {
  Serial.begin(115200);

  String text("  Arduino WiFi Client  ");
  text.trim();
  int wifiIndex = text.indexOf("WiFi");
  int lastI = text.lastIndexOf('i');
  bool offsetPrefix = text.startsWith("WiFi", 8);
  bool compareLess = String("abc").compareTo("abd") < 0;
  bool equalsOk = String("demo").equals("demo");
  text.toLowerCase();
  text.toUpperCase();
  bool concatOk = text.concat('!') && text.concat("OK");

  IPAddress ip;
  bool parsed = ip.fromString(String("10.20.30.40"));
  bool wasSet = ip.isSet();
  uint8_t *raw = ip.raw_address();
  raw[3] = 41;
  IPAddress copied(raw);
  IPAddress assigned;
  assigned = static_cast<uint32_t>(ip);

  WiFi.setChipset(NRF_WIFI_CHIPSET_NRF7002);
  WiFi.setTransport(NRF_WIFI_TRANSPORT_SPI);
  WiFi.setHostname("arduinonrf-nrf52-lab");
  WiFi.setAutoReconnect(true);
  IPAddress resolved;
  int resolveStatus = WiFi.hostByName("192.168.1.20", resolved);
  char bssid[18] = {0};
  WiFi.BSSIDstr(bssid);

  WiFiClient client;
  int connectStatus = client.connect(resolved, 80);

  Serial.println(F("Extended compat example"));
  Serial.println(FPSTR(kFlashText));

  Serial.print("wifiIndex: ");
  Serial.println(wifiIndex);
  Serial.print("last index of i: ");
  Serial.println(lastI);
  Serial.print("offset prefix matches: ");
  printYesNo(offsetPrefix);
  Serial.print("compareTo result is less: ");
  printYesNo(compareLess);
  Serial.print("equals ok: ");
  printYesNo(equalsOk);
  Serial.print("concat ok: ");
  printYesNo(concatOk);
  Serial.print("IP parsed: ");
  printYesNo(parsed);
  Serial.print("IP set: ");
  printYesNo(wasSet);
  Serial.print("copied IP: ");
  Serial.println(copied);
  Serial.print("assigned IP: ");
  Serial.println(assigned);
  Serial.print("resolve status: ");
  Serial.println(resolveStatus);
  Serial.print("resolved IP: ");
  Serial.println(resolved);
  Serial.print("auto reconnect: ");
  printYesNo(WiFi.autoReconnect());
  Serial.print("hostname: ");
  Serial.println(WiFi.hostname());
  Serial.print("client remote IP: ");
  Serial.println(client.remoteIP());
  Serial.print("client remote port: ");
  Serial.println(client.remotePort());
  Serial.print("client available for write: ");
  Serial.println(client.availableForWrite());
  Serial.print("client status: ");
  Serial.println(client.status());
  Serial.print("connect status: ");
  Serial.println(connectStatus);
  Serial.print("SPI pins: ");
  Serial.print(SPI.pinMOSI());
  Serial.print(',');
  Serial.print(SPI.pinMISO());
  Serial.print(',');
  Serial.println(SPI.pinSCK());
  Serial.print("Wire pins: ");
  Serial.print(Wire.pinSDA());
  Serial.print(',');
  Serial.println(Wire.pinSCL());
  Serial.print("clock cycles/us: ");
  Serial.println(clockCyclesPerMicrosecond());
  Serial.print("clock cycles for 10 us: ");
  Serial.println(microsecondsToClockCycles(10));
  Serial.print("microseconds for one cycle block: ");
  Serial.println(clockCyclesToMicroseconds(clockCyclesPerMicrosecond()));
  Serial.print("Serial1 baud via macro: ");
  Serial.println(SERIAL_PORT_HARDWARE1.baudRate());
}

void loop() {
  delay(50);
}