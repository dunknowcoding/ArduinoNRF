
void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

class DummyStream : public Stream {
public:
  using Print::write;

  explicit DummyStream(const char *input)
    : input_("") {
    if (input != nullptr) {
      input_ = input;
    }
  }

  int available(void) override {
    if (input_[index_] == '\0') {
      return 0;
    }
    return 1;
  }

  int read(void) override {
    if (input_[index_] == '\0') {
      return -1;
    }
    return input_[index_++];
  }

  int peek(void) override {
    if (input_[index_] == '\0') {
      return -1;
    }
    return input_[index_];
  }

  void flush(void) override {
  }

  size_t write(uint8_t value) override {
    if (outputLength_ >= sizeof(output_) - 1) {
      return 0;
    }
    output_[outputLength_++] = static_cast<char>(value);
    output_[outputLength_] = '\0';
    return 1;
  }

  const char *output() const {
    return output_;
  }

private:
  const char *input_;
  size_t index_ = 0;
  char output_[96] = {0};
  size_t outputLength_ = 0;
};

void setup() {
  Serial.begin(115200);

  DummyStream lineStream(" 123,45.50 done");
  lineStream.setTimeout(5);
  long whole = lineStream.parseInt();
  float fraction = lineStream.parseFloat(',');
  bool foundDone = lineStream.find("done");

  DummyStream readStream("alpha|beta");
  String first = readStream.readStringUntil('|');
  String second = readStream.readString();

  DummyStream bytesStream("XYZ!tail");
  char bytes[4] = {0};
  size_t copied = bytesStream.readBytesUntil('!', bytes, 3);
  bytes[copied] = '\0';

  String text("  Arduino NRF  ");
  text.trim();
  text.replace("NRF", "NRF52");
  text.remove(7, 1);
  bool sameIgnoreCase = String("hello").equalsIgnoreCase("HeLLo");
  double parsedDouble = String("3.75").toDouble();
  float parsedFloat = String("2.50").toFloat();

  DummyStream printStream("");
  const uint8_t buffer[] = { 'O', 'K' };
  printStream.write(buffer, sizeof(buffer));
  printStream.print(' ');
  printStream.print(true);
  printStream.print(' ');
  printStream.print(static_cast<unsigned char>(255), HEX);
  printStream.println();

  Serial.print("whole: ");
  Serial.println(whole);
  Serial.print("fraction: ");
  Serial.println(fraction, 2);
  Serial.print("found done: ");
  printYesNo(foundDone);
  Serial.print("first token: ");
  Serial.println(first);
  Serial.print("second token: ");
  Serial.println(second);
  Serial.print("copied bytes: ");
  Serial.println(copied);
  Serial.print("byte text: ");
  Serial.println(bytes);
  Serial.print("trimmed text: ");
  Serial.println(text);
  Serial.print("equals ignore case: ");
  printYesNo(sameIgnoreCase);
  Serial.print("parsed double: ");
  Serial.println(parsedDouble, 2);
  Serial.print("parsed float: ");
  Serial.println(parsedFloat, 2);
  Serial.print("print output: ");
  Serial.println(printStream.output());
}

void loop() {
  delay(50);
}