#include <EEPROM.h>

namespace {
constexpr uint32_t kMagic = 0x4E524646;
constexpr uint16_t kCurrentVersion = 2;

struct StoredState {
  uint32_t magic;
  uint16_t version;
  uint16_t boots;
  uint8_t payload[16];
  uint32_t crc;
};

uint32_t checksum(const uint8_t *data, size_t length) {
  uint32_t value = 0x811C9DC5;
  for (size_t index = 0; index < length; ++index) {
    value ^= data[index];
    value *= 16777619;
  }
  return value;
}

uint32_t stateChecksum(const StoredState &state) {
  return checksum(reinterpret_cast<const uint8_t *>(&state), sizeof(state) - sizeof(state.crc));
}

bool isValid(const StoredState &state) {
  return state.magic == kMagic && state.version >= 1 && state.version <= kCurrentVersion && state.crc == stateChecksum(state);
}

void fillPayload(StoredState &state, uint8_t seed) {
  for (size_t index = 0; index < sizeof(state.payload); ++index) {
    state.payload[index] = static_cast<uint8_t>(seed + index * 7);
  }
}

void initializeDefaults(StoredState &state) {
  state.magic = kMagic;
  state.version = 1;
  state.boots = 0;
  fillPayload(state, 0x21);
  state.crc = stateChecksum(state);
}

void migrateToCurrent(StoredState &state) {
  if (state.version >= kCurrentVersion) {
    return;
  }
  fillPayload(state, static_cast<uint8_t>(0x30 + state.version));
  state.version = kCurrentVersion;
  state.crc = stateChecksum(state);
}

void printYesNo(bool value) {
  if (value) {
    Serial.println("yes");
  } else {
    Serial.println("no");
  }
}
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  StoredState defaults = {};
  initializeDefaults(defaults);

  StoredState state = {};
  bool loaded = EEPROM.get(0, state, defaults, isValid);

  if (state.version != kCurrentVersion) {
    migrateToCurrent(state);
  }

  ++state.boots;
  state.crc = stateChecksum(state);
  EEPROM.put(0, state);
  while (!EEPROM.commit()) {
    delay(1000);
  }

  StoredState verify = {};
  EEPROM.get(0, verify);

  Serial.print("loaded existing: ");
  printYesNo(loaded);
  Serial.print("version: ");
  Serial.println(verify.version);
  Serial.print("boots: ");
  Serial.println(verify.boots);
  Serial.print("crc valid: ");
  printYesNo(isValid(verify));
}

void loop() {
  delay(20);
}