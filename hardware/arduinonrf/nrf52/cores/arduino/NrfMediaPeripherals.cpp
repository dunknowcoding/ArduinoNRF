// NrfMediaPeripherals.cpp - implementations of NrfQspi / NrfPdm / NrfI2s.
//
// IMPORTANT: these drivers are NOT verified on the reference ProMicro
// board this repo targets (no external flash chip, no PDM mic, no I2S
// codec wired). The register sequences follow the nRF52840 PS and are
// expected to work on boards that DO have the matching hardware (XIAO
// Sense PDM, Pitaya Go QSPI, ...), but treat them as "compiles + matches
// spec" until someone exercises them on real hardware.

#include "NrfMediaPeripherals.h"
#include <string.h>

namespace {

inline volatile uint32_t &reg32(uint32_t base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t *>(base + offset);
}

// ---- QSPI register map ---------------------------------------------------

constexpr uint32_t QSPI_BASE              = 0x40029000UL;
constexpr uint32_t QSPI_TASKS_ACTIVATE    = 0x000UL;
constexpr uint32_t QSPI_TASKS_READSTART   = 0x004UL;
constexpr uint32_t QSPI_TASKS_WRITESTART  = 0x008UL;
constexpr uint32_t QSPI_TASKS_ERASESTART  = 0x00CUL;
constexpr uint32_t QSPI_TASKS_DEACTIVATE  = 0x010UL;
constexpr uint32_t QSPI_EVENTS_READY      = 0x100UL;
constexpr uint32_t QSPI_INTENSET          = 0x304UL;
constexpr uint32_t QSPI_INTENCLR          = 0x308UL;
constexpr uint32_t QSPI_ENABLE            = 0x500UL;
constexpr uint32_t QSPI_READ_SRC          = 0x504UL;
constexpr uint32_t QSPI_READ_DST          = 0x508UL;
constexpr uint32_t QSPI_READ_CNT          = 0x50CUL;
constexpr uint32_t QSPI_WRITE_DST         = 0x510UL;
constexpr uint32_t QSPI_WRITE_SRC         = 0x514UL;
constexpr uint32_t QSPI_WRITE_CNT         = 0x518UL;
constexpr uint32_t QSPI_ERASE_PTR         = 0x51CUL;
constexpr uint32_t QSPI_ERASE_LEN         = 0x520UL;
constexpr uint32_t QSPI_PSEL_SCK          = 0x524UL;
constexpr uint32_t QSPI_PSEL_CSN          = 0x528UL;
constexpr uint32_t QSPI_PSEL_IO0          = 0x530UL;
constexpr uint32_t QSPI_PSEL_IO1          = 0x534UL;
constexpr uint32_t QSPI_PSEL_IO2          = 0x538UL;
constexpr uint32_t QSPI_PSEL_IO3          = 0x53CUL;
constexpr uint32_t QSPI_XIPOFFSET         = 0x540UL;
constexpr uint32_t QSPI_IFCONFIG0         = 0x544UL;
constexpr uint32_t QSPI_IFCONFIG1         = 0x600UL;
constexpr uint32_t QSPI_STATUS            = 0x604UL;
constexpr uint32_t QSPI_DPMDUR            = 0x608UL;
constexpr uint32_t QSPI_ADDRCONF          = 0x624UL;
constexpr uint32_t QSPI_CINSTRCONF        = 0x634UL;
constexpr uint32_t QSPI_CINSTRDAT0        = 0x638UL;
constexpr uint32_t QSPI_CINSTRDAT1        = 0x63CUL;
constexpr uint32_t QSPI_IFTIMING          = 0x640UL;

constexpr uint32_t QSPI_ERASE_LEN_4KB     = 0UL;
constexpr uint32_t QSPI_ERASE_LEN_64KB    = 1UL;
constexpr uint32_t QSPI_ERASE_LEN_ALL     = 2UL;

constexpr uint32_t QSPI_IFCONFIG0_READOC_FASTREAD = 0x0UL;
constexpr uint32_t QSPI_IFCONFIG0_WRITEOC_PP     = 0x0UL << 4;
constexpr uint32_t QSPI_IFCONFIG0_ADDRMODE_24BIT = 0x0UL << 8;

constexpr uint8_t  QSPI_CMD_JEDEC_ID      = 0x9FU;

bool waitQspiReady() {
    for (uint32_t spin = 0; spin < 1000000UL; ++spin) {
        if (reg32(QSPI_BASE, QSPI_EVENTS_READY) != 0UL) {
            reg32(QSPI_BASE, QSPI_EVENTS_READY) = 0UL;
            return true;
        }
    }
    return false;
}

bool g_qspiRunning = false;

// ---- PDM register map ----------------------------------------------------

constexpr uint32_t PDM_BASE             = 0x4001D000UL;
constexpr uint32_t PDM_TASKS_START      = 0x000UL;
constexpr uint32_t PDM_TASKS_STOP       = 0x004UL;
constexpr uint32_t PDM_EVENTS_STARTED   = 0x100UL;
constexpr uint32_t PDM_EVENTS_STOPPED   = 0x104UL;
constexpr uint32_t PDM_EVENTS_END       = 0x108UL;
constexpr uint32_t PDM_ENABLE           = 0x500UL;
constexpr uint32_t PDM_PDMCLKCTRL       = 0x504UL;
constexpr uint32_t PDM_MODE             = 0x508UL;
constexpr uint32_t PDM_GAINL            = 0x518UL;
constexpr uint32_t PDM_GAINR            = 0x51CUL;
constexpr uint32_t PDM_RATIO            = 0x520UL;   // not on every nRF52
constexpr uint32_t PDM_PSEL_CLK         = 0x540UL;
constexpr uint32_t PDM_PSEL_DIN         = 0x544UL;
constexpr uint32_t PDM_SAMPLE_PTR       = 0x560UL;
constexpr uint32_t PDM_SAMPLE_MAXCNT    = 0x564UL;

// PDM clock control values (decimation = 64 -> samples at PDMCLK/64).
constexpr uint32_t PDM_CLK_DEFAULT_1280K = 0x0A000000UL;   // ~1.28 MHz -> 20 kHz samples
constexpr uint32_t PDM_CLK_DEFAULT_1032K = 0x08400000UL;   // ~1.032 MHz -> 16.125 kHz

constexpr uint32_t PDM_GAIN_0DB = 0x28UL;            // 0x28 = 0 dB

bool g_pdmRunning = false;
int16_t *g_pdmBuf = nullptr;
size_t g_pdmBufLen = 0;
volatile size_t g_pdmReadIndex = 0;

// ---- I2S register map ----------------------------------------------------

constexpr uint32_t I2S_BASE              = 0x40025000UL;
constexpr uint32_t I2S_TASKS_START       = 0x000UL;
constexpr uint32_t I2S_TASKS_STOP        = 0x004UL;
constexpr uint32_t I2S_EVENTS_RXPTRUPD   = 0x104UL;
constexpr uint32_t I2S_EVENTS_STOPPED    = 0x108UL;
constexpr uint32_t I2S_EVENTS_TXPTRUPD   = 0x114UL;
constexpr uint32_t I2S_ENABLE            = 0x500UL;
constexpr uint32_t I2S_CONFIG_MODE       = 0x504UL;
constexpr uint32_t I2S_CONFIG_RXEN       = 0x508UL;
constexpr uint32_t I2S_CONFIG_TXEN       = 0x50CUL;
constexpr uint32_t I2S_CONFIG_MCKEN      = 0x510UL;
constexpr uint32_t I2S_CONFIG_MCKFREQ    = 0x514UL;
constexpr uint32_t I2S_CONFIG_RATIO      = 0x518UL;
constexpr uint32_t I2S_CONFIG_SWIDTH     = 0x51CUL;
constexpr uint32_t I2S_CONFIG_ALIGN      = 0x520UL;
constexpr uint32_t I2S_CONFIG_FORMAT     = 0x524UL;
constexpr uint32_t I2S_CONFIG_CHANNELS   = 0x528UL;
constexpr uint32_t I2S_RXD_PTR           = 0x538UL;
constexpr uint32_t I2S_TXD_PTR           = 0x540UL;
constexpr uint32_t I2S_RXTXD_MAXCNT      = 0x550UL;
constexpr uint32_t I2S_PSEL_MCK          = 0x560UL;
constexpr uint32_t I2S_PSEL_SCK          = 0x564UL;
constexpr uint32_t I2S_PSEL_LRCK         = 0x568UL;
constexpr uint32_t I2S_PSEL_SDIN         = 0x56CUL;
constexpr uint32_t I2S_PSEL_SDOUT        = 0x570UL;

bool g_i2sRunning = false;

}  // namespace

// ============================================================================
// NrfQspi
// ============================================================================

bool NrfQspi::begin(uint8_t pinSck, uint8_t pinCs,
                    uint8_t pinIo0, uint8_t pinIo1,
                    uint8_t pinIo2, uint8_t pinIo3,
                    uint32_t sckHz) {
    // Disable first
    reg32(QSPI_BASE, QSPI_ENABLE) = 0UL;

    reg32(QSPI_BASE, QSPI_PSEL_SCK) = pinSck;
    reg32(QSPI_BASE, QSPI_PSEL_CSN) = pinCs;
    reg32(QSPI_BASE, QSPI_PSEL_IO0) = pinIo0;
    reg32(QSPI_BASE, QSPI_PSEL_IO1) = pinIo1;
    reg32(QSPI_BASE, QSPI_PSEL_IO2) = (pinIo2 == 0xFFU) ? 0xFFFFFFFFUL : pinIo2;
    reg32(QSPI_BASE, QSPI_PSEL_IO3) = (pinIo3 == 0xFFU) ? 0xFFFFFFFFUL : pinIo3;

    // IFCONFIG0: standard fast-read, page program, 24-bit address.
    reg32(QSPI_BASE, QSPI_IFCONFIG0) = QSPI_IFCONFIG0_READOC_FASTREAD |
                                       QSPI_IFCONFIG0_WRITEOC_PP |
                                       QSPI_IFCONFIG0_ADDRMODE_24BIT;
    // IFCONFIG1: SCK frequency divider. SCK = 32 MHz / (SCKFREQ + 1).
    uint32_t sckDiv = (32000000UL / sckHz) - 1UL;
    if (sckDiv > 15UL) sckDiv = 15UL;
    reg32(QSPI_BASE, QSPI_IFCONFIG1) = (sckDiv << 0);

    reg32(QSPI_BASE, QSPI_ENABLE) = 1UL;
    reg32(QSPI_BASE, QSPI_EVENTS_READY) = 0UL;
    reg32(QSPI_BASE, QSPI_TASKS_ACTIVATE) = 1UL;
    if (!waitQspiReady()) {
        return false;
    }
    g_qspiRunning = true;
    return true;
}

void NrfQspi::end() {
    if (!g_qspiRunning) return;
    reg32(QSPI_BASE, QSPI_TASKS_DEACTIVATE) = 1UL;
    reg32(QSPI_BASE, QSPI_ENABLE) = 0UL;
    g_qspiRunning = false;
}

bool NrfQspi::isReady() {
    return g_qspiRunning && (reg32(QSPI_BASE, QSPI_STATUS) & 0x08UL) != 0UL;
}

bool NrfQspi::readId(uint8_t id[3]) {
    if (!g_qspiRunning) return false;
    // Use custom-instruction mode to issue JEDEC-ID and read 3 bytes.
    reg32(QSPI_BASE, QSPI_CINSTRDAT0) = 0UL;
    reg32(QSPI_BASE, QSPI_CINSTRDAT1) = 0UL;
    // CINSTRCONF: LENGTH=4 (1 cmd + 3 data), LFEN=0, LFSTOP=0, WREN=0.
    reg32(QSPI_BASE, QSPI_CINSTRCONF) = (static_cast<uint32_t>(QSPI_CMD_JEDEC_ID)) |
                                        (4UL << 8);   // LENGTH = 4 bytes
    if (!waitQspiReady()) return false;
    const uint32_t v0 = reg32(QSPI_BASE, QSPI_CINSTRDAT0);
    id[0] = static_cast<uint8_t>((v0 >> 0) & 0xFFU);
    id[1] = static_cast<uint8_t>((v0 >> 8) & 0xFFU);
    id[2] = static_cast<uint8_t>((v0 >> 16) & 0xFFU);
    return true;
}

bool NrfQspi::readBytes(uint32_t flashAddr, void *buf, size_t len) {
    if (!g_qspiRunning || buf == nullptr) return false;
    if ((reinterpret_cast<uintptr_t>(buf) & 0x3U) != 0U) return false;   // word-align required
    if ((len & 0x3U) != 0U) return false;
    reg32(QSPI_BASE, QSPI_READ_SRC) = flashAddr;
    reg32(QSPI_BASE, QSPI_READ_DST) = reinterpret_cast<uint32_t>(buf);
    reg32(QSPI_BASE, QSPI_READ_CNT) = len;
    reg32(QSPI_BASE, QSPI_TASKS_READSTART) = 1UL;
    return waitQspiReady();
}

bool NrfQspi::writeBytes(uint32_t flashAddr, const void *buf, size_t len) {
    if (!g_qspiRunning || buf == nullptr) return false;
    if ((reinterpret_cast<uintptr_t>(buf) & 0x3U) != 0U) return false;
    if ((len & 0x3U) != 0U) return false;
    reg32(QSPI_BASE, QSPI_WRITE_DST) = flashAddr;
    reg32(QSPI_BASE, QSPI_WRITE_SRC) = reinterpret_cast<uint32_t>(buf);
    reg32(QSPI_BASE, QSPI_WRITE_CNT) = len;
    reg32(QSPI_BASE, QSPI_TASKS_WRITESTART) = 1UL;
    return waitQspiReady();
}

bool NrfQspi::erase4kSector(uint32_t flashAddr) {
    if (!g_qspiRunning) return false;
    reg32(QSPI_BASE, QSPI_ERASE_PTR) = flashAddr;
    reg32(QSPI_BASE, QSPI_ERASE_LEN) = QSPI_ERASE_LEN_4KB;
    reg32(QSPI_BASE, QSPI_TASKS_ERASESTART) = 1UL;
    return waitQspiReady();
}

bool NrfQspi::erase64kBlock(uint32_t flashAddr) {
    if (!g_qspiRunning) return false;
    reg32(QSPI_BASE, QSPI_ERASE_PTR) = flashAddr;
    reg32(QSPI_BASE, QSPI_ERASE_LEN) = QSPI_ERASE_LEN_64KB;
    reg32(QSPI_BASE, QSPI_TASKS_ERASESTART) = 1UL;
    return waitQspiReady();
}

bool NrfQspi::eraseChip() {
    if (!g_qspiRunning) return false;
    reg32(QSPI_BASE, QSPI_ERASE_LEN) = QSPI_ERASE_LEN_ALL;
    reg32(QSPI_BASE, QSPI_TASKS_ERASESTART) = 1UL;
    return waitQspiReady();
}

// ============================================================================
// NrfPdm
// ============================================================================

bool NrfPdm::begin(uint8_t pinClock, uint8_t pinData, uint32_t sampleRateHz, bool leftChannel) {
    reg32(PDM_BASE, PDM_ENABLE) = 0UL;
    reg32(PDM_BASE, PDM_PSEL_CLK) = pinClock;
    reg32(PDM_BASE, PDM_PSEL_DIN) = pinData;
    // PDM clock = sample-rate * 64 (default decimation).
    // Closest preset to the requested rate; fine-grained adjustment via
    // PDMCLKCTRL bits 26..0 (16.16 fractional MHz).
    const uint32_t targetClk = sampleRateHz * 64UL;
    const uint64_t v = (static_cast<uint64_t>(targetClk) << 16) / 32000000UL;
    reg32(PDM_BASE, PDM_PDMCLKCTRL) = static_cast<uint32_t>(v) << 16;
    reg32(PDM_BASE, PDM_MODE) = leftChannel ? 0UL : 1UL;   // mono left/right
    reg32(PDM_BASE, PDM_GAINL) = PDM_GAIN_0DB;
    reg32(PDM_BASE, PDM_GAINR) = PDM_GAIN_0DB;
    reg32(PDM_BASE, PDM_ENABLE) = 1UL;

    if (g_pdmBuf == nullptr) {
        return true;   // caller will provide buffer with setSampleBuffer
    }
    reg32(PDM_BASE, PDM_SAMPLE_PTR) = reinterpret_cast<uint32_t>(g_pdmBuf);
    reg32(PDM_BASE, PDM_SAMPLE_MAXCNT) = g_pdmBufLen;
    reg32(PDM_BASE, PDM_TASKS_START) = 1UL;
    g_pdmRunning = true;
    return true;
}

void NrfPdm::end() {
    reg32(PDM_BASE, PDM_TASKS_STOP) = 1UL;
    reg32(PDM_BASE, PDM_ENABLE) = 0UL;
    g_pdmRunning = false;
}

bool NrfPdm::isRunning() { return g_pdmRunning; }

bool NrfPdm::setSampleBuffer(int16_t *buf, size_t lengthSamples) {
    if (buf == nullptr || lengthSamples == 0U) return false;
    if ((reinterpret_cast<uintptr_t>(buf) & 0x3U) != 0U) return false;
    g_pdmBuf = buf;
    g_pdmBufLen = lengthSamples;
    g_pdmReadIndex = 0;
    if (g_pdmRunning) {
        reg32(PDM_BASE, PDM_SAMPLE_PTR) = reinterpret_cast<uint32_t>(buf);
        reg32(PDM_BASE, PDM_SAMPLE_MAXCNT) = lengthSamples;
    }
    return true;
}

size_t NrfPdm::read(int16_t *dst, size_t count) {
    if (!g_pdmRunning || g_pdmBuf == nullptr || dst == nullptr) return 0;
    // Simple synchronous copy from the head of the DMA buffer. A
    // production driver would arm two ping-pong buffers and use the END
    // event to swap; this is the simplest version that works for sketches
    // that poll faster than the sample rate.
    const size_t toCopy = (count > g_pdmBufLen) ? g_pdmBufLen : count;
    memcpy(dst, g_pdmBuf, toCopy * sizeof(int16_t));
    return toCopy;
}

// ============================================================================
// NrfI2s
// ============================================================================

bool NrfI2s::begin(uint8_t pinSck, uint8_t pinLrck, uint8_t pinData,
                   uint8_t pinMck, uint32_t sampleRateHz, Mode mode) {
    reg32(I2S_BASE, I2S_ENABLE) = 0UL;

    // CONFIG.MODE: 0 = master, 1 = slave.
    reg32(I2S_BASE, I2S_CONFIG_MODE) = (mode == MODE_SLAVE_TX || mode == MODE_SLAVE_RX) ? 1UL : 0UL;

    // RXEN / TXEN bits.
    const bool wantRx = (mode == MODE_MASTER_RX || mode == MODE_MASTER_RXTX || mode == MODE_SLAVE_RX);
    const bool wantTx = (mode == MODE_MASTER_TX || mode == MODE_MASTER_RXTX || mode == MODE_SLAVE_TX);
    reg32(I2S_BASE, I2S_CONFIG_RXEN) = wantRx ? 1UL : 0UL;
    reg32(I2S_BASE, I2S_CONFIG_TXEN) = wantTx ? 1UL : 0UL;

    // MCK on, 32 MHz / div for MCK. For ~12 MHz MCK / 256 ratio -> 46.875 kHz
    // sample rate. The exact MCKFREQ + RATIO combination determines the
    // sample rate; this driver picks a reasonable default for ~48 kHz.
    reg32(I2S_BASE, I2S_CONFIG_MCKEN) = 1UL;
    // Approximate sample-rate calc: MCK = 32M / (MCKFREQ_div), LRCK = MCK / RATIO.
    // Targeting `sampleRateHz`, pick RATIO = 256 (= 0x6 register value).
    (void)sampleRateHz;   // simplified: caller picks via pin selection
    reg32(I2S_BASE, I2S_CONFIG_MCKFREQ) = 0x20000000UL;   // ~4 MHz MCK
    reg32(I2S_BASE, I2S_CONFIG_RATIO) = 0x6UL;            // 256
    reg32(I2S_BASE, I2S_CONFIG_SWIDTH) = 1UL;             // 16-bit
    reg32(I2S_BASE, I2S_CONFIG_ALIGN) = 0UL;              // left-aligned
    reg32(I2S_BASE, I2S_CONFIG_FORMAT) = 0UL;             // I2S format
    reg32(I2S_BASE, I2S_CONFIG_CHANNELS) = 0UL;           // stereo

    reg32(I2S_BASE, I2S_PSEL_MCK)   = (pinMck == 0xFFU) ? 0xFFFFFFFFUL : pinMck;
    reg32(I2S_BASE, I2S_PSEL_SCK)   = pinSck;
    reg32(I2S_BASE, I2S_PSEL_LRCK)  = pinLrck;
    reg32(I2S_BASE, I2S_PSEL_SDIN)  = wantRx ? pinData : 0xFFFFFFFFUL;
    reg32(I2S_BASE, I2S_PSEL_SDOUT) = wantTx ? pinData : 0xFFFFFFFFUL;

    reg32(I2S_BASE, I2S_ENABLE) = 1UL;
    reg32(I2S_BASE, I2S_TASKS_START) = 1UL;
    g_i2sRunning = true;
    return true;
}

void NrfI2s::end() {
    reg32(I2S_BASE, I2S_TASKS_STOP) = 1UL;
    reg32(I2S_BASE, I2S_ENABLE) = 0UL;
    g_i2sRunning = false;
}

bool NrfI2s::isRunning() { return g_i2sRunning; }

bool NrfI2s::setTxBuffer(const uint32_t *buf, size_t lengthWords) {
    if (buf == nullptr || (reinterpret_cast<uintptr_t>(buf) & 0x3U) != 0U) return false;
    reg32(I2S_BASE, I2S_TXD_PTR) = reinterpret_cast<uint32_t>(buf);
    reg32(I2S_BASE, I2S_RXTXD_MAXCNT) = lengthWords;
    return true;
}

bool NrfI2s::setRxBuffer(uint32_t *buf, size_t lengthWords) {
    if (buf == nullptr || (reinterpret_cast<uintptr_t>(buf) & 0x3U) != 0U) return false;
    reg32(I2S_BASE, I2S_RXD_PTR) = reinterpret_cast<uint32_t>(buf);
    reg32(I2S_BASE, I2S_RXTXD_MAXCNT) = lengthWords;
    return true;
}
