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
    int total = 0;
    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
        total += node->getInterface(interfaceCount);
    }
    return total;
}

int PluggableUSB_::getDescriptor(USBSetup &setup) {
    int total = 0;
    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
        total += node->getDescriptor(setup);
    }
    return total;
}

int PluggableUSB_::getSetupResponse(USBSetup &setup) {
    int total = 0;
    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
        total += node->getSetupResponse(setup);
        if (total > 0) {
            return total;
        }
    }
    return 0;
}

bool PluggableUSB_::setup(USBSetup &setup) {
    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
        if (node->setup(setup)) {
            return true;
        }
    }
    return false;
}

uint8_t PluggableUSB_::getShortName(char *name) {
    uint8_t length = 0U;
    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
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
    if (descriptorLength_ > capacity) {
        descriptorLength_ = capacity;
    }
}

size_t PluggableUSB_::endDescriptorBuild() {
    const size_t length = descriptorLength_;
    descriptorBuffer_ = nullptr;
    descriptorCapacity_ = 0U;
    descriptorLength_ = 0U;
    return length;
}

bool PluggableUSB_::appendDescriptor(const void *data, size_t length) {
    if (descriptorBuffer_ == nullptr || data == nullptr || length == 0U) {
        return false;
    }
    if ((descriptorLength_ + length) > descriptorCapacity_) {
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
    if (module == nullptr) {
        return false;
    }
    return nrfUsbdDriver().sendInPacket(module->pluggedEndpoint, data, length);
}

void PluggableUSB_::endpointInComplete(uint8_t endpoint) {
    for (PluggableUSBModule *node = rootNode_; node != nullptr; node = node->next) {
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
