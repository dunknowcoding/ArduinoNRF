#include <EEPROM.h>

namespace {
constexpr uint32_t kMagic = 0x45534F4B;
constexpr int kWritesPerBoot = 16;

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}
}

struct SoakState {
  uint32_t magic;
  uint32_t cycles;
  uint32_t checksum;
};

static uint32_t checksumFor(const SoakState &state) {
  return state.magic ^ state.cycles ^ 0xA5A55A5A;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  const SoakState defaults = {kMagic, 0, 0};
  SoakState state = {};
  bool loaded = EEPROM.get(0, state, defaults, [](const SoakState &value) {
    return value.magic == kMagic && value.checksum == checksumFor(value);
  });

  for (int index = 0; index < kWritesPerBoot; ++index) {
    ++state.cycles;
    state.checksum = checksumFor(state);
    EEPROM.put(0, state);
    while (!EEPROM.commit()) {
      delay(1000);
    }

    SoakState verify = {0, 0, 0};
    EEPROM.get(0, verify);
    Serial.print("cycle ");
    Serial.print(index + 1);
    Serial.print(": ");
    Serial.println(verify.cycles);
  }

  Serial.print("loaded existing: ");
  printYesNo(loaded);
}

void loop() {
  delay(50);
}