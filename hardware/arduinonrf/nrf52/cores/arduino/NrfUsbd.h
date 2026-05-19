#pragma once

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
    void userFlush();
    void injectRx(const uint8_t *data, size_t length);
    void setLineCoding(const NrfUsbLineCoding &lineCoding);
    void setLineState(bool dtr, bool rts);
    void setUserLineCoding(const NrfUsbLineCoding &lineCoding);
    void setUserLineState(bool dtr, bool rts);
    NrfUsbdStatus status() const;

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

    void initDescriptors();
    void clearEvents();
    void enableInterrupts();
    void disableInterrupts();
    void enablePullup(bool enabled);
    void processBusState(bool hasVbus);
    void startCdcEndpoints();
    void queueDataOut(bool userPort);
    void serviceDataOut(bool userPort);
    void serviceDataIn(bool userPort);
    void serviceNotificationIn(bool userPort);
    void serviceDynamicEndpoints();
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
    volatile bool enabled_ = false;
    volatile bool started_ = false;
    volatile bool attached_ = false;
    volatile bool ready_ = false;
    volatile bool configured_ = false;
    volatile bool suspended_ = false;
    volatile bool dtr_ = false;
    volatile bool rts_ = false;
    volatile bool cdcActive_ = false;
    volatile bool dataInFlight_ = false;
    volatile bool notificationInFlight_ = false;
    volatile bool notificationPending_ = false;
    volatile bool userDtr_ = false;
    volatile bool userRts_ = false;
    volatile bool userDataInFlight_ = false;
    volatile bool userNotificationInFlight_ = false;
    volatile bool userNotificationPending_ = false;
    volatile Ep0InXferPhase ep0InXferPhase_ = Ep0InXferPhase::Idle;
    uint16_t ep0InRemaining_ = 0;
    const uint8_t *ep0InCursor_ = nullptr;
    bool ep0InNeedsZlp_ = false;

    volatile uint32_t detachRequestMagic_ = 0;
    volatile uint32_t detachCause_ = 0;
    volatile bool pendingAddressValid_ = false;
    volatile bool serviceSawNonResetBaud_ = false;
    volatile bool serviceTouchPending_ = false;
    volatile uint32_t serviceTouchResetMillis_ = 0;
    volatile uint8_t ignoredResetTouchCount_ = 0;
    volatile uint8_t address_ = 0;
    volatile uint8_t configuration_ = 0;
    volatile uint8_t pendingAddress_ = 0;
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
