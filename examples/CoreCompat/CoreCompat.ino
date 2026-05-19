#include <Arduino.h>

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

void setup() {
  String prefix("Arduino");
  prefix += ' ';
  prefix += String(52);
  String suffix(" Boards");
  String combined = prefix;
  combined += suffix;

  IPAddress ip;
  bool parsed = ip.fromString("192.168.4.1");

  Serial.begin(115200);
  Serial.print(combined);
  Serial.print(' ');
  Serial.print(ip);
  Serial.print(' ');
  Serial.println(3.14, 2);
  Serial.print("parsed: ");
  printYesNo(parsed);
  Serial.print("startsWith Arduino: ");
  printYesNo(combined.startsWith("Arduino"));
  Serial.print("endsWith Boards: ");
  printYesNo(combined.endsWith("Boards"));
  Serial.print("indexOf('5'): ");
  Serial.println(combined.indexOf('5'));
  Serial.print("substring(8,10): ");
  Serial.println(combined.substring(8, 10));
  Serial.print("first IP octet: ");
  Serial.println(ip[0]);
}

void loop() {
  delay(50);
}