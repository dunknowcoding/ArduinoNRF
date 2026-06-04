
namespace {
constexpr uint8_t kUsbDescInterface = 0x04;

void printYesNo(const char *label, bool value) {
  Serial.print(label);
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}

class DummyUsbModule : public PluggableUSBModule {
public:
  DummyUsbModule() : PluggableUSBModule(0, 1, nullptr) {
  }

  int getInterface(uint8_t *interfaceCount) override {
    const uint8_t descriptor[] = {
      0x09, kUsbDescInterface, pluggedInterface, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00
    };
    if (!appendDescriptor(descriptor, sizeof(descriptor))) {
      return 0;
    }
    if (interfaceCount != nullptr) {
      ++(*interfaceCount);
    }
    return sizeof(descriptor);
  }

  int getDescriptor(USBSetup &) override {
    return 0;
  }

  bool setup(USBSetup &) override {
    return false;
  }

  uint8_t getShortName(char *name) override {
    if (name != nullptr) {
      name[0] = 'D';
    }
    return 1;
  }
};

DummyUsbModule g_dummyModule;
}

void setup() {
  PluggableUSB().plug(&g_dummyModule);
  USBDevice.attach();
  Serial.begin(115200);
  while (!USBDevice.connected() && millis() < 1000) {
    USBDevice.poll();
    delay(10);
  }

  Serial.println("USB device example");
  printYesNo("attached=", USBDevice.attached());
  printYesNo("ready=", USBDevice.ready());
  Serial.print("address=");
  Serial.println(USBDevice.address());
  Serial.print("configuration=");
  Serial.println(USBDevice.configuration());
  printYesNo("configured=", USBDevice.configured());
  printYesNo("connected=", USBDevice.connected());
  Serial.print("totalInterfaces=");
  Serial.println(PluggableUSB().totalInterfaceCount());
  printYesNo("wakeupHost=", USBDevice.wakeupHost());
  Serial.print("dummy interface index=");
  Serial.println(g_dummyModule.pluggedInterface);

  USBDevice.detach();
  delay(20);
  USBDevice.attach();
}

void loop() {
  USBDevice.poll();
  delay(50);
}