void setup() {
  Serial1.begin(115200);
  Serial1.write('N');
  Serial1.write('R');
  Serial1.write('F');
}

void loop() {
  if (Serial1.available() > 0) {
    Serial1.read();
  }
  delay(20);
}
