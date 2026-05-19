#include "PeripheralLease.h"

namespace {
struct PeripheralLeaseSlot {
    uint32_t base;
    const void *owner;
};

PeripheralLeaseSlot g_peripheralLeases[] = {
    {0x40003000UL, nullptr},
    {0x40004000UL, nullptr},
    {0x40023000UL, nullptr},
};

PeripheralLeaseSlot *findSlot(uint32_t base) {
    for (PeripheralLeaseSlot &slot : g_peripheralLeases) {
        if (slot.base == base) {
            return &slot;
        }
    }
    return nullptr;
}
}

bool nrfPeripheralTake(uint32_t base, const void *owner) {
    PeripheralLeaseSlot *slot = findSlot(base);
    if (slot == nullptr) {
        return false;
    }

    slot->owner = owner;
    return true;
}

bool nrfPeripheralOwnerIs(uint32_t base, const void *owner) {
    PeripheralLeaseSlot *slot = findSlot(base);
    return slot != nullptr && slot->owner == owner;
}

void nrfPeripheralRelease(uint32_t base, const void *owner) {
    PeripheralLeaseSlot *slot = findSlot(base);
    if (slot != nullptr && slot->owner == owner) {
        slot->owner = nullptr;
    }
}