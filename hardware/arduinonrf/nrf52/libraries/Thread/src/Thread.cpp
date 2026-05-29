// Thread.cpp - stub implementation.
#include "Thread.h"
#include <string.h>

#ifndef NRF_THREAD_VENDORED
#define NRF_THREAD_VENDORED 0
#endif

#if NRF_THREAD_VENDORED
extern "C" {
extern Thread::Status nrfThread_begin(uint8_t role);
extern void            nrfThread_end();
extern Thread::Status nrfThread_joinNetwork(const char *name, const uint8_t xpan[8],
                                              const uint8_t key[16], uint8_t channel,
                                              uint16_t panId);
extern bool            nrfThread_isAttached();
extern Thread::Status nrfThread_onCoapGet(const char *path, Thread::coapHandler_t cb);
extern void            nrfThread_getEui64(uint8_t eui[8]);
}
#endif

namespace { bool g_started = false; }

Thread::Status Thread::begin(Role role) {
#if NRF_THREAD_VENDORED
    const Status r = nrfThread_begin(static_cast<uint8_t>(role));
    g_started = (r == THREAD_OK);
    return r;
#else
    (void)role;
    return THREAD_NOT_VENDORED;
#endif
}

void Thread::end() {
#if NRF_THREAD_VENDORED
    if (g_started) { nrfThread_end(); g_started = false; }
#endif
}

bool Thread::isAvailable() {
#if NRF_THREAD_VENDORED
    return g_started;
#else
    return false;
#endif
}

Thread::Status Thread::joinNetwork(const char *name, const uint8_t xpan[8],
                                    const uint8_t key[16], uint8_t channel,
                                    uint16_t panId) {
#if NRF_THREAD_VENDORED
    if (!g_started) return THREAD_NOT_STARTED;
    return nrfThread_joinNetwork(name, xpan, key, channel, panId);
#else
    (void)name; (void)xpan; (void)key; (void)channel; (void)panId;
    return THREAD_NOT_VENDORED;
#endif
}

bool Thread::isAttached() {
#if NRF_THREAD_VENDORED
    return g_started && nrfThread_isAttached();
#else
    return false;
#endif
}

Thread::Status Thread::onCoapGet(const char *path, coapHandler_t handler) {
#if NRF_THREAD_VENDORED
    if (!g_started) return THREAD_NOT_STARTED;
    return nrfThread_onCoapGet(path, handler);
#else
    (void)path; (void)handler;
    return THREAD_NOT_VENDORED;
#endif
}

void Thread::getEui64(uint8_t eui[8]) {
#if NRF_THREAD_VENDORED
    if (g_started) { nrfThread_getEui64(eui); return; }
#endif
    memset(eui, 0xFFU, 8);
}
