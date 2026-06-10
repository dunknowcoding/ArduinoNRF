// Thread.h - compatibility shim.
//
// ArduinoNRF's Thread (OpenThread) support moved into its own library,
// NiusThread: https://github.com/dunknowcoding/ArduinoNRF-Thread
//
// This header keeps existing `#include <Thread.h>` sketches building by
// forwarding to NiusThread (same ThreadClass API, same global `Thread`
// object, plus the official OpenThread C API underneath).
//
// If the next line fails with "NiusThread.h: No such file or directory",
// install the NiusThread library from the Arduino Library Manager or from
// https://github.com/dunknowcoding/ArduinoNRF-Thread and build again.
#pragma once

#include <NiusThread.h>
