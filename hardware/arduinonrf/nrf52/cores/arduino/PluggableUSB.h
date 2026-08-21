#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct USBSetup {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
};

class NrfUsbdDriver;

class PluggableUSBModule {
public:
    PluggableUSBModule(uint8_t numEndpoints, uint8_t numInterfaces, const uint8_t *endpointTypes);
    virtual ~PluggableUSBModule() = default;

    virtual int getInterface(uint8_t *interfaceCount) = 0;
    // A matching control-IN request must append exactly the positive byte count
    // it returns. Return zero without appending when the request is not owned;
    // negative, overflowed, or mismatched responses are rejected transactionally.
    virtual int getDescriptor(USBSetup &setup) = 0;
    virtual int getSetupResponse(USBSetup &setup);
    // A nonzero-length control-OUT transfer is two-phase. claimControlOut()
    // must be a side-effect-free ownership query; the core accepts the
    // request only when exactly one admitted module claims it. The bounded
    // payload is delivered atomically to completeControlOut() only after every
    // packet has arrived. Returning false from completion stalls the status
    // stage, so malformed payloads cannot be acknowledged.
    virtual bool claimControlOut(const USBSetup &setup);
    virtual bool completeControlOut(const USBSetup &setup, const uint8_t *data, size_t length);
    virtual bool setup(USBSetup &setup) = 0;
    virtual uint8_t getShortName(char *name);
    virtual void onEndpointComplete(uint8_t endpoint);

protected:
    bool appendDescriptor(const void *data, size_t length);
    bool send(const void *data, size_t length);

public:
    PluggableUSBModule *next = nullptr;
    uint8_t pluggedInterface = 0U;
    uint8_t pluggedEndpoint = 0U;
    uint8_t numEndpoints = 0U;
    uint8_t numInterfaces = 0U;
    const uint8_t *endpointType = nullptr;
    // Set only after one complete, internally consistent descriptor
    // transaction. A rejected module remains linked for diagnostics but cannot
    // receive setup/data callbacks or corrupt the base CDC configuration.
    bool descriptorAdmitted = false;
};

class PluggableUSB_ {
public:
    bool plug(PluggableUSBModule *module);
    int getInterface(uint8_t *interfaceCount);
    int getDescriptor(USBSetup &setup);
    int getSetupResponse(USBSetup &setup);
    bool setup(USBSetup &setup);
    uint8_t getShortName(char *name);
    bool appendDescriptor(const void *data, size_t length);
    bool send(PluggableUSBModule *module, const void *data, size_t length);
    void endpointInComplete(uint8_t endpoint);
    size_t descriptorLength() const;
    uint8_t totalInterfaceCount() const;
    uint8_t nextEndpoint() const;

private:
    friend class NrfUsbdDriver;
    // Only the controller driver may open/close a bounded response transaction;
    // modules can contribute bytes solely through appendDescriptor().
    PluggableUSBModule *controlOutOwner(const USBSetup &setup);
    bool completeControlOut(PluggableUSBModule *owner, const USBSetup &setup,
                            const uint8_t *data, size_t length);
    void beginDescriptorBuild(uint8_t *buffer, size_t capacity, size_t initialLength = 0U);
    size_t endDescriptorBuild();

    PluggableUSBModule *rootNode_ = nullptr;
    uint8_t lastInterface_ = 3U;
    uint8_t lastEndpoint_ = 3U;
    uint8_t *descriptorBuffer_ = nullptr;
    size_t descriptorCapacity_ = 0U;
    size_t descriptorLength_ = 0U;
    bool descriptorOverflowed_ = false;
};

PluggableUSB_ &PluggableUSB();
