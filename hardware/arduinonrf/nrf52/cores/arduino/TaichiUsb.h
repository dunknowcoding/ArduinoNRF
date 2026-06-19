#pragma once
//
// TaichiUSB - the ArduinoNRF self-developed USB device stack for the nRF52840.
// =========================================================================
//
// TaichiUSB is a clean, from-scratch USB device stack written directly against
// the nRF USBD peripheral (EasyDMA endpoints, the errata-wrapped ENABLE
// handshake, VBUS/OUTPUTRDY sequencing, suspend/resume, and the 1200-bps-touch
// upload handoff). It is NOT TinyUSB and shares no code with it: enumeration is
// serviced from the USBD ISR so it keeps running no matter what the user
// sketch's setup()/loop() does, and the same routines are re-entrant from the
// foreground poll() path used by yield().
//
// The implementation lives in NrfUsbd.{h,cpp} (device core), NrfUsbSerial.cpp
// (user CDC), and NrfServiceSerial.cpp (maintenance/upload CDC). This header is
// the public identity + a thin, stable facade over that core, plus a build-time
// guard (below) that turns the single most painful misconfiguration into a
// clear compile error instead of a silently USB-less binary.
//
#include "NrfUsbd.h"

#define TAICHIUSB_VERSION_MAJOR 1
#define TAICHIUSB_VERSION_MINOR 0
#define TAICHIUSB_VERSION_PATCH 0
#define TAICHIUSB_VERSION "1.0.0"

// -------------------------------------------------------------------------
// Build-flag guard.
//
// On this core the per-board `build.extra_flags` is an AGGREGATE that carries
// the critical system defines, including the USB-CDC enable flag:
//
//   build.extra_flags = {build.base_flags} {build.system_flags}
//                       {build.usb_backend_flags} {build.usb_cdc_flags} ...
//
// `-DNRF_SYSTEM_HAS_USB_CDC=0|1` is supplied by the `usbcdc` menu via
// `build.usb_cdc_flags`. If a build OVERRIDES `build.extra_flags` wholesale
// (e.g. `arduino-cli --build-property build.extra_flags=-DMY_DEFINE`), every one
// of those aggregated defines is dropped. NRF_SYSTEM_HAS_USB_CDC then becomes
// UNDEFINED and the whole stack compiles itself out *silently* - the firmware
// runs but never enumerates a COM port. (This is a real bug we hit on the
// bench; it cost hours of "the boards won't enumerate" debugging.)
//
// `NRF52_SERIES` comes from `build.flags.common`, a SEPARATE property that an
// `build.extra_flags` override does NOT touch - so it is the reliable anchor
// for "we are in the Arduino/nRF build but the aggregated flags went missing".
// The `usbcdc` menu always defines NRF_SYSTEM_HAS_USB_CDC (to 0 when disabled,
// 1 when enabled), so an *undefined* state can only mean the flags were
// clobbered. Catch it loudly with an actionable message.
//
#if defined(NRF52_SERIES) && !defined(NRF_SYSTEM_HAS_USB_CDC)
#error "TaichiUSB: NRF_SYSTEM_HAS_USB_CDC is undefined - the board's build.extra_flags were overridden, dropping the USB-CDC (and other system) defines, which silently disables USB. Add your own -D defines via compiler.cpp.extra_flags / compiler.c.extra_flags (NOT build.extra_flags), or include {build.usb_cdc_flags} {build.system_flags} {build.usb_backend_flags} in your override. See TaichiUsb.h."
#endif

// -------------------------------------------------------------------------
// Public facade.
//
// Stable names for the TaichiUSB device core. The underlying class/accessor
// keep their historical Nrf* names for source compatibility; these aliases are
// the going-forward TaichiUSB-branded surface.
//
using TaichiUsbDriver = NrfUsbdDriver;
using TaichiUsbStatus = NrfUsbdStatus;
using TaichiUsbLineCoding = NrfUsbLineCoding;

// Accessor for the singleton device-core instance.
NrfUsbdDriver &nrfUsbdDriver();
inline TaichiUsbDriver &taichiUsb() { return nrfUsbdDriver(); }
