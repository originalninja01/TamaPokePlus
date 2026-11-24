#pragma once
#if defined(ARDUINO_ARCH_RP2040)
#include <Arduino.h>

// Minimal I2S output for the RP2350 port, fed directly by core 1.
//
// arduino-pico's I2S class uses a DMA interrupt (DMA_IRQ_0) that it shares with
// the SPI/SD driver. With audio started on core 1 and the SD card on core 0,
// both cores ended up running the audio interrupt handler at once, corrupting
// its buffer list and hanging the board. This driver uses a PIO state machine
// only: no DMA, no interrupts. Core 1 pushes samples into the PIO FIFO and
// pio_sm_put_blocking() paces it at the sample rate.
//
// Pins: data on I2S_DO_IO, BCLK on I2S_BCK_IO, LRCLK on I2S_BCK_IO + 1.
// Format: Philips I2S, 16-bit stereo, 32 BCLK per frame (same as before).
bool i2sPioBegin(uint32_t sampleRate);
void i2sPioWrite(int16_t left, int16_t right);  // blocks until the FIFO has room
void i2sPioSilence(uint32_t ms, uint32_t sampleRate);  // keeps the clocks running
#endif
