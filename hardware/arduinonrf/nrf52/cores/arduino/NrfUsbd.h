#pragma once
//
// NrfUsbd - device core of TaichiUSB, the ArduinoNRF self-developed USB device
// stack for the nRF52840 (a clean-room stack, NOT TinyUSB; see TaichiUsb.h for
// the stack identity, version, and the build-flag guard). This header declares
// the low-level USBD device driver (EasyDMA endpoints, errata-wrapped ENABLE,
// VBUS/OUTPUTRDY sequencing, suspend/resume, the 1200-bps-touch upload handoff)
// that the user CDC (NrfUsbSerial) and maintenance CDC (NrfServiceSerial) sit on.
//
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct NrfUsbdStatus {
    bool enabled;
    bool started;
    bool attached;
    bool vbusDetected;
    bool ready;
    bool configured;
    bool suspended;
    bool cdcActive;
    uint8_t address;
    uint8_t configuration;
    uint32_t eventCause;
};

struct NrfUsbLineCoding {
    uint32_t baudRate;
    uint8_t stopBits;
    uint8_t parity;
    uint8_t dataBits;
};

struct NrfUsbdPollTrace {
    uint32_t magic;
    uint32_t pollCalls;
    uint32_t irqCalls;
    uint32_t ep0SetupEvents;
    uint32_t ep0DataDoneEvents;
    uint32_t cdcOutEvents;
    uint32_t cdcInEvents;
    uint32_t usbEventEvents;
    uint32_t lastEventCause;
};

class NrfUsbdDriver {
public:
    void begin();
    void end();
    void attach();
    void detach();
    void poll();
    void irqHandler();
    // Called from SysTick so a 1200-bps touch completes even when loop() never
    // yields and no further USB packet arrives after the host drops DTR.
    void serviceTick();
    bool enabled() const;
    bool attached() const;
    bool ready() const;
    bool connected() const;
    bool configured() const;
    bool suspended() const;
    bool dtr() const;
    bool rts() const;
    bool userConnected() const;
    bool userDtr() const;
    bool userRts() const;
    unsigned long baud() const;
    unsigned long userBaud() const;
    uint8_t address() const;
    uint8_t configuration() const;
    bool sendInPacket(uint8_t endpoint, const void *data, size_t length);
    const NrfUsbLineCoding &lineCoding() const;
    const NrfUsbLineCoding &userLineCoding() const;
    int available() const;
    int peek() const;
    int read();
    size_t write(uint8_t value);
    void flush();
    int userAvailable() const;
    int userPeek() const;
    int userRead();
    size_t userWrite(uint8_t value);
    // Block write: push a whole buffer to the user-CDC TX ring lock-free (the ring
    // is single-producer/single-consumer) and arm the IN endpoint ONCE, instead of
    // taking a UsbdIrqLock around serviceDataIn() per byte. Cutting the per-byte
    // lock churn stops the foreground from starving the EPDATA ISR that arms the
    // next packet, which is what capped CDC TX throughput. @return bytes accepted.
    size_t userWrite(const uint8_t *data, size_t length);
    void userFlush();
    size_t serviceTxQueued() const;
    size_t userTxQueued() const;
    void injectRx(const uint8_t *data, size_t length);
    void setLineCoding(const NrfUsbLineCoding &lineCoding);
    void setLineState(bool dtr, bool rts);
    void setUserLineCoding(const NrfUsbLineCoding &lineCoding);
    void setUserLineState(bool dtr, bool rts);
    NrfUsbdStatus status() const;

    // Brick-prevention for the GDB stub. While the stub is halted in DebugMon
    // (highest priority), SysTick is blocked so millis() is frozen and the
    // normal millis-gated 1200-bps touch can never arm — an upload to a halted
    // board then stalls mid-DFU and corrupts flash. setStubHalted(true) tells
    // the driver it is being pumped from the halted stub loop;
    // serviceHaltedTouch() is called every poll iteration and reboots to the
    // bootloader on a host 1200-bps open, using a poll-iteration counter
    // instead of millis() so it works while time is frozen.
    void setStubHalted(bool halted);
    void serviceHaltedTouch();

    // TX nudge for the GDB stub. A reply newly queued by the halted stub has no
    // pending USB event to start it, so the stub calls this to kick
    // serviceDataIn(). It is PRIMASK-guarded against the USBD ISR.
    void kickServiceDataIn();

    // Drain only CDC OUT endpoints whose EPDATASTATUS bit proves that a complete
    // host packet is buffered. The GDB stub calls this while halted because the
    // aggregate EPDATA interrupt may be masked or consumed before its polling
    // loop observes it.
    void drainServiceDataOut();

    // Foreground RX pump for sketches: bit-gated drain of both CDC OUT
    // endpoints, PRIMASK-guarded against the ISR. Serial.available()/read()
    // call this so host->device bulk OUT makes foreground progress even if an
    // aggregate EPDATA edge was already consumed.
    void pumpRx();

private:
    enum class ControlOutTransfer : uint8_t {
        None,
        ServiceLineCoding,
        UserLineCoding,
    };

    enum class Ep0InXferPhase : uint8_t {
        Idle,
        Data,
        StatusPending,
    };

    void resetEp0InXferState();
    void sendEp0ControlInChunkOrAdvanceStatus();

    bool initDescriptors();
    void clearEvents();
    void enableInterrupts();
    void disableInterrupts();
    void enablePullup(bool enabled);
    void processBusState(bool hasVbus);
    void startCdcEndpoints();
    void fetchOutPacket(uint8_t endpoint, bool userPort, uint32_t statusBit);
    void serviceDataOut(bool userPort, uint32_t received);
    void serviceDataIn(bool userPort);
    void serviceNotificationIn(bool userPort);
    void serviceSetup();
    void completeControlOutTransfer();
    void handleStandardRequest(uint8_t request, uint16_t value, uint16_t index, uint16_t length);
    void handleClassRequest(uint8_t requestType, uint8_t request, uint16_t value, uint16_t index, uint16_t length);
    void startControlIn(const uint8_t *data, size_t length);
    void expectControlOut(ControlOutTransfer transferType, size_t length);
    void sendZeroLengthStatus();
    void stallControlEndpoint();
    void completePendingAddress();
    void setAddress(uint8_t address);
    bool interfaceExists(uint8_t interfaceNumber) const;
    bool endpointDescriptorAttributes(uint8_t endpointAddress, uint8_t &attributes) const;
    bool endpointRequestValid(uint16_t index, uint8_t &endpointAddress, uint8_t &attributes) const;
    void setEndpointHalt(uint8_t endpointAddress, bool halted);
    bool endpointHalted(uint8_t endpointAddress) const;
    void ringPushRx(uint8_t value);
    bool ringPushTx(uint8_t value);
    int ringPopRx();
    int ringPeekRx() const;
    size_t txPending() const;
    void userRingPushRx(uint8_t value);
    bool userRingPushTx(uint8_t value);
    int userRingPopRx();
    int userRingPeekRx() const;
    size_t userTxPending() const;
    void resetConnectionState();
    void resetDynamicEndpoints();
    void queueSerialStateNotification(bool userPort);
    void updateSerialState(bool userPort);
    void serviceTouchTimer();
    void serviceDetachTimer();
    volatile bool enabled_ = false;
    volatile bool started_ = false;
    volatile bool attached_ = false;
    volatile bool ready_ = false;
    volatile bool startRequested_ = false;
    volatile bool startupInProgress_ = false;
    // A failed ENABLE/READY ownership transition is terminal for this object.
    // Re-entering the same half-initialized controller from attach()/begin()
    // can make a host-visible node that never completes control transfers.
    // A chip reset is the next safe peripheral-ownership boundary.
    volatile bool startupFaulted_ = false;
    volatile bool configured_ = false;
    volatile bool suspended_ = false;
    volatile bool dtr_ = false;
    volatile bool rts_ = false;
    volatile uint32_t dtrAssertedMillis_ = 0;
    volatile bool cdcActive_ = false;
    volatile bool dataInFlight_ = false;
    volatile uint8_t dataInFlightLength_ = 0;
    volatile bool notificationInFlight_ = false;
    volatile bool notificationPending_ = false;
    volatile bool userDtr_ = false;
    volatile bool userRts_ = false;
    volatile uint32_t userDtrAssertedMillis_ = 0;
    volatile bool userDataInFlight_ = false;
    volatile uint8_t userDataInFlightLength_ = 0;
    volatile bool userNotificationInFlight_ = false;
    volatile bool userNotificationPending_ = false;
    volatile Ep0InXferPhase ep0InXferPhase_ = Ep0InXferPhase::Idle;
    uint16_t ep0InRemaining_ = 0;
    uint16_t ep0InRequested_ = 0;
    const uint8_t *ep0InCursor_ = nullptr;
    bool ep0InNeedsZlp_ = false;

    volatile uint32_t detachRequestMagic_ = 0;
    volatile uint32_t detachCause_ = 0;
    volatile uint32_t detachRequestedMillis_ = 0;
    volatile bool pendingAddressValid_ = false;
    volatile bool serviceSawNonResetBaud_ = false;
    volatile bool serviceTouchPending_ = false;
    volatile bool stubHalted_ = false;
    volatile uint32_t haltTouchTicks_ = 0;
    volatile uint32_t serviceTouchResetMillis_ = 0;
    // millis() when the service CDC last saw a 1200-bps SET_LINE_CODING. Lets a
    // following DTR-drop arm the touch even if the host's single-port sequence
    // (usbcdc=disabled) made the 1200 baud and the DTR-drop non-coincident.
    volatile uint32_t serviceSaw1200Millis_ = 0;
    volatile uint8_t ignoredResetTouchCount_ = 0;
    volatile uint8_t address_ = 0;
    volatile uint8_t configuration_ = 0;
    volatile uint8_t pendingAddress_ = 0;
    volatile uint8_t haltedInEndpoints_ = 0;
    volatile uint8_t haltedOutEndpoints_ = 0;
    volatile uint32_t configStartMillis_ = 0;
    volatile uint32_t configuredMillis_ = 0;
    NrfUsbLineCoding lineCoding_ = {115200UL, 0U, 0U, 8U};
    NrfUsbLineCoding userLineCoding_ = {115200UL, 0U, 0U, 8U};
    ControlOutTransfer pendingControlOut_ = ControlOutTransfer::None;
    uint8_t controlOutBuffer_[16] = {0};
    // 256 bytes — full configuration descriptor payload (dual CDC + DFU +
    // PluggableUSB). EP0 responses are staged here; multi-packet control IN is
    // split in software at CONTROL_EP_MAX_PACKET with mandatory STATUS via EP0STATUS.
    uint8_t controlInBuffer_[256] = {0};
    size_t controlOutExpected_ = 0;
    size_t controlOutLength_ = 0;
    static constexpr size_t RingBufferSize = 256;
    uint8_t rxBuffer_[RingBufferSize] = {0};
    uint8_t txBuffer_[RingBufferSize] = {0};
    volatile size_t rxHead_ = 0;
    volatile size_t rxTail_ = 0;
    volatile size_t txHead_ = 0;
    volatile size_t txTail_ = 0;
    uint8_t userRxBuffer_[RingBufferSize] = {0};
    uint8_t userTxBuffer_[RingBufferSize] = {0};
    volatile size_t userRxHead_ = 0;
    volatile size_t userRxTail_ = 0;
    volatile size_t userTxHead_ = 0;
    volatile size_t userTxTail_ = 0;
    uint8_t endpointOutBuffer_[64] = {0};
    uint8_t endpointInBuffer_[64] = {0};
    uint8_t notificationBuffer_[10] = {0};
    uint8_t userEndpointOutBuffer_[64] = {0};
    uint8_t userEndpointInBuffer_[64] = {0};
    uint8_t userNotificationBuffer_[10] = {0};
    uint8_t dynamicInBuffers_[8][64] = {{0}};
    volatile uint8_t dynamicInLengths_[8] = {0};
    volatile bool dynamicInBusy_[8] = {false};
    uint8_t deviceDescriptor_[18] = {0};
    uint8_t configurationDescriptor_[192] = {0};
    uint16_t configurationDescriptorLength_ = 0;
    volatile uint32_t eventCause_ = 0;
    volatile uint16_t serialStateBitmap_ = 0;
    volatile uint16_t userSerialStateBitmap_ = 0;
};

NrfUsbdDriver &nrfUsbdDriver();
const NrfUsbdPollTrace &nrfUsbdPollTrace();
void nrfUsbdResetPollTrace();
void nrfUsbdClearPollTrace();
