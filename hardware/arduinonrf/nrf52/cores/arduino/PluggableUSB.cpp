#include "PluggableUSB.h"

#include "NrfSystem.h"
#include "NrfUsbd.h"

namespace {
PluggableUSB_ g_pluggableUsb;
}

PluggableUSBModule::PluggableUSBModule(uint8_t endpointCount, uint8_t interfaceCount, const uint8_t *endpointTypes)
    : numEndpoints(endpointCount), numInterfaces(interfaceCount), endpointType(endpointTypes) {
}

int PluggableUSBModule::getSetupResponse(USBSetup &setup) {
    (void)setup;
    return 0;
}

bool PluggableUSBModule::claimControlOut(const USBSetup &setup) {
    (void)setup;
    return false;
}

bool PluggableUSBModule::completeControlOut(const USBSetup &setup, const uint8_t *data,
                                            size_t length) {
    (void)setup;
    (void)data;
    (void)length;
    return false;
}

uint8_t PluggableUSBModule::getShortName(char *name) {
    (void)name;
    return 0U;
}

void PluggableUSBModule::onEndpointComplete(uint8_t endpoint) {
    (void)endpoint;
}

bool PluggableUSBModule::appendDescriptor(const void *data, size_t length) {
    return PluggableUSB().appendDescriptor(data, length);
}

bool PluggableUSBModule::send(const void *data, size_t length) {
    return PluggableUSB().send(this, data, length);
}

bool PluggableUSB_::plug(PluggableUSBModule *module) {
    if (module == nullptr) {
        return false;
    }

    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
        if (node == module) {
            return true;
        }
    }

    if (rootNode_ == nullptr) {
        lastInterface_ = nrfUsbUserPortEnabled() ? 5U : 3U;
        lastEndpoint_ = nrfUsbUserPortEnabled() ? 5U : 3U;
    }

    // nRF52840 exposes endpoint numbers 0..7; EP0 and the compiled CDC/DFU
    // functions already own the prefix represented by lastEndpoint_. Reject
    // overcommit before linking the module, rather than emitting descriptors
    // for endpoints the controller can never enable.
    if ((module->numEndpoints != 0U && module->endpointType == nullptr) ||
        static_cast<uint16_t>(lastEndpoint_) + module->numEndpoints > 8U ||
        static_cast<uint16_t>(lastInterface_) + module->numInterfaces > 255U) {
        return false;
    }

    module->pluggedInterface = lastInterface_;
    module->pluggedEndpoint = lastEndpoint_;
    lastInterface_ = static_cast<uint8_t>(lastInterface_ + module->numInterfaces);
    lastEndpoint_ = static_cast<uint8_t>(lastEndpoint_ + module->numEndpoints);

    if (rootNode_ == nullptr) {
        rootNode_ = module;
        return true;
    }

    PluggableUSBModule *tail = rootNode_;
    while (tail->next != nullptr) {
        if (tail == module) {
            return true;
        }
        tail = tail->next;
    }
    if (tail == module) {
        return true;
    }
    tail->next = module;
    return true;
}

int PluggableUSB_::getInterface(uint8_t *interfaceCount) {
    if (interfaceCount == nullptr) {
        return 0;
    }
    int total = 0;
    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
        node->descriptorAdmitted = false;
        const size_t beforeLength = descriptorLength_;
        const uint8_t beforeInterfaces = *interfaceCount;
        // Descriptor rejection must not leave an interface-number hole for a
        // later valid module. Rebind each candidate to the next admitted
        // interface for this complete configuration build.
        node->pluggedInterface = beforeInterfaces;
        descriptorOverflowed_ = false;
        const int reported = node->getInterface(interfaceCount);
        const size_t written = descriptorLength_ - beforeLength;
        const uint16_t expectedInterfaces =
            static_cast<uint16_t>(beforeInterfaces) + node->numInterfaces;
        const bool valid = reported >= 0 &&
            (node->numInterfaces == 0U || reported > 0) && !descriptorOverflowed_ &&
            static_cast<size_t>(reported) == written &&
            expectedInterfaces <= 255U &&
            *interfaceCount == static_cast<uint8_t>(expectedInterfaces);
        if (!valid) {
            descriptorLength_ = beforeLength;
            *interfaceCount = beforeInterfaces;
            continue;
        }
        node->descriptorAdmitted = true;
        total += reported;
    }
    descriptorOverflowed_ = false;
    lastInterface_ = *interfaceCount;
    return total;
}

int PluggableUSB_::getDescriptor(USBSetup &setup) {
    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
        if (!node->descriptorAdmitted) {
            continue;
        }
        const size_t beforeLength = descriptorLength_;
        descriptorOverflowed_ = false;
        const int reported = node->getDescriptor(setup);
        const bool lengthMonotonic = descriptorLength_ >= beforeLength;
        const size_t written = lengthMonotonic ? descriptorLength_ - beforeLength : 0U;
        if (reported == 0 && written == 0U && !descriptorOverflowed_ && lengthMonotonic) {
            continue;
        }
        if (reported <= 0 || descriptorOverflowed_ || !lengthMonotonic ||
            static_cast<size_t>(reported) != written) {
            descriptorLength_ = beforeLength;
            descriptorOverflowed_ = false;
            return -1;
        }
        // A control request has exactly one owner. Stop at the first complete,
        // internally consistent response instead of concatenating responses
        // from two modules that accidentally claim the same selector.
        return reported;
    }
    return 0;
}

int PluggableUSB_::getSetupResponse(USBSetup &setup) {
    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
        if (!node->descriptorAdmitted) {
            continue;
        }
        const size_t beforeLength = descriptorLength_;
        descriptorOverflowed_ = false;
        const int reported = node->getSetupResponse(setup);
        const bool lengthMonotonic = descriptorLength_ >= beforeLength;
        const size_t written = lengthMonotonic ? descriptorLength_ - beforeLength : 0U;
        if (reported == 0 && written == 0U && !descriptorOverflowed_ && lengthMonotonic) {
            continue;
        }
        if (reported <= 0 || descriptorOverflowed_ || !lengthMonotonic ||
            static_cast<size_t>(reported) != written) {
            descriptorLength_ = beforeLength;
            descriptorOverflowed_ = false;
            return -1;
        }
        return reported;
    }
    return 0;
}

PluggableUSBModule *PluggableUSB_::controlOutOwner(const USBSetup &setup) {
    PluggableUSBModule *owner = nullptr;
    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
        if (!node->descriptorAdmitted || !node->claimControlOut(setup)) {
            continue;
        }
        if (owner != nullptr) {
            // Ambiguous ownership is never resolved by link order. A module's
            // claim callback is a query and must not mutate state, so rejecting
            // the whole request is transactional.
            return nullptr;
        }
        owner = node;
    }
    return owner;
}

bool PluggableUSB_::completeControlOut(PluggableUSBModule *owner, const USBSetup &setup,
                                       const uint8_t *data, size_t length) {
    if (owner == nullptr || data == nullptr || length == 0U) {
        return false;
    }
    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
        if (node == owner) {
            return node->descriptorAdmitted &&
                node->completeControlOut(setup, data, length);
        }
    }
    return false;
}

bool PluggableUSB_::setup(USBSetup &setup) {
    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
        if (!node->descriptorAdmitted) {
            continue;
        }
        if (node->setup(setup)) {
            return true;
        }
    }
    return false;
}

uint8_t PluggableUSB_::getShortName(char *name) {
    uint8_t length = 0U;
    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
        if (!node->descriptorAdmitted) {
            continue;
        }
        char *cursor = nullptr;
        if (name != nullptr) {
            cursor = name + length;
        }
        length = static_cast<uint8_t>(length + node->getShortName(cursor));
    }
    return length;
}

void PluggableUSB_::beginDescriptorBuild(uint8_t *buffer, size_t capacity, size_t initialLength) {
    descriptorBuffer_ = buffer;
    descriptorCapacity_ = capacity;
    descriptorLength_ = initialLength;
    descriptorOverflowed_ = false;
    if (descriptorLength_ > capacity) {
        descriptorLength_ = capacity;
    }
}

size_t PluggableUSB_::endDescriptorBuild() {
    const size_t length = descriptorLength_;
    descriptorBuffer_ = nullptr;
    descriptorCapacity_ = 0U;
    descriptorLength_ = 0U;
    descriptorOverflowed_ = false;
    return length;
}

bool PluggableUSB_::appendDescriptor(const void *data, size_t length) {
    if (descriptorBuffer_ == nullptr || data == nullptr || length == 0U) {
        return false;
    }
    // Subtraction form also rejects SIZE_MAX-style lengths without allowing
    // the addition to wrap and turn an overflow into an out-of-bounds copy.
    if (descriptorLength_ > descriptorCapacity_ ||
        length > (descriptorCapacity_ - descriptorLength_)) {
        descriptorOverflowed_ = true;
        return false;
    }

    const uint8_t *source = reinterpret_cast<const uint8_t *>(data);
    for (size_t index = 0; index < length; ++index) {
        descriptorBuffer_[descriptorLength_ + index] = source[index];
    }
    descriptorLength_ += length;
    return true;
}

bool PluggableUSB_::send(PluggableUSBModule *module, const void *data, size_t length) {
    if (module == nullptr || !module->descriptorAdmitted) {
        return false;
    }
    return nrfUsbdDriver().sendInPacket(module->pluggedEndpoint, data, length);
}

void PluggableUSB_::endpointInComplete(uint8_t endpoint) {
    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
        if (!node->descriptorAdmitted) {
            continue;
        }
        const uint8_t firstEndpoint = node->pluggedEndpoint;
        const uint8_t lastEndpoint = static_cast<uint8_t>(firstEndpoint + node->numEndpoints);
        if (endpoint >= firstEndpoint && endpoint < lastEndpoint) {
            node->onEndpointComplete(endpoint);
            return;
        }
    }
}

size_t PluggableUSB_::descriptorLength() const {
    return descriptorLength_;
}

uint8_t PluggableUSB_::totalInterfaceCount() const {
    return lastInterface_;
}

uint8_t PluggableUSB_::nextEndpoint() const {
    return lastEndpoint_;
}

PluggableUSB_ &PluggableUSB() {
    return g_pluggableUsb;
}
