#include <Arduino.h>

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

class DummyClient : public Client {
public:
  using Client::read;
  using Client::write;

  int connect(IPAddress, uint16_t port) override {
    connected_ = port != 0;
    if (connected_) {
      return 1;
    }
    return 0;
  }

  int connect(const char *host, uint16_t port) override {
    connected_ = host != nullptr && host[0] != '\0' && port != 0;
    if (connected_) {
      return 1;
    }
    return 0;
  }

  int available(void) override {
    if (connected_) {
      return static_cast<int>(sizeof(readBuffer_) - readIndex_);
    }
    return 0;
  }

  int read(void) override {
    if (readIndex_ >= sizeof(readBuffer_)) {
      return -1;
    }
    return readBuffer_[readIndex_++];
  }

  int peek(void) override {
    if (readIndex_ >= sizeof(readBuffer_)) {
      return -1;
    }
    return readBuffer_[readIndex_];
  }

  void flush(void) override {
  }

  size_t write(uint8_t value) override {
    if (writeLength_ >= sizeof(writeBuffer_)) {
      return 0;
    }
    writeBuffer_[writeLength_++] = value;
    return 1;
  }

  uint8_t connected() override {
    if (connected_) {
      return 1;
    }
    return 0;
  }

  void stop() override {
    connected_ = false;
  }

  operator bool() override {
    return connected_;
  }

  size_t writeLength() const {
    return writeLength_;
  }

private:
  bool connected_ = false;
  uint8_t readBuffer_[3] = {0x11, 0x22, 0x33};
  uint8_t writeBuffer_[8] = {0};
  size_t readIndex_ = 0;
  size_t writeLength_ = 0;
};

class DummyServer : public Server {
public:
  using Server::begin;

  void begin() override {
    began_ = true;
  }

  size_t write(uint8_t value) override {
    lastByte_ = value;
    return 1;
  }

  bool began() const {
    return began_;
  }

  uint8_t lastByte() const {
    return lastByte_;
  }

private:
  bool began_ = false;
  uint8_t lastByte_ = 0;
};

class DummyUDP : public UDP {
public:
  using UDP::read;
  using UDP::write;

  uint8_t begin(uint16_t port) override {
    localPort_ = port;
    if (port != 0) {
      return 1;
    }
    return 0;
  }

  int beginPacket(IPAddress ip, uint16_t port) override {
    remoteIp_ = ip;
    remotePort_ = port;
    packetLength_ = 0;
    readIndex_ = 0;
    return 1;
  }

  int beginPacket(const char *host, uint16_t port) override {
    if (host != nullptr && host[0] != '\0') {
      remoteIp_ = IPAddress(192, 168, 1, 10);
    } else {
      remoteIp_ = IPAddress();
    }
    remotePort_ = port;
    packetLength_ = 0;
    readIndex_ = 0;
    return 1;
  }

  int endPacket() override {
    return static_cast<int>(packetLength_);
  }

  int parsePacket() override {
    readIndex_ = 0;
    return static_cast<int>(packetLength_);
  }

  int available(void) override {
    return static_cast<int>(packetLength_ - readIndex_);
  }

  int read(void) override {
    if (readIndex_ >= packetLength_) {
      return -1;
    }
    return packetBuffer_[readIndex_++];
  }

  int peek(void) override {
    if (readIndex_ >= packetLength_) {
      return -1;
    }
    return packetBuffer_[readIndex_];
  }

  void flush(void) override {
  }

  size_t write(uint8_t value) override {
    if (packetLength_ >= sizeof(packetBuffer_)) {
      return 0;
    }
    packetBuffer_[packetLength_++] = value;
    return 1;
  }

  IPAddress remoteIP() override {
    return remoteIp_;
  }

  uint16_t remotePort() override {
    return remotePort_;
  }

  void stop() override {
    packetLength_ = 0;
    readIndex_ = 0;
    localPort_ = 0;
  }

  uint16_t localPort() override {
    return localPort_;
  }

private:
  IPAddress remoteIp_;
  uint16_t remotePort_ = 0;
  uint16_t localPort_ = 0;
  uint8_t packetBuffer_[16] = {0};
  size_t packetLength_ = 0;
  size_t readIndex_ = 0;
};

void setup() {
  Serial.begin(115200);

  DummyClient client;
  uint8_t clientWriteData[] = {0xAA, 0x55, 0x10};
  size_t clientWritten = client.write(clientWriteData, sizeof(clientWriteData));
  uint8_t clientReadData[3] = {0};
  client.connect("arduinonrf", 80);
  int clientRead = client.read(clientReadData, sizeof(clientReadData));

  DummyServer server;
  server.begin(8080);
  server.write('S');

  DummyUDP udp;
  udp.begin(1234);
  udp.beginMulticast(IPAddress(239, 1, 2, 3), 1234);
  udp.beginPacket(IPAddress(192, 168, 4, 1), 5000);
  udp.write(clientWriteData, sizeof(clientWriteData));
  udp.endPacket();
  int parsed = udp.parsePacket();
  uint8_t udpReadData[3] = {0};
  int udpRead = udp.read(udpReadData, sizeof(udpReadData));

  Wire.begin(static_cast<uint8_t>(0x42));
  Wire.setWireTimeout(3000, true);
  Wire.onReceive(nullptr);
  Wire.onRequest(nullptr);
  int wirePeek = Wire.peek();
  Wire.flush();

  SPI.begin();
  SPI.usingInterrupt(digitalPinToInterrupt(2));
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  const uint8_t spiTx[] = {0x9F, 0x00, 0x00, 0x00};
  uint8_t spiRx[sizeof(spiTx)] = {0};
  SPI.transfer(spiTx, spiRx, sizeof(spiTx));
  SPI.endTransaction();
  SPI.notUsingInterrupt(digitalPinToInterrupt(2));

  Serial.print("client written: ");
  Serial.println(static_cast<unsigned long>(clientWritten));
  Serial.print("client read: ");
  Serial.println(clientRead);
  Serial.print("client connected: ");
  Serial.println(client.connected());
  Serial.print("server began: ");
  printYesNo(server.began());
  Serial.print("server last byte: ");
  Serial.println(static_cast<char>(server.lastByte()));
  Serial.print("udp parsed length: ");
  Serial.println(parsed);
  Serial.print("udp read length: ");
  Serial.println(udpRead);
  Serial.print("udp local port: ");
  Serial.println(udp.localPort());
  Serial.print("udp destination IP: ");
  Serial.println(udp.destinationIP());
  Serial.print("wire peek: ");
  Serial.println(wirePeek);
  Serial.print("wire timeout flag: ");
  printYesNo(Wire.getWireTimeoutFlag());
}

void loop() {
  delay(50);
}