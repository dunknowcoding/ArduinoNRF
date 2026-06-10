// arduino-ot-config.h - OpenThread project core config for the ArduinoNRF
// nRF52840 port. Pulled in by openthread-core-config.h through the official
// OPENTHREAD_PROJECT_CORE_CONFIG_FILE hook (see the ARDUINONRF-PATCH there).
//
// Arduino's build model cannot pass per-library -D flags, so everything that
// an OpenThread platform normally sets on the compiler command line lives
// here instead. Every core source includes openthread-core-config.h before
// any config macro is evaluated, so this header is seen first everywhere.
#ifndef ARDUINO_OT_CONFIG_H_
#define ARDUINO_OT_CONFIG_H_

// ---- Device type ------------------------------------------------------------
// FTD so a single board can become Leader / Router and parent test children.
// (MTD/SED behavior is still reachable at runtime via otThreadSetLinkMode.)
#if !defined(OPENTHREAD_FTD) && !defined(OPENTHREAD_MTD) && !defined(OPENTHREAD_RADIO)
#define OPENTHREAD_FTD 1
#endif

#define OPENTHREAD_CONFIG_PLATFORM_INFO "ArduinoNRF-nRF52840"

// Normally injected by OpenThread's build system; used by otGetVersionString.
#define PACKAGE_NAME "OPENTHREAD"
#define PACKAGE_VERSION "fa3213ec-arduinonrf"

// ---- Radio driver contract ----------------------------------------------------
// Our register-level RADIO driver advertises OT_RADIO_CAPS_NONE, so the MAC
// sublayer must do CSMA backoff, retransmit, ack timeout, and TX security in
// software. The driver itself performs one CCA per transmit attempt and the
// imm-ack TX/RX handling.
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_CSMA_BACKOFF_ENABLE 1
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_RETRANSMIT_ENABLE 1
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_ACK_TIMEOUT_ENABLE 1
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_SECURITY_ENABLE 1
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_ENERGY_SCAN_ENABLE 1

// TIMER3 gives a 1 MHz tick, so the sub-MAC can run its ack/CSMA timing on
// the microsecond timer instead of the 1 kHz RTC.
#define OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE 1

// No CSL receiver: imm-acks only, no 802.15.4-2015 enh-ack scheduling.
#define OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE 0

// ---- Feature trim -------------------------------------------------------------
// Bring-up scope: MLE + mesh forwarding + MeshCoP dataset handling. The
// infrastructure-facing services stay off until there is a host interface.
#define OPENTHREAD_CONFIG_TCP_ENABLE 0
#define OPENTHREAD_CONFIG_COAP_API_ENABLE 1

// ---- Logging ------------------------------------------------------------------
// Routed to Serial by otPlatLog() in platform_impl.cpp. NOTE keeps role
// transitions and attach failures visible without the INFO firehose.
#define OPENTHREAD_CONFIG_LOG_OUTPUT OPENTHREAD_CONFIG_LOG_OUTPUT_PLATFORM_DEFINED
#define OPENTHREAD_CONFIG_LOG_LEVEL OT_LOG_LEVEL_NOTE

#endif // ARDUINO_OT_CONFIG_H_
