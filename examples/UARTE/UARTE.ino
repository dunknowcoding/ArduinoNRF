// UARTE - the nRF52840's UART with EasyDMA.
//
// On classic 8-bit Arduinos the UART moves one byte at a time and interrupts
// the CPU for every character. The nRF52840 has UARTE instead: the same UART,
// but backed by EasyDMA, so bytes are streamed by hardware while the CPU is
// free. The result is reliable high-speed serial with few missed bytes.
//
// UART vs UARTE - the compatibility note you should know:
//   UART (legacy) and UARTE (DMA) are two register views of the SAME hardware
//   block (peripheral instance 0). Only ONE can be enabled at a time. This core
//   ALWAYS uses UARTE for Serial1, so there is nothing to configure and no
//   conflict to avoid - you simply get the DMA-backed version through the
//   normal Serial1 API. (The USB "Serial" port is a separate peripheral and
//   runs independently of this.)
//
// What EasyDMA buys you here:
//   * TX: each byte is clocked out by DMA, so back-to-back writes stay smooth
//     even at 1 Mbaud.
//   * RX: incoming bytes are DMA'd into a background ring buffer by an
//     interrupt, so a fast burst from the peer is not lost while loop() is busy.
//
// Wiring (3.3 V logic ONLY - 5 V will damage the nRF52840):
//   board D0 (TX)  ---->  peer RX
//   board D1 (RX)  <----  peer TX
//   board GND      -----  peer GND

static const size_t FRAME_LEN = 240;
uint8_t frame[FRAME_LEN];

void setup() {
  // UARTE runs cleanly at 1 Mbaud - far faster than a bit-banged UART.
  Serial1.begin(1000000);

  // Build a recognizable ramp pattern (0, 1, 2, ... 239) to stream out.
  for (size_t i = 0; i < FRAME_LEN; ++i) {
    frame[i] = (uint8_t)i;
  }
}

void loop() {
  // Send the whole frame. write(buffer, length) hands the bytes to the UARTE,
  // which DMAs them out; the CPU is not busy-waiting on each character.
  Serial1.write(frame, FRAME_LEN);

  // Drain whatever the peer sent. The bytes were already DMA'd into the RX ring
  // buffer in the background, so this just copies them out - none are dropped.
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    (void)b;                        // handle the incoming byte here
  }

  delay(100);
}
