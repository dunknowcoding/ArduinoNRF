#include <Arduino.h>

namespace {
constexpr uint8_t kShortNameCapacity = 8;
constexpr uint8_t kUsbDescInterface = 0x04;

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

class CompatibilityModule : public PluggableUSBModule {
public:
  CompatibilityModule() : PluggableUSBModule(1, 1, nullptr) {
  }

  int getInterface(uint8_t *interfaceCount) override {
    const uint8_t descriptor[] = {
      0x09, kUsbDescInterface, pluggedInterface, 0x00, 0x01, 0xFF, 0x00, 0x00, 0x00
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
    if (((setup.wValue >> 8) & 0xFF) != 0x22) {
      return 0;
    }

    const uint8_t reportDescriptor[] = {0x06, 0x00, 0xFF};
    if (appendDescriptor(reportDescriptor, sizeof(reportDescriptor))) {
      return sizeof(reportDescriptor);
    }
    return 0;
  }

  bool setup(USBSetup &setup) override {
    return setup.bRequest == 0x7F;
  }

  uint8_t getShortName(char *name) override {
    if (name != nullptr) {
      name[0] = 'C';
      name[1] = 'M';
    }
    return 2;
  }
};

CompatibilityModule g_module;
}

void setup() {
  char shortName[kShortNameCapacity] = {0};
  uint8_t descriptorBuffer[8] = {0};
  uint8_t interfaceCount = 0;
  USBSetup setupPacket = {0x21, 0x7F, 0, 0, 0};
  USBSetup descriptorSetup = {0x81, 0x06, 0x2200, 0, 3};

  PluggableUSB().plug(&g_module);
  USBDevice.init();
  PluggableUSB().setup(setupPacket);
  PluggableUSB().getShortName(shortName);
  PluggableUSB().beginDescriptorBuild(descriptorBuffer, sizeof(descriptorBuffer));
  const int reportLength = PluggableUSB().getDescriptor(descriptorSetup);
  const size_t builtLength = PluggableUSB().endDescriptorBuild();
  PluggableUSB().getInterface(&interfaceCount);

  USBDevice.detach();
  delay(5);
  USBDevice.attach();

  Serial.begin(115200);
  for (uint8_t tries = 0; tries < 50 && !USBDevice.connected(); ++tries) {
    USBDevice.poll();
    delay(10);
  }

  Serial.print("initialized: ");
  printYesNo(USBDevice.initialized());
  Serial.print("plugged interface: ");
  Serial.println(g_module.pluggedInterface);
  Serial.print("plugged endpoint: ");
  Serial.println(g_module.pluggedEndpoint);
  Serial.print("total interfaces: ");
  Serial.println(PluggableUSB().totalInterfaceCount());
  Serial.print("short name: ");
  Serial.println(shortName);
  Serial.print("report length: ");
  Serial.println(reportLength);
  Serial.print("built length: ");
  Serial.println(static_cast<unsigned long>(builtLength));
  Serial.print("first descriptor byte: 0x");
  Serial.println(descriptorBuffer[0], HEX);
}

void loop() {
  USBDevice.poll();
  delay(20);
}