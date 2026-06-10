// ot_radio_nrf52840.cpp - OpenThread radio platform (otPlatRadio*) on the
// nRF52840's own RADIO peripheral in IEEE 802.15.4 (250 kbit) mode.
//
// The register recipe (MODE/PCNF/CRC/SFD/FREQUENCY) is the configuration that
// was hardware-verified bidirectionally against a CC2530 sniffer/transceiver
// on 2026-06-07. On top of it this driver adds what OpenThread needs:
//
//   * IRQ-driven RX into a small ring of frame slots, with software address
//     filtering (the nRF 802.15.4 mode has no hardware address filter).
//   * Immediate-ack TX for received unicast frames with the AR bit, with the
//     frame-pending bit driven by the OT source-match table.
//   * TX with a single hardware CCA attempt per otPlatRadioTransmit() call -
//     the radio advertises OT_RADIO_CAPS_NONE so the OT sub-MAC does CSMA
//     backoff, retransmits, ack timeout and TX security in software.
//   * ISR -> thread handoff: the ISR only moves bytes and flips slot states;
//     otPlatRadioReceiveDone()/otPlatRadioTxDone() run from nrfOtRadioProcess()
//     in the main loop (the OT core is not ISR-safe).
//
// Ack-timeout contract (matches the simulation platform): the driver reports
// TxDone only for "ack received", "tx done, no ack wanted" and "CCA busy".
// It NEVER reports NO_ACK - the sub-MAC's software ack timer owns that. A
// stale ack-wait is cancelled by the next Transmit()/Receive() call.

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "openthread-core-config.h"

extern "C" {
#include <openthread/platform/radio.h>
#include <openthread/platform/time.h>
}

#include "../../../cores/arduino/NrfPeripherals.h"   // NrfTimer (TIMER3 = us clock)
#include "ot_platform_arduino.h"

// ---------------------------------------------------------------------------
// Register access (offsets verified against the vendored nRF52840 MDK header)
// ---------------------------------------------------------------------------

#define RADIO_REG(o) (*(volatile uint32_t *)(0x40001000UL + (o)))
#define CLOCK_REG(o) (*(volatile uint32_t *)(0x40000000UL + (o)))

// RADIO tasks
#define R_TASKS_TXEN       0x000
#define R_TASKS_RXEN       0x004
#define R_TASKS_START      0x008
#define R_TASKS_STOP       0x00C
#define R_TASKS_DISABLE    0x010
#define R_TASKS_RSSISTART  0x014
#define R_TASKS_CCASTART   0x02C
#define R_TASKS_CCASTOP    0x030
// RADIO events
#define R_EVENTS_READY     0x100
#define R_EVENTS_END       0x10C
#define R_EVENTS_DISABLED  0x110
#define R_EVENTS_RSSIEND   0x11C
#define R_EVENTS_CRCOK     0x130
#define R_EVENTS_CRCERROR  0x134
#define R_EVENTS_CCAIDLE   0x144
#define R_EVENTS_CCABUSY   0x148
#define R_EVENTS_TXREADY   0x154
#define R_EVENTS_RXREADY   0x158
// RADIO registers
#define R_SHORTS           0x200
#define R_INTENSET         0x304
#define R_INTENCLR         0x308
#define R_CRCSTATUS        0x400
#define R_PACKETPTR        0x504
#define R_FREQUENCY        0x508
#define R_TXPOWER          0x50C
#define R_MODE             0x510
#define R_PCNF0            0x514
#define R_PCNF1            0x518
#define R_CRCCNF           0x534
#define R_CRCPOLY          0x538
#define R_CRCINIT          0x53C
#define R_RSSISAMPLE       0x548
#define R_STATE            0x550
#define R_SFD              0x660
#define R_CCACTRL          0x66C

// SHORTS bits
#define SH_DISABLED_TXEN   (1UL << 2)
#define SH_DISABLED_RXEN   (1UL << 3)
#define SH_ADDRESS_RSSISTART (1UL << 4)
#define SH_TXREADY_START   (1UL << 18)
#define SH_RXREADY_START   (1UL << 19)

// INTEN bits
#define INT_END            (1UL << 3)
#define INT_CRCERROR       (1UL << 13)
#define INT_CCAIDLE        (1UL << 17)
#define INT_CCABUSY        (1UL << 18)

// NVIC, RADIO = IRQ 1
#define NVIC_ISER          (*(volatile uint32_t *)0xE000E100UL)
#define NVIC_ICER          (*(volatile uint32_t *)0xE000E180UL)
#define NVIC_IPR1_B1       (*(volatile uint8_t *)(0xE000E400UL + 1U))
#define RADIO_IRQ_BIT      (1UL << 1)

// CLOCK
#define C_TASKS_HFCLKSTART 0x000
#define C_EVENTS_HFCLKSTARTED 0x100

// ---------------------------------------------------------------------------
// MAC frame constants
// ---------------------------------------------------------------------------

#define FCF_TYPE_MASK     0x0007
#define FCF_TYPE_DATA     0x0001
#define FCF_TYPE_ACK      0x0002
#define FCF_TYPE_CMD      0x0003
#define FCF_FRAME_PENDING 0x0010
#define FCF_ACK_REQUEST   0x0020
#define FCF_PANID_COMP    0x0040
#define FCF_DST_MASK      0x0C00
#define FCF_DST_NONE      0x0000
#define FCF_DST_SHORT     0x0800
#define FCF_DST_EXT       0x0C00
#define FCF_SRC_MASK      0xC000
#define FCF_SRC_NONE      0x0000
#define FCF_SRC_SHORT     0x8000
#define FCF_SRC_EXT       0xC000

#define MAC_BCAST_ADDR    0xFFFF
#define MAC_BCAST_PANID   0xFFFF

#define PHR_SIZE          1
#define FCS_SIZE          2
#define MAX_PSDU          127

// ---------------------------------------------------------------------------
// Driver state
// ---------------------------------------------------------------------------

enum RadioState : uint8_t
{
    kStateDisabled = 0,
    kStateSleep,
    kStateRx,         // receiving (also the resting state)
    kStateCca,        // CCA running, TX armed on idle
    kStateTx,         // transmitting the outgoing frame
    kStateAckTx,      // transmitting an immediate ack for a received frame
};

enum SlotState : uint8_t
{
    kSlotFree = 0,
    kSlotRadio,       // owned by the RADIO DMA right now
    kSlotReady,       // CRC-ok frame waiting for nrfOtRadioProcess()
};

struct RxSlot
{
    uint8_t  buf[PHR_SIZE + MAX_PSDU + 1]; // PHR + PSDU (FCS bytes not stored)
    volatile SlotState state;
    int8_t   rssi;
    uint8_t  lqi;
    uint64_t timestamp;
    bool     ackedWithFp;
};

enum TxEvent : uint8_t
{
    kTxEventNone = 0,
    kTxEventDone,       // sent, no ack requested
    kTxEventAcked,      // sent, matching imm-ack captured in sAckRxBuf
    kTxEventCcaBusy,    // CCA reported busy
};

#define RX_SLOT_COUNT 4

static RxSlot   sRxSlots[RX_SLOT_COUNT];
static volatile uint8_t sCurrentSlot;        // slot the RADIO DMA writes into

static RadioState volatile sState = kStateDisabled;
static bool     sEnabled;
static bool     sPromiscuous;
static uint8_t  sChannel = 11;
static int8_t   sTxPower;
static int8_t   sCcaThresholdDbm = -75;

static uint16_t sPanId  = MAC_BCAST_PANID;
static uint16_t sShortAddr = MAC_BCAST_ADDR;
static uint8_t  sExtAddr[8];                 // little-endian, as on air

// TX
static uint8_t  sTxBuf[PHR_SIZE + MAX_PSDU]; // PHR + PSDU (FCS appended by HW)
static otRadioFrame sTxFrame;                // handed to the OT core
static uint8_t  sTxPsdu[MAX_PSDU];
static volatile bool sAckWait;               // TX done, waiting for imm-ack
static volatile uint8_t sAckWaitSeq;
static volatile TxEvent sTxEvent = kTxEventNone;
static uint8_t  sAckRxBuf[PHR_SIZE + MAX_PSDU + 1]; // captured ack for TxDone
static otRadioFrame sAckRxFrame;
static volatile bool sTxPending;             // Transmit() accepted, TxDone owed

// immediate ack we transmit: PHR + FCF(2) + seq
static uint8_t  sAckTxBuf[PHR_SIZE + 3];

// source match (drives the frame-pending bit in our acks)
#define SRC_MATCH_SHORT_COUNT 16
#define SRC_MATCH_EXT_COUNT   8
static bool     sSrcMatchEnabled;
static uint16_t sSrcMatchShort[SRC_MATCH_SHORT_COUNT];
static uint8_t  sSrcMatchShortCount;
static uint8_t  sSrcMatchExt[SRC_MATCH_EXT_COUNT][8];
static uint8_t  sSrcMatchExtCount;

// IEEE 802.15.4 energy-detect threshold register granularity: EDSAMPLE/CCA ED
// values are roughly dBm + 94 (0 == -94 dBm, the radio's measurement floor).
#define ED_FLOOR_DBM (-94)

static inline uint32_t criticalEnter(void)
{
    uint32_t primask;
    __asm volatile("mrs %0, primask" : "=r"(primask));
    __asm volatile("cpsid i" ::: "memory");
    return primask;
}

static inline void criticalExit(uint32_t primask)
{
    if ((primask & 1U) == 0U)
    {
        __asm volatile("cpsie i" ::: "memory");
    }
}

static inline uint64_t radioTimeUs(void) { return (uint64_t)nrfTimer3().counter(); }

// ---------------------------------------------------------------------------
// Low-level helpers (callable from ISR and, inside critical sections, thread)
// ---------------------------------------------------------------------------

static void radioApplyChannel(void)
{
    RADIO_REG(R_FREQUENCY) = (uint32_t)(5U * (sChannel - 10U));
}

static void radioApplyTxPower(void)
{
    RADIO_REG(R_TXPOWER) = (uint32_t)(uint8_t)sTxPower;
}

static uint8_t findFreeSlot(void)
{
    for (uint8_t i = 0; i < RX_SLOT_COUNT; i++)
    {
        if (sRxSlots[i].state == kSlotFree)
        {
            return i;
        }
    }
    return sCurrentSlot; // ring exhausted: overwrite the in-flight slot
}

// Disable -> RXEN -> START chain; leaves the radio receiving into a slot.
static void radioEnterRx(void)
{
    uint8_t slot = findFreeSlot();

    sCurrentSlot = slot;
    sRxSlots[slot].state = kSlotRadio;

    RADIO_REG(R_PACKETPTR) = (uint32_t)sRxSlots[slot].buf;
    RADIO_REG(R_EVENTS_END) = 0;
    RADIO_REG(R_EVENTS_CRCOK) = 0;
    RADIO_REG(R_EVENTS_CRCERROR) = 0;
    RADIO_REG(R_EVENTS_DISABLED) = 0;
    RADIO_REG(R_EVENTS_RXREADY) = 0;
    RADIO_REG(R_SHORTS) = SH_DISABLED_RXEN | SH_RXREADY_START | SH_ADDRESS_RSSISTART;
    RADIO_REG(R_TASKS_DISABLE) = 1;
    sState = kStateRx;
}

static void radioEnterSleep(void)
{
    RADIO_REG(R_SHORTS) = 0;
    RADIO_REG(R_TASKS_CCASTOP) = 1;
    RADIO_REG(R_TASKS_DISABLE) = 1;
    if (sCurrentSlot < RX_SLOT_COUNT && sRxSlots[sCurrentSlot].state == kSlotRadio)
    {
        sRxSlots[sCurrentSlot].state = kSlotFree;
    }
    sState = kStateSleep;
}

// Parse enough of the MHR to (a) know whether the frame is for us and
// (b) find the source address for the frame-pending decision. Returns false
// for frames the filter should drop (when not promiscuous).
struct FrameInfo
{
    uint16_t fcf;
    uint8_t  seq;
    bool     dstIsUs;     // unicast match on short or extended address
    bool     dstIsBcast;
    uint8_t  srcMode;     // FCF_SRC_* value
    uint16_t srcShort;
    const uint8_t *srcExt;
};

static bool parseFrame(const uint8_t *psdu, uint8_t length, FrameInfo &info)
{
    if (length < 3)
    {
        return false;
    }

    info.fcf  = (uint16_t)(psdu[0] | ((uint16_t)psdu[1] << 8));
    info.seq  = psdu[2];
    info.dstIsUs = false;
    info.dstIsBcast = false;
    info.srcMode = 0;
    info.srcShort = 0;
    info.srcExt = NULL;

    uint8_t  pos = 3;
    uint16_t dstMode = info.fcf & FCF_DST_MASK;
    uint16_t srcMode = info.fcf & FCF_SRC_MASK;

    if (dstMode != FCF_DST_NONE)
    {
        if (pos + 2 > length)
        {
            return false;
        }
        uint16_t dstPan = (uint16_t)(psdu[pos] | ((uint16_t)psdu[pos + 1] << 8));
        pos += 2;

        bool panOk = (dstPan == sPanId) || (dstPan == MAC_BCAST_PANID);

        if (dstMode == FCF_DST_SHORT)
        {
            if (pos + 2 > length)
            {
                return false;
            }
            uint16_t dst = (uint16_t)(psdu[pos] | ((uint16_t)psdu[pos + 1] << 8));
            pos += 2;
            info.dstIsBcast = (dst == MAC_BCAST_ADDR) && panOk;
            info.dstIsUs    = (dst == sShortAddr) && panOk;
        }
        else if (dstMode == FCF_DST_EXT)
        {
            if (pos + 8 > length)
            {
                return false;
            }
            info.dstIsUs = panOk && (memcmp(&psdu[pos], sExtAddr, 8) == 0);
            pos += 8;
        }
        else
        {
            return false; // reserved dst mode
        }
    }

    if (srcMode != FCF_SRC_NONE)
    {
        if ((info.fcf & FCF_PANID_COMP) == 0)
        {
            if (pos + 2 > length)
            {
                return false;
            }
            pos += 2; // src PAN id - not used by the filter
        }
        if (srcMode == FCF_SRC_SHORT)
        {
            if (pos + 2 > length)
            {
                return false;
            }
            info.srcShort = (uint16_t)(psdu[pos] | ((uint16_t)psdu[pos + 1] << 8));
        }
        else if (srcMode == FCF_SRC_EXT)
        {
            if (pos + 8 > length)
            {
                return false;
            }
            info.srcExt = &psdu[pos];
        }
        else
        {
            return false;
        }
        info.srcMode = (uint8_t)(srcMode >> 14);
    }

    return true;
}

// Frame-pending decision for the ack we are about to send. With source
// matching disabled OT expects "pending" for everyone (conservative default
// while the parent has queued indirect frames).
static bool srcMatchHasPending(const FrameInfo &info)
{
    bool pending = true;

    if (!sSrcMatchEnabled)
    {
        return true;
    }

    pending = false;

    if (info.srcMode == (uint8_t)(FCF_SRC_SHORT >> 14))
    {
        for (uint8_t i = 0; i < sSrcMatchShortCount; i++)
        {
            if (sSrcMatchShort[i] == info.srcShort)
            {
                pending = true;
                break;
            }
        }
    }
    else if (info.srcExt != NULL)
    {
        for (uint8_t i = 0; i < sSrcMatchExtCount; i++)
        {
            if (memcmp(sSrcMatchExt[i], info.srcExt, 8) == 0)
            {
                pending = true;
                break;
            }
        }
    }

    return pending;
}

static int8_t lastRssiDbm(void)
{
    return (int8_t)(-(int32_t)(RADIO_REG(R_RSSISAMPLE) & 0x7FU));
}

static uint8_t rssiToLqi(int8_t rssi)
{
    // Linear map of the useful RSSI window (-100..-40 dBm) onto 0..255.
    int32_t lqi = ((int32_t)rssi + 100) * 255 / 60;
    if (lqi < 0)
    {
        lqi = 0;
    }
    if (lqi > 255)
    {
        lqi = 255;
    }
    return (uint8_t)lqi;
}

// ---------------------------------------------------------------------------
// RADIO interrupt
// ---------------------------------------------------------------------------

extern "C" void RADIO_IRQHandler(void)
{
    // CCA outcome (TX armed via SHORTS on idle; busy needs cleanup here).
    if (RADIO_REG(R_EVENTS_CCABUSY) != 0)
    {
        RADIO_REG(R_EVENTS_CCABUSY) = 0;
        RADIO_REG(R_EVENTS_CCAIDLE) = 0;
        if (sState == kStateCca)
        {
            sTxEvent = kTxEventCcaBusy;
            radioEnterRx();
        }
    }

    if (RADIO_REG(R_EVENTS_CCAIDLE) != 0)
    {
        RADIO_REG(R_EVENTS_CCAIDLE) = 0;
        if (sState == kStateCca)
        {
            // Channel clear: hop RX -> DISABLED -> TX, START on ready.
            RADIO_REG(R_PACKETPTR) = (uint32_t)sTxBuf;
            RADIO_REG(R_EVENTS_END) = 0;
            RADIO_REG(R_EVENTS_DISABLED) = 0;
            RADIO_REG(R_SHORTS) = SH_DISABLED_TXEN | SH_TXREADY_START;
            RADIO_REG(R_TASKS_DISABLE) = 1;
            sState = kStateTx;
        }
    }

    if (RADIO_REG(R_EVENTS_CRCERROR) != 0)
    {
        RADIO_REG(R_EVENTS_CRCERROR) = 0;
        RADIO_REG(R_EVENTS_END) = 0; // END fired together with CRCERROR
        if (sState == kStateRx)
        {
            RADIO_REG(R_TASKS_START) = 1; // same slot, just listen again
        }
    }

    if (RADIO_REG(R_EVENTS_END) != 0)
    {
        RADIO_REG(R_EVENTS_END) = 0;

        switch (sState)
        {
        case kStateTx:
        {
            // Outgoing frame is on the air.
            bool wantAck = (sTxFrame.mPsdu[0] & (FCF_ACK_REQUEST & 0xFF)) != 0;

            if (wantAck)
            {
                sAckWait    = true;
                sAckWaitSeq = sTxFrame.mPsdu[2];
                radioEnterRx(); // listen for the imm-ack
            }
            else
            {
                sTxEvent = kTxEventDone;
                radioEnterRx();
            }
            break;
        }

        case kStateAckTx:
            // Our immediate ack went out; resume listening.
            radioEnterRx();
            break;

        case kStateRx:
        {
            RxSlot &slot = sRxSlots[sCurrentSlot];

            if (RADIO_REG(R_EVENTS_CRCOK) == 0)
            {
                // END without CRCOK (CRCERROR path already restarted RX).
                break;
            }
            RADIO_REG(R_EVENTS_CRCOK) = 0;

            uint8_t phr = slot.buf[0];

            if (phr < 3 + FCS_SIZE || phr > MAX_PSDU)
            {
                RADIO_REG(R_TASKS_START) = 1;
                break;
            }

            uint8_t   macLen = (uint8_t)(phr - FCS_SIZE); // PSDU minus FCS
            FrameInfo info;
            bool      parsed = parseFrame(&slot.buf[1], macLen, info);
            int8_t    rssi   = lastRssiDbm();

            // Imm-ack we are waiting for after a TX?
            if (parsed && (info.fcf & FCF_TYPE_MASK) == FCF_TYPE_ACK)
            {
                if (sAckWait && info.seq == sAckWaitSeq && sTxEvent == kTxEventNone)
                {
                    sAckWait = false;
                    memcpy(sAckRxBuf, slot.buf, (size_t)phr + 1U - FCS_SIZE);
                    sAckRxBuf[0] = phr;
                    sAckRxFrame.mLength = phr;
                    sAckRxFrame.mChannel = sChannel;
                    sAckRxFrame.mInfo.mRxInfo.mRssi = rssi;
                    sAckRxFrame.mInfo.mRxInfo.mLqi  = rssiToLqi(rssi);
                    sAckRxFrame.mInfo.mRxInfo.mTimestamp = radioTimeUs();
                    sTxEvent = kTxEventAcked;
                }
                RADIO_REG(R_TASKS_START) = 1; // acks are never queued upward
                break;
            }

            bool accept = sPromiscuous ||
                          (parsed && (info.dstIsUs || info.dstIsBcast ||
                                      (info.fcf & FCF_DST_MASK) == FCF_DST_NONE));

            if (!accept)
            {
                RADIO_REG(R_TASKS_START) = 1;
                break;
            }

            slot.rssi      = rssi;
            slot.lqi       = rssiToLqi(rssi);
            slot.timestamp = radioTimeUs();

            bool needAck = !sPromiscuous && parsed && info.dstIsUs &&
                           (info.fcf & FCF_ACK_REQUEST) != 0 &&
                           (((info.fcf & FCF_TYPE_MASK) == FCF_TYPE_DATA) ||
                            ((info.fcf & FCF_TYPE_MASK) == FCF_TYPE_CMD));

            if (needAck)
            {
                bool fp = srcMatchHasPending(info);

                slot.ackedWithFp = fp;
                slot.state       = kSlotReady;

                sAckTxBuf[0] = 5; // PHR: FCF(2) + seq + FCS(2)
                sAckTxBuf[1] = (uint8_t)(FCF_TYPE_ACK | (fp ? FCF_FRAME_PENDING : 0));
                sAckTxBuf[2] = 0;
                sAckTxBuf[3] = info.seq;

                RADIO_REG(R_PACKETPTR) = (uint32_t)sAckTxBuf;
                RADIO_REG(R_EVENTS_DISABLED) = 0;
                RADIO_REG(R_SHORTS) = SH_DISABLED_TXEN | SH_TXREADY_START;
                RADIO_REG(R_TASKS_DISABLE) = 1;
                sState = kStateAckTx;
            }
            else
            {
                slot.ackedWithFp = false;
                slot.state       = kSlotReady;
                radioEnterRx(); // next free slot, keep listening
            }
            break;
        }

        default:
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// otPlatRadio* implementation
// ---------------------------------------------------------------------------

extern "C" {

otRadioCaps otPlatRadioGetCaps(otInstance *aInstance)
{
    (void)aInstance;
    return OT_RADIO_CAPS_NONE;
}

int8_t otPlatRadioGetReceiveSensitivity(otInstance *aInstance)
{
    (void)aInstance;
    return -100;
}

void otPlatRadioSetPanId(otInstance *aInstance, otPanId aPanId)
{
    (void)aInstance;
    uint32_t m = criticalEnter();
    sPanId = aPanId;
    criticalExit(m);
}

void otPlatRadioSetExtendedAddress(otInstance *aInstance, const otExtAddress *aExtAddress)
{
    (void)aInstance;
    uint32_t m = criticalEnter();
    memcpy(sExtAddr, aExtAddress->m8, 8); // already little-endian (as on air)
    criticalExit(m);
}

void otPlatRadioSetShortAddress(otInstance *aInstance, otShortAddress aShortAddress)
{
    (void)aInstance;
    uint32_t m = criticalEnter();
    sShortAddr = aShortAddress;
    criticalExit(m);
}

otError otPlatRadioGetTransmitPower(otInstance *aInstance, int8_t *aPower)
{
    (void)aInstance;
    if (aPower == NULL)
    {
        return OT_ERROR_INVALID_ARGS;
    }
    *aPower = sTxPower;
    return OT_ERROR_NONE;
}

otError otPlatRadioSetTransmitPower(otInstance *aInstance, int8_t aPower)
{
    (void)aInstance;
    // Clamp to the nRF52840 PA range.
    if (aPower > 8)
    {
        aPower = 8;
    }
    if (aPower < -40)
    {
        aPower = -40;
    }
    sTxPower = aPower;
    radioApplyTxPower();
    return OT_ERROR_NONE;
}

otError otPlatRadioGetCcaEnergyDetectThreshold(otInstance *aInstance, int8_t *aThreshold)
{
    (void)aInstance;
    if (aThreshold == NULL)
    {
        return OT_ERROR_INVALID_ARGS;
    }
    *aThreshold = sCcaThresholdDbm;
    return OT_ERROR_NONE;
}

otError otPlatRadioSetCcaEnergyDetectThreshold(otInstance *aInstance, int8_t aThreshold)
{
    (void)aInstance;
    int32_t ed = (int32_t)aThreshold - ED_FLOOR_DBM;
    if (ed < 0)
    {
        ed = 0;
    }
    if (ed > 255)
    {
        ed = 255;
    }
    sCcaThresholdDbm = aThreshold;
    // CCA mode = ED (0), EDTHRES in bits 8..15; keep correlator defaults.
    RADIO_REG(R_CCACTRL) = (RADIO_REG(R_CCACTRL) & ~0x0000FF07UL) | ((uint32_t)ed << 8);
    return OT_ERROR_NONE;
}

bool otPlatRadioGetPromiscuous(otInstance *aInstance)
{
    (void)aInstance;
    return sPromiscuous;
}

void otPlatRadioSetPromiscuous(otInstance *aInstance, bool aEnable)
{
    (void)aInstance;
    sPromiscuous = aEnable;
}

otError otPlatRadioEnable(otInstance *aInstance)
{
    (void)aInstance;
    if (!sEnabled)
    {
        nrfOtRadioInit();
        sEnabled = true;
        sState   = kStateSleep;
    }
    return OT_ERROR_NONE;
}

otError otPlatRadioDisable(otInstance *aInstance)
{
    (void)aInstance;
    if (sEnabled)
    {
        uint32_t m = criticalEnter();
        radioEnterSleep();
        sState   = kStateDisabled;
        sEnabled = false;
        criticalExit(m);
    }
    return OT_ERROR_NONE;
}

bool otPlatRadioIsEnabled(otInstance *aInstance)
{
    (void)aInstance;
    return sEnabled;
}

otError otPlatRadioSleep(otInstance *aInstance)
{
    (void)aInstance;
    if (!sEnabled)
    {
        return OT_ERROR_INVALID_STATE;
    }

    uint32_t m = criticalEnter();
    sAckWait = false;
    radioEnterSleep();
    criticalExit(m);
    return OT_ERROR_NONE;
}

otError otPlatRadioReceive(otInstance *aInstance, uint8_t aChannel)
{
    (void)aInstance;
    if (!sEnabled)
    {
        return OT_ERROR_INVALID_STATE;
    }

    uint32_t m = criticalEnter();

    sAckWait = false; // a new Receive() abandons any stale ack wait

    if (sChannel != aChannel || sState != kStateRx)
    {
        sChannel = aChannel;
        RADIO_REG(R_SHORTS) = 0;
        RADIO_REG(R_TASKS_CCASTOP) = 1;
        radioApplyChannel();
        radioEnterRx();
    }

    criticalExit(m);
    return OT_ERROR_NONE;
}

otRadioFrame *otPlatRadioGetTransmitBuffer(otInstance *aInstance)
{
    (void)aInstance;
    return &sTxFrame;
}

otError otPlatRadioTransmit(otInstance *aInstance, otRadioFrame *aFrame)
{
    if (!sEnabled || aFrame != &sTxFrame || aFrame->mLength < FCS_SIZE + 3 ||
        aFrame->mLength > MAX_PSDU)
    {
        return OT_ERROR_INVALID_STATE;
    }

    uint32_t m = criticalEnter();

    if (sState != kStateRx && sState != kStateSleep)
    {
        criticalExit(m);
        return OT_ERROR_INVALID_STATE;
    }

    sAckWait  = false;
    sTxEvent  = kTxEventNone;
    sTxPending = true;

    if (sChannel != aFrame->mChannel)
    {
        sChannel = aFrame->mChannel;
        radioApplyChannel();
    }

    sTxBuf[0] = (uint8_t)aFrame->mLength; // PHR includes the FCS bytes
    memcpy(&sTxBuf[1], aFrame->mPsdu, (size_t)aFrame->mLength - FCS_SIZE);

    if (sCurrentSlot < RX_SLOT_COUNT && sRxSlots[sCurrentSlot].state == kSlotRadio)
    {
        sRxSlots[sCurrentSlot].state = kSlotFree;
    }

    if (aFrame->mInfo.mTxInfo.mCsmaCaEnabled)
    {
        // Single CCA attempt; the sub-MAC owns the backoff loop. CCA needs
        // an active receiver, so make sure RX is up before CCASTART.
        RADIO_REG(R_EVENTS_CCAIDLE) = 0;
        RADIO_REG(R_EVENTS_CCABUSY) = 0;

        if (sState == kStateRx && (RADIO_REG(R_STATE) & 0x0FU) != 0U)
        {
            sState = kStateCca;
            RADIO_REG(R_SHORTS) = 0;
            RADIO_REG(R_TASKS_CCASTART) = 1;
        }
        else
        {
            // From sleep: ramp RX and chain straight into CCA.
            sState = kStateCca;
            RADIO_REG(R_SHORTS) = SH_DISABLED_RXEN | SH_RXREADY_START;
            RADIO_REG(R_EVENTS_RXREADY) = 0;
            RADIO_REG(R_TASKS_DISABLE) = 1;
            // Wait for ramp-up in the ISR? RXREADY has no interrupt enabled;
            // instead start CCA from the thread after ramp (~130 us spin).
            criticalExit(m);
            {
                uint32_t spin = 0;
                while (RADIO_REG(R_EVENTS_RXREADY) == 0 && spin++ < 50000U)
                {
                }
            }
            m = criticalEnter();
            RADIO_REG(R_SHORTS) = 0;
            RADIO_REG(R_TASKS_CCASTART) = 1;
        }
    }
    else
    {
        sState = kStateTx;
        RADIO_REG(R_PACKETPTR) = (uint32_t)sTxBuf;
        RADIO_REG(R_EVENTS_END) = 0;
        RADIO_REG(R_EVENTS_DISABLED) = 0;
        RADIO_REG(R_SHORTS) = SH_DISABLED_TXEN | SH_TXREADY_START;
        RADIO_REG(R_TASKS_DISABLE) = 1;
    }

    aFrame->mInfo.mTxInfo.mTimestamp = radioTimeUs();

    criticalExit(m);

    otPlatRadioTxStarted(aInstance, aFrame);

    return OT_ERROR_NONE;
}

int8_t otPlatRadioGetRssi(otInstance *aInstance)
{
    (void)aInstance;

    if (sState != kStateRx)
    {
        return OT_RADIO_RSSI_INVALID;
    }

    RADIO_REG(R_EVENTS_RSSIEND) = 0;
    RADIO_REG(R_TASKS_RSSISTART) = 1;

    uint32_t spin = 0;
    while (RADIO_REG(R_EVENTS_RSSIEND) == 0 && spin++ < 10000U)
    {
    }

    return lastRssiDbm();
}

otError otPlatRadioEnergyScan(otInstance *aInstance, uint8_t aScanChannel, uint16_t aScanDuration)
{
    (void)aInstance;
    (void)aScanChannel;
    (void)aScanDuration;
    // The sub-MAC performs energy scan in software by polling GetRssi().
    return OT_ERROR_NOT_IMPLEMENTED;
}

void otPlatRadioEnableSrcMatch(otInstance *aInstance, bool aEnable)
{
    (void)aInstance;
    sSrcMatchEnabled = aEnable;
}

otError otPlatRadioAddSrcMatchShortEntry(otInstance *aInstance, otShortAddress aShortAddress)
{
    (void)aInstance;
    uint32_t m = criticalEnter();
    otError  err = OT_ERROR_NO_BUFS;

    for (uint8_t i = 0; i < sSrcMatchShortCount; i++)
    {
        if (sSrcMatchShort[i] == aShortAddress)
        {
            err = OT_ERROR_NONE;
            break;
        }
    }
    if (err != OT_ERROR_NONE && sSrcMatchShortCount < SRC_MATCH_SHORT_COUNT)
    {
        sSrcMatchShort[sSrcMatchShortCount++] = aShortAddress;
        err = OT_ERROR_NONE;
    }

    criticalExit(m);
    return err;
}

otError otPlatRadioAddSrcMatchExtEntry(otInstance *aInstance, const otExtAddress *aExtAddress)
{
    (void)aInstance;
    uint32_t m = criticalEnter();
    otError  err = OT_ERROR_NO_BUFS;

    for (uint8_t i = 0; i < sSrcMatchExtCount; i++)
    {
        if (memcmp(sSrcMatchExt[i], aExtAddress->m8, 8) == 0)
        {
            err = OT_ERROR_NONE;
            break;
        }
    }
    if (err != OT_ERROR_NONE && sSrcMatchExtCount < SRC_MATCH_EXT_COUNT)
    {
        memcpy(sSrcMatchExt[sSrcMatchExtCount++], aExtAddress->m8, 8);
        err = OT_ERROR_NONE;
    }

    criticalExit(m);
    return err;
}

otError otPlatRadioClearSrcMatchShortEntry(otInstance *aInstance, otShortAddress aShortAddress)
{
    (void)aInstance;
    uint32_t m = criticalEnter();
    otError  err = OT_ERROR_NO_ADDRESS;

    for (uint8_t i = 0; i < sSrcMatchShortCount; i++)
    {
        if (sSrcMatchShort[i] == aShortAddress)
        {
            sSrcMatchShort[i] = sSrcMatchShort[--sSrcMatchShortCount];
            err = OT_ERROR_NONE;
            break;
        }
    }

    criticalExit(m);
    return err;
}

otError otPlatRadioClearSrcMatchExtEntry(otInstance *aInstance, const otExtAddress *aExtAddress)
{
    (void)aInstance;
    uint32_t m = criticalEnter();
    otError  err = OT_ERROR_NO_ADDRESS;

    for (uint8_t i = 0; i < sSrcMatchExtCount; i++)
    {
        if (memcmp(sSrcMatchExt[i], aExtAddress->m8, 8) == 0)
        {
            memcpy(sSrcMatchExt[i], sSrcMatchExt[--sSrcMatchExtCount], 8);
            err = OT_ERROR_NONE;
            break;
        }
    }

    criticalExit(m);
    return err;
}

void otPlatRadioClearSrcMatchShortEntries(otInstance *aInstance)
{
    (void)aInstance;
    sSrcMatchShortCount = 0;
}

void otPlatRadioClearSrcMatchExtEntries(otInstance *aInstance)
{
    (void)aInstance;
    sSrcMatchExtCount = 0;
}

// CSL is disabled in the build config; these exist because weak helpers in
// the core (e.g. otPlatRadioResetCsl) still reference them.
otError otPlatRadioEnableCsl(otInstance *aInstance, uint32_t aCslPeriod, otShortAddress aShortAddr, const otExtAddress *aExtAddr)
{
    (void)aInstance;
    (void)aCslPeriod;
    (void)aShortAddr;
    (void)aExtAddr;
    return OT_ERROR_NOT_IMPLEMENTED;
}

void otPlatRadioUpdateCslSampleTime(otInstance *aInstance, uint32_t aCslSampleTime)
{
    (void)aInstance;
    (void)aCslSampleTime;
}

} // extern "C"

// ---------------------------------------------------------------------------
// Init + event pump
// ---------------------------------------------------------------------------

extern "C" void nrfOtRadioInit(void)
{
    // HFXO must be running for the radio to meet RF timing.
    if (CLOCK_REG(C_EVENTS_HFCLKSTARTED) == 0)
    {
        CLOCK_REG(C_TASKS_HFCLKSTART) = 1;
        while (CLOCK_REG(C_EVENTS_HFCLKSTARTED) == 0)
        {
        }
    }

    // The hardware-verified IEEE 802.15.4 configuration.
    RADIO_REG(R_MODE)    = 15;          // Ieee802154_250Kbit
    RADIO_REG(R_PCNF0)   = 0x06000008;  // LFLEN=8, PLEN=32bitZero, CRCINC
    RADIO_REG(R_PCNF1)   = 0x7F;        // MAXLEN=127
    RADIO_REG(R_CRCCNF)  = 0x202;       // 2-byte FCS, skip address
    RADIO_REG(R_CRCPOLY) = 0x011021;
    RADIO_REG(R_CRCINIT) = 0;
    RADIO_REG(R_SFD)     = 0xA7;
    radioApplyChannel();
    radioApplyTxPower();
    otPlatRadioSetCcaEnergyDetectThreshold(NULL, sCcaThresholdDbm);

    for (uint8_t i = 0; i < RX_SLOT_COUNT; i++)
    {
        sRxSlots[i].state = kSlotFree;
    }

    sTxFrame.mPsdu = sTxPsdu;
    sAckRxFrame.mPsdu = &sAckRxBuf[1];

    // Make sure the microsecond clock used for timestamps is running.
    if (!nrfTimer3().isRunning())
    {
        nrfTimer3().begin(1000000U);
        nrfTimer3().start();
    }

    RADIO_REG(R_INTENCLR) = 0xFFFFFFFFUL;
    RADIO_REG(R_INTENSET) = INT_END | INT_CRCERROR | INT_CCAIDLE | INT_CCABUSY;

    NVIC_IPR1_B1 = 0x40;       // priority 2 (0 = highest)
    NVIC_ICER    = RADIO_IRQ_BIT;
    NVIC_ISER    = RADIO_IRQ_BIT;
}

extern "C" void nrfOtRadioProcess(otInstance *aInstance)
{
    // Receive-done events.
    for (uint8_t i = 0; i < RX_SLOT_COUNT; i++)
    {
        RxSlot &slot = sRxSlots[i];

        if (slot.state != kSlotReady)
        {
            continue;
        }

        otRadioFrame frame;

        memset(&frame, 0, sizeof(frame));
        frame.mPsdu    = &slot.buf[1];
        frame.mLength  = slot.buf[0];
        frame.mChannel = sChannel;
        frame.mInfo.mRxInfo.mRssi = slot.rssi;
        frame.mInfo.mRxInfo.mLqi  = slot.lqi;
        frame.mInfo.mRxInfo.mTimestamp = slot.timestamp;
        frame.mInfo.mRxInfo.mAckedWithFramePending = slot.ackedWithFp;

        otPlatRadioReceiveDone(aInstance, &frame, OT_ERROR_NONE);

        slot.state = kSlotFree;
    }

    // Transmit-done events.
    if (sTxPending)
    {
        TxEvent ev = sTxEvent;

        switch (ev)
        {
        case kTxEventDone:
            sTxEvent   = kTxEventNone;
            sTxPending = false;
            otPlatRadioTxDone(aInstance, &sTxFrame, NULL, OT_ERROR_NONE);
            break;

        case kTxEventAcked:
            sTxEvent   = kTxEventNone;
            sTxPending = false;
            otPlatRadioTxDone(aInstance, &sTxFrame, &sAckRxFrame, OT_ERROR_NONE);
            break;

        case kTxEventCcaBusy:
            sTxEvent   = kTxEventNone;
            sTxPending = false;
            otPlatRadioTxDone(aInstance, &sTxFrame, NULL, OT_ERROR_CHANNEL_ACCESS_FAILURE);
            break;

        default:
            break;
        }
    }
}
