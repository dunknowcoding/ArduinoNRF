#include "EEPROM.h"

#include <string.h>

namespace {
constexpr uint32_t FICR_BASE = 0x10000000UL;
constexpr uint32_t FICR_CODEPAGESIZE = 0x010UL;
constexpr uint32_t FICR_CODESIZE = 0x014UL;
constexpr uint32_t NVMC_BASE = 0x4001E000UL;
constexpr uint32_t NVMC_READY = 0x400UL;
constexpr uint32_t NVMC_CONFIG = 0x504UL;
constexpr uint32_t NVMC_ERASEPAGE = 0x508UL;
constexpr uint32_t NVMC_CONFIG_REN = 0UL;
constexpr uint32_t NVMC_CONFIG_WEN = 1UL;
constexpr uint32_t NVMC_CONFIG_EEN = 2UL;
constexpr uint32_t EEPROM_MAGIC = 0x4550524FUL;
constexpr uint32_t EEPROM_LOG_MAGIC = 0x45504C47UL;
constexpr uint32_t EEPROM_FLASH_BUFFER_SIZE = 4096UL;
constexpr uint16_t EEPROM_LEGACY_VERSION = 1U;
constexpr uint16_t EEPROM_LOG_VERSION = 2U;

struct EepromFlashLayout {
    uint32_t magic;
    uint16_t length;
    uint16_t version;
    uint32_t crc;
    uint8_t data[EEPROMClass::kSize];
};

struct EepromLogRecordHeader {
    uint32_t magic;
    uint16_t length;
    uint16_t version;
    uint32_t sequence;
    uint32_t crc;
};

static_assert(sizeof(EepromFlashLayout) <= EEPROM_FLASH_BUFFER_SIZE, "EEPROM layout must fit in one flash page");

constexpr size_t align4(size_t value) {
    return (value + 3U) & ~static_cast<size_t>(0x3U);
}

constexpr size_t EEPROM_LOG_RECORD_SIZE = align4(sizeof(EepromLogRecordHeader) + EEPROMClass::kSize);

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

extern "C" uint32_t __nrf_storage_region_start;
extern "C" uint32_t __nrf_storage_region_length;

uintptr_t linkerSymbolValue(uint32_t &symbol) {
    return reinterpret_cast<uintptr_t>(&symbol);
}

bool useLogStructuredBackend() {
#if defined(NRF_SYSTEM_STORAGE_BACKEND) && (NRF_SYSTEM_STORAGE_BACKEND == 2)
    return true;
#else
    return false;
#endif
}

uintptr_t storageRegionStart() {
    return linkerSymbolValue(__nrf_storage_region_start);
}

uintptr_t storageRegionLength() {
    return linkerSymbolValue(__nrf_storage_region_length);
}

uintptr_t storageRegionEnd() {
    return storageRegionStart() + storageRegionLength();
}

void nvmcWaitReady() {
    while (reg32(NVMC_BASE, NVMC_READY) == 0UL) {
    }
}

void nvmcSetMode(uint32_t mode) {
    reg32(NVMC_BASE, NVMC_CONFIG) = mode;
    nvmcWaitReady();
}

void eraseFlashPages(uintptr_t address, uint32_t length, uint32_t pageSize) {
    if (address == 0U || length == 0U || pageSize == 0U) {
        return;
    }

    nvmcSetMode(NVMC_CONFIG_EEN);
    for (uint32_t offset = 0; offset < length; offset += pageSize) {
        reg32(NVMC_BASE, NVMC_ERASEPAGE) = static_cast<uint32_t>(address + offset);
        nvmcWaitReady();
    }
    nvmcSetMode(NVMC_CONFIG_REN);
}

void writeFlashWords(uintptr_t address, const uint8_t *data, size_t length) {
    if (address == 0U || data == nullptr || length == 0U) {
        return;
    }

    nvmcSetMode(NVMC_CONFIG_WEN);
    for (size_t offset = 0; offset < length; offset += 4U) {
        uint32_t word = 0xFFFFFFFFUL;
        memcpy(&word, data + offset, sizeof(word));
        *reinterpret_cast<volatile uint32_t *>(address + offset) = word;
        nvmcWaitReady();
    }
    nvmcSetMode(NVMC_CONFIG_REN);
}

uintptr_t physicalFlashEnd() {
    const uint32_t pageSize = reg32(FICR_BASE, FICR_CODEPAGESIZE);
    const uint32_t codeSize = reg32(FICR_BASE, FICR_CODESIZE);
    if (pageSize == 0U || codeSize == 0U) {
        return 0U;
    }
    return static_cast<uintptr_t>(pageSize) * static_cast<uintptr_t>(codeSize);
}

uintptr_t legacyPageAddress(uint32_t pageSize) {
    const uintptr_t reservedLength = storageRegionLength();
    if (pageSize != 0U && reservedLength >= pageSize) {
        return storageRegionEnd() - pageSize;
    }

    const uintptr_t flashEnd = physicalFlashEnd();
    if (pageSize == 0U || flashEnd < pageSize) {
        return 0U;
    }
    return flashEnd - pageSize;
}

bool loadLegacyData(uint8_t *storage, uint32_t (*checksumFn)(const uint8_t *, size_t), uint32_t pageSize) {
    const uintptr_t pageAddress = legacyPageAddress(pageSize);
    if (pageAddress == 0U) {
        return false;
    }

    const EepromFlashLayout *layout = reinterpret_cast<const EepromFlashLayout *>(pageAddress);
    if (layout->magic != EEPROM_MAGIC || layout->length != EEPROMClass::kSize || layout->version != EEPROM_LEGACY_VERSION) {
        return false;
    }
    if (layout->crc != checksumFn(layout->data, EEPROMClass::kSize)) {
        return false;
    }

    memcpy(storage, layout->data, EEPROMClass::kSize);
    return true;
}

bool recordHeaderValid(const EepromLogRecordHeader *header) {
    return header != nullptr &&
        header->magic == EEPROM_LOG_MAGIC &&
        header->length == EEPROMClass::kSize &&
        header->version == EEPROM_LOG_VERSION;
}

bool loadLatestLogData(uint8_t *storage, uint32_t (*checksumFn)(const uint8_t *, size_t), uint32_t *sequenceOut) {
    const uintptr_t regionStart = storageRegionStart();
    const uintptr_t regionLengthBytes = storageRegionLength();
    if (regionStart == 0U || regionLengthBytes < EEPROM_LOG_RECORD_SIZE) {
        return false;
    }

    bool found = false;
    uint32_t newestSequence = 0U;
    uintptr_t newestRecord = 0U;
    for (uintptr_t offset = 0U; (offset + EEPROM_LOG_RECORD_SIZE) <= regionLengthBytes; offset += EEPROM_LOG_RECORD_SIZE) {
        const uintptr_t recordAddress = regionStart + offset;
        const EepromLogRecordHeader *header = reinterpret_cast<const EepromLogRecordHeader *>(recordAddress);
        if (!recordHeaderValid(header)) {
            continue;
        }

        const uint8_t *data = reinterpret_cast<const uint8_t *>(recordAddress + sizeof(EepromLogRecordHeader));
        if (header->crc != checksumFn(data, EEPROMClass::kSize)) {
            continue;
        }

        if (!found || header->sequence >= newestSequence) {
            found = true;
            newestSequence = header->sequence;
            newestRecord = recordAddress;
        }
    }

    if (!found) {
        return false;
    }

    memcpy(storage, reinterpret_cast<const void *>(newestRecord + sizeof(EepromLogRecordHeader)), EEPROMClass::kSize);
    if (sequenceOut != nullptr) {
        *sequenceOut = newestSequence;
    }
    return true;
}

uintptr_t firstFreeLogSlot() {
    const uintptr_t regionStart = storageRegionStart();
    const uintptr_t regionLengthBytes = storageRegionLength();
    if (regionStart == 0U || regionLengthBytes < EEPROM_LOG_RECORD_SIZE) {
        return 0U;
    }

    for (uintptr_t offset = 0U; (offset + EEPROM_LOG_RECORD_SIZE) <= regionLengthBytes; offset += EEPROM_LOG_RECORD_SIZE) {
        const uintptr_t recordAddress = regionStart + offset;
        const uint32_t firstWord = *reinterpret_cast<const volatile uint32_t *>(recordAddress);
        if (firstWord == 0xFFFFFFFFUL) {
            return recordAddress;
        }
    }
    return 0U;
}
}

uint8_t EEPROMClass::storage_[EEPROMClass::kSize] = {0};
bool EEPROMClass::dirty_ = false;
bool EEPROMClass::initialized_ = false;
EEPROMClass EEPROM;

bool EEPROMClass::begin(size_t length) {
    ensureLoaded();
    return length <= static_cast<size_t>(kSize);
}

void EEPROMClass::end() {
}

EEPROMClass::Cell EEPROMClass::operator[](int index) {
    return Cell(this, index);
}

uint8_t EEPROMClass::operator[](int index) const {
    return read(index);
}

bool EEPROMClass::isValid(int index) const {
    return index >= 0 && index < kSize;
}

size_t EEPROMClass::size() const {
    return static_cast<size_t>(kSize);
}

uint8_t EEPROMClass::read(int index) const {
    ensureLoaded();
    if (index < 0 || index >= kSize) {
        return 0;
    }
    return storage_[index];
}

void EEPROMClass::write(int index, uint8_t value) {
    ensureLoaded();
    if (index < 0 || index >= kSize) {
        return;
    }
    storage_[index] = value;
    dirty_ = true;
}

void EEPROMClass::update(int index, uint8_t value) {
    if (read(index) != value) {
        write(index, value);
    }
}

int EEPROMClass::length() const {
    return kSize;
}

bool EEPROMClass::commit() {
    ensureLoaded();
    bool changed = dirty_;
    if (!changed) {
        return false;
    }

    const uint32_t pageSize = flashPageSize();
    const uintptr_t pageAddress = flashPageAddress();
    if (pageSize == 0 || pageSize > EEPROM_FLASH_BUFFER_SIZE || pageAddress == 0U) {
        return false;
    }

    if (useLogStructuredBackend() && storageRegionLength() >= EEPROM_LOG_RECORD_SIZE) {
        uint32_t newestSequence = 0U;
        uint8_t stagedData[kSize];
        memcpy(stagedData, storage_, kSize);
        (void)loadLatestLogData(stagedData, checksum, &newestSequence);
        memcpy(stagedData, storage_, kSize);

        uint8_t recordBuffer[EEPROM_LOG_RECORD_SIZE];
        memset(recordBuffer, 0xFF, sizeof(recordBuffer));

        EepromLogRecordHeader *header = reinterpret_cast<EepromLogRecordHeader *>(recordBuffer);
        header->magic = EEPROM_LOG_MAGIC;
        header->length = static_cast<uint16_t>(kSize);
        header->version = EEPROM_LOG_VERSION;
        header->sequence = newestSequence + 1U;
        header->crc = checksum(stagedData, kSize);
        memcpy(recordBuffer + sizeof(EepromLogRecordHeader), stagedData, kSize);

        uintptr_t recordAddress = firstFreeLogSlot();
        if (recordAddress == 0U) {
            eraseFlashPages(storageRegionStart(), static_cast<uint32_t>(storageRegionLength()), pageSize);
            recordAddress = storageRegionStart();
        }
        if (recordAddress == 0U) {
            return false;
        }

        writeFlashWords(recordAddress, recordBuffer, sizeof(recordBuffer));
    } else {
        uint8_t pageBuffer[EEPROM_FLASH_BUFFER_SIZE];
        memset(pageBuffer, 0xFF, pageSize);

        EepromFlashLayout *layout = reinterpret_cast<EepromFlashLayout *>(pageBuffer);
        layout->magic = EEPROM_MAGIC;
        layout->length = static_cast<uint16_t>(kSize);
        layout->version = EEPROM_LEGACY_VERSION;
        memcpy(layout->data, storage_, kSize);
        layout->crc = checksum(layout->data, kSize);

        eraseFlashPages(pageAddress, pageSize, pageSize);
        writeFlashWords(pageAddress, pageBuffer, pageSize);
    }

    dirty_ = false;
    return changed;
}

void EEPROMClass::ensureLoaded() {
    if (initialized_) {
        return;
    }

    initialized_ = true;
    memset(storage_, 0, sizeof(storage_));
    uint32_t sequence = 0U;
    if (useLogStructuredBackend() && loadLatestLogData(storage_, checksum, &sequence)) {
        return;
    }
    (void)loadLegacyData(storage_, checksum, flashPageSize());
}

uintptr_t EEPROMClass::flashPageAddress() {
    const uint32_t pageSize = flashPageSize();
    return legacyPageAddress(pageSize);
}

uint32_t EEPROMClass::flashPageSize() {
    return reg32(FICR_BASE, FICR_CODEPAGESIZE);
}

uint32_t EEPROMClass::checksum(const uint8_t *data, size_t length) {
    uint32_t crc = 0x811C9DC5UL;
    for (size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        crc *= 16777619UL;
    }
    return crc;
}
