#include <Arduino.h>

namespace {
constexpr uint8_t kUsbDescInterface = 0x04;
constexpr uint8_t kUsbDescEndpoint = 0x05;
constexpr uint8_t kUsbDescHid = 0x21;
constexpr uint8_t kUsbDescReport = 0x22;
constexpr uint8_t kHidReqGetReport = 0x01;
constexpr uint8_t kHidReqGetIdle = 0x02;
constexpr uint8_t kHidReqGetProtocol = 0x03;
constexpr uint8_t kHidReqSetReport = 0x09;
constexpr uint8_t kHidReqSetIdle = 0x0A;
constexpr uint8_t kHidReqSetProtocol = 0x0B;

class HidInModule : public PluggableUSBModule {
public:
  HidInModule() : PluggableUSBModule(1, 1, nullptr) {
  }

  int getInterface(uint8_t *interfaceCount) override {
    const uint8_t descriptor[] = {
      0x09, kUsbDescInterface, pluggedInterface, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00,
      0x09, kUsbDescHid, 0x11, 0x01, 0x00, 0x01, kUsbDescReport, 0x19, 0x00,
      0x07, kUsbDescEndpoint, static_cast<uint8_t>(0x80 | pluggedEndpoint), 0x03, 0x08, 0x00, 0x10
    };
    if (!appendDescriptor(descriptor, sizeof(descriptor))) {
      return 0;
    }
    if (interfaceCount != nullptr) {
      ++(*interfaceCount);
    }
    return sizeof(descriptor);
  }

  int getDescriptor(USBSetup &setup) override {
    if (((setup.wValue >> 8) & 0xFF) != kUsbDescReport || (setup.wIndex & 0xFF) != pluggedInterface) {
      return 0;
    }

    const uint8_t reportDescriptor[] = {
      0x06, 0x00, 0xFF,
      0x09, 0x01,
      0xA1, 0x01,
      0x15, 0x00,
      0x26, 0xFF, 0x00,
      0x75, 0x08,
      0x95, 0x08,
      0x09, 0x01,
      0x81, 0x02,
      0xC0
    };
    if (appendDescriptor(reportDescriptor, sizeof(reportDescriptor))) {
      return sizeof(reportDescriptor);
    }
    return 0;
  }

  int getSetupResponse(USBSetup &setup) override {
    if ((setup.wIndex & 0xFF) != pluggedInterface) {
      return 0;
    }

    switch (setup.bRequest) {
      case kHidReqGetReport:
        if (appendDescriptor(lastReport_, sizeof(lastReport_))) {
          return sizeof(lastReport_);
        }
        return 0;
      case kHidReqGetIdle:
        if (appendDescriptor(&idleRate_, sizeof(idleRate_))) {
          return sizeof(idleRate_);
        }
        return 0;
      case kHidReqGetProtocol:
        if (appendDescriptor(&protocol_, sizeof(protocol_))) {
          return sizeof(protocol_);
        }
        return 0;
      default:
        return 0;
    }
  }

  bool setup(USBSetup &setup) override {
    if ((setup.wIndex & 0xFF) != pluggedInterface) {
      return false;
    }

    switch (setup.bRequest) {
      case kHidReqSetIdle:
        idleRate_ = static_cast<uint8_t>((setup.wValue >> 8) & 0xFF);
        return true;
      case kHidReqSetProtocol:
        protocol_ = static_cast<uint8_t>(setup.wValue & 0xFF);
        return true;
      case kHidReqSetReport:
        return true;
      default:
        return false;
    }
  }

  uint8_t getShortName(char *name) override {
    if (name != nullptr) {
      name[0] = 'H';
      name[1] = 'I';
      name[2] = 'D';
    }
    return 3;
  }

  void onEndpointComplete(uint8_t endpoint) override {
    lastCompletedEndpoint_ = endpoint;
    transferComplete_ = true;
  }

  bool sendReport(uint8_t counter) {
    lastReport_[0] = counter;
    lastReport_[1] = static_cast<uint8_t>(counter + 1);
    lastReport_[2] = static_cast<uint8_t>(counter + 2);
    lastReport_[3] = static_cast<uint8_t>(counter + 3);
    lastReport_[4] = idleRate_;
    lastReport_[5] = protocol_;
    lastReport_[6] = 0;
    lastReport_[7] = 0;
    transferComplete_ = false;
    return send(lastReport_, sizeof(lastReport_));
  }

  bool transferComplete() const {
    return transferComplete_;
  }

  uint8_t lastCompletedEndpoint() const {
    return lastCompletedEndpoint_;
  }

  uint8_t idleRate() const {
    return idleRate_;
  }

  uint8_t protocol() const {
    return protocol_;
  }

private:
  bool transferComplete_ = false;
  uint8_t lastCompletedEndpoint_ = 0;
  uint8_t idleRate_ = 0;
  uint8_t protocol_ = 1;
  uint8_t lastReport_[8] = {0};
};

HidInModule g_hidModule;
}

void setup() {
  uint8_t controlResponse[16] = {0};
  USBSetup getProtocol = {0xA1, kHidReqGetProtocol, 0, 0x0003, 1};
  USBSetup getIdle = {0xA1, kHidReqGetIdle, 0, 0x0003, 1};
  USBSetup setIdle = {0x21, kHidReqSetIdle, 0x0500, 0x0003, 0};
  USBSetup setProtocol = {0x21, kHidReqSetProtocol, 0x0000, 0x0003, 0};
  USBSetup getReport = {0xA1, kHidReqGetReport, 0x0100, 0x0003, 8};

  PluggableUSB().plug(&g_hidModule);
  USBDevice.attach();
  g_hidModule.setup(setIdle);
  g_hidModule.setup(setProtocol);
  PluggableUSB().beginDescriptorBuild(controlResponse, sizeof(controlResponse));
  const int protocolLength = g_hidModule.getSetupResponse(getProtocol);
  const size_t protocolBuilt = PluggableUSB().endDescriptorBuild();
  PluggableUSB().beginDescriptorBuild(controlResponse, sizeof(controlResponse));
  const int idleLength = g_hidModule.getSetupResponse(getIdle);
  const size_t idleBuilt = PluggableUSB().endDescriptorBuild();
  g_hidModule.sendReport(0x2A);
  PluggableUSB().beginDescriptorBuild(controlResponse, sizeof(controlResponse));
  const int reportLength = g_hidModule.getSetupResponse(getReport);
  const size_t reportBuilt = PluggableUSB().endDescriptorBuild();
  Serial.begin(115200);
  for (uint8_t tries = 0; tries < 100 && !USBDevice.configured(); ++tries) {
    USBDevice.poll();
    delay(10);
  }

  Serial.print("protocol length: ");
  Serial.println(protocolLength);
  Serial.print("protocol built bytes: ");
  Serial.println(static_cast<unsigned long>(protocolBuilt));
  Serial.print("idle length: ");
  Serial.println(idleLength);
  Serial.print("idle built bytes: ");
  Serial.println(static_cast<unsigned long>(idleBuilt));
  Serial.print("idle rate: ");
  Serial.println(g_hidModule.idleRate());
  Serial.print("report length: ");
  Serial.println(reportLength);
  Serial.print("report built bytes: ");
  Serial.println(static_cast<unsigned long>(reportBuilt));
}

void loop() {
  static uint8_t counter = 0;
  USBDevice.poll();
  if (USBDevice.configured()) {
    g_hidModule.sendReport(counter++);
  }
  if (g_hidModule.transferComplete()) {
    Serial.print("last completed endpoint: ");
    Serial.println(g_hidModule.lastCompletedEndpoint());
  }
  delay(20);
}