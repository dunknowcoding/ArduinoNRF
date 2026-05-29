// NrfMediaPeripherals.h - the three peripherals on nRF52840 that need
// real external hardware to verify and are big enough to warrant their
// own header:
//   * NrfQspi - Quad SPI master, intended for external NOR flash chips.
//                Up to 32 MHz, programmable instruction format.
//   * NrfPdm  - Pulse Density Modulation receiver for MEMS digital mics.
//                Mono, 16-bit PCM output, 16 kHz typical sample rate.
//   * NrfI2s  - I2S master/slave, stereo PCM up to 192 kHz.
//
// These compile and the register sequences match the Nordic PS; they are
// NOT hardware-verified in this revision because the verified ProMicro
// reference board has no external flash chip, MEMS mic, or I2S codec
// wired. Boards that do (XIAO nRF52840 Sense onboard PDM, Pitaya Go QSPI
// flash, custom audio boards) should work but will need their own
// verification pass.

#pragma once

#include <stdint.h>
#include <stddef.h>

// ---- NrfQspi - Quad SPI master for external flash --------------------------
//
// nRF52840's QSPI has up to 32 MHz SCK + a programmable instruction set
// (READ, WRITE, ERASE, READ ID, ...). Use it to talk to MX25R6435F / GD25Q64
// / W25Q128JV-class NOR flash chips.

class NrfQspi {
public:
    // Most common pin assignment on Adafruit Feather + Pitaya Go style
    // boards; pass 0xFF for IO2/IO3 if your board only wires a Dual SPI
    // path (the driver will fall back to 2-line mode).
    static bool begin(uint8_t pinSck, uint8_t pinCs,
                       uint8_t pinIo0, uint8_t pinIo1,
                       uint8_t pinIo2 = 0xFFU, uint8_t pinIo3 = 0xFFU,
                       uint32_t sckHz = 8000000UL);
    static void end();

    static bool isReady();

    // Read a 3-byte JEDEC ID (Manufacturer / Memory Type / Capacity).
    static bool readId(uint8_t id[3]);

    // Read / write / erase. Addresses are flash-internal (start at 0).
    static bool readBytes(uint32_t flashAddr, void *buf, size_t len);
    static bool writeBytes(uint32_t flashAddr, const void *buf, size_t len);
    static bool erase4kSector(uint32_t flashAddr);
    static bool erase64kBlock(uint32_t flashAddr);
    static bool eraseChip();
};

// ---- NrfPdm - PDM digital MEMS mic input ----------------------------------
//
// One channel (mono) driven by a PDM bit stream. The peripheral decimates +
// filters into 16-bit PCM samples at the configured rate (default 16 kHz).
// Output goes into a user-provided buffer via EasyDMA.

class NrfPdm {
public:
    static bool begin(uint8_t pinClock, uint8_t pinData,
                       uint32_t sampleRateHz = 16000UL,
                       bool leftChannel = true);
    static void end();
    static bool isRunning();

    // Provide the SAMPLE buffer the peripheral DMAs into. Must be word-aligned.
    // Length is in samples (each sample is one int16_t).
    static bool setSampleBuffer(int16_t *buf, size_t lengthSamples);

    // Read out up to `count` samples that have been latched since the last
    // call. Returns the number actually copied.
    static size_t read(int16_t *dst, size_t count);
};

// ---- NrfI2s - I2S audio (stereo PCM, master/slave) ------------------------

class NrfI2s {
public:
    enum Mode : uint8_t {
        MODE_MASTER_TX = 0,
        MODE_MASTER_RX = 1,
        MODE_MASTER_RXTX = 2,
        MODE_SLAVE_TX = 3,
        MODE_SLAVE_RX = 4,
    };

    static bool begin(uint8_t pinSck, uint8_t pinLrck, uint8_t pinData,
                       uint8_t pinMck, uint32_t sampleRateHz, Mode mode);
    static void end();
    static bool isRunning();

    // Hand a TX buffer to the peripheral. Must be word-aligned; length is in
    // 32-bit words (each word = 1 stereo sample for 16-bit, or part of a
    // sample for larger widths).
    static bool setTxBuffer(const uint32_t *buf, size_t lengthWords);
    static bool setRxBuffer(uint32_t *buf, size_t lengthWords);
};
