// Thread.cpp - Arduino wrapper around the vendored OpenThread core.

#include "Thread.h"

#include <string.h>

extern "C" {
#include <openthread/error.h>
#include <openthread/instance.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>
#include <openthread/dataset.h>
#include <openthread/ip6.h>
#include <openthread/platform/radio.h>
}

#include "ot_platform_arduino.h"

ThreadClass Thread;

namespace {

otInstance *sInstance;

} // namespace

ThreadClass::Status ThreadClass::begin()
{
    if (sInstance != nullptr)
    {
        return THREAD_OK;
    }

    nrfOtPlatformInit();

    sInstance = otInstanceInitSingle();

    return (sInstance != nullptr) ? THREAD_OK : THREAD_INTERNAL;
}

void ThreadClass::end()
{
    if (sInstance != nullptr)
    {
        otThreadSetEnabled(sInstance, false);
        otIp6SetEnabled(sInstance, false);
        otInstanceFinalize(sInstance);
        sInstance = nullptr;
    }
}

bool ThreadClass::isAvailable() { return sInstance != nullptr; }

ThreadClass::Status ThreadClass::setNetwork(const char   *networkName,
                                            uint8_t       channel,
                                            uint16_t      panId,
                                            const uint8_t networkKey[16],
                                            const uint8_t extendedPanId[8])
{
    static const uint8_t kDefaultExtPanId[8] = {0xAD, 0x0E, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};

    if (sInstance == nullptr)
    {
        return THREAD_NOT_STARTED;
    }
    if (networkName == nullptr || networkKey == nullptr || channel < 11 || channel > 26 ||
        panId == 0xFFFF || strlen(networkName) > 16)
    {
        return THREAD_BAD_PARAM;
    }

    otOperationalDataset dataset;

    memset(&dataset, 0, sizeof(dataset));

    dataset.mActiveTimestamp.mSeconds       = 1;
    dataset.mComponents.mIsActiveTimestampPresent = true;

    strncpy(dataset.mNetworkName.m8, networkName, sizeof(dataset.mNetworkName.m8) - 1);
    dataset.mComponents.mIsNetworkNamePresent = true;

    memcpy(dataset.mNetworkKey.m8, networkKey, 16);
    dataset.mComponents.mIsNetworkKeyPresent = true;

    memcpy(dataset.mExtendedPanId.m8, (extendedPanId != nullptr) ? extendedPanId : kDefaultExtPanId, 8);
    dataset.mComponents.mIsExtendedPanIdPresent = true;

    // Fixed mesh-local ULA prefix; both nodes must agree (it travels in the
    // dataset anyway once a real commissioner is involved).
    static const uint8_t kMlPrefix[8] = {0xFD, 0x00, 0x0D, 0xB8, 0x00, 0x00, 0x00, 0x00};
    memcpy(dataset.mMeshLocalPrefix.m8, kMlPrefix, 8);
    dataset.mComponents.mIsMeshLocalPrefixPresent = true;

    dataset.mPanId = panId;
    dataset.mComponents.mIsPanIdPresent = true;

    dataset.mChannel = channel;
    dataset.mComponents.mIsChannelPresent = true;

    return (otDatasetSetActive(sInstance, &dataset) == OT_ERROR_NONE) ? THREAD_OK : THREAD_INTERNAL;
}

ThreadClass::Status ThreadClass::start()
{
    if (sInstance == nullptr)
    {
        return THREAD_NOT_STARTED;
    }
    if (otIp6SetEnabled(sInstance, true) != OT_ERROR_NONE)
    {
        return THREAD_INTERNAL;
    }
    return (otThreadSetEnabled(sInstance, true) == OT_ERROR_NONE) ? THREAD_OK : THREAD_INTERNAL;
}

ThreadClass::Status ThreadClass::stop()
{
    if (sInstance == nullptr)
    {
        return THREAD_NOT_STARTED;
    }
    otThreadSetEnabled(sInstance, false);
    otIp6SetEnabled(sInstance, false);
    return THREAD_OK;
}

void ThreadClass::process()
{
    if (sInstance == nullptr)
    {
        return;
    }

    nrfOtPlatformProcess(sInstance);
    nrfOtRadioProcess(sInstance);

    while (otTaskletsArePending(sInstance))
    {
        otTaskletsProcess(sInstance);
    }
}

ThreadClass::Role ThreadClass::role()
{
    if (sInstance == nullptr)
    {
        return ROLE_DISABLED;
    }
    return (Role)otThreadGetDeviceRole(sInstance);
}

const char *ThreadClass::roleString()
{
    switch (role())
    {
    case ROLE_DISABLED: return "disabled";
    case ROLE_DETACHED: return "detached";
    case ROLE_CHILD:    return "child";
    case ROLE_ROUTER:   return "router";
    case ROLE_LEADER:   return "leader";
    }
    return "?";
}

bool ThreadClass::isAttached() { return role() >= ROLE_CHILD; }

uint16_t ThreadClass::rloc16()
{
    return (sInstance != nullptr) ? otThreadGetRloc16(sInstance) : 0xFFFE;
}

void ThreadClass::getEui64(uint8_t eui[8])
{
    if (eui == nullptr)
    {
        return;
    }
    if (sInstance == nullptr)
    {
        memset(eui, 0xFF, 8);
        return;
    }
    otPlatRadioGetIeeeEui64(sInstance, eui);
}

otInstance *ThreadClass::instance() { return sInstance; }
