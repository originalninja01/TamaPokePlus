#pragma once
#if defined(ARDUINO_ARCH_RP2040)
#include <Arduino.h>

// CO5300 AMOLED driver for the Waveshare RP2350-Touch-AMOLED-1.75.
//
// The panel speaks QSPI. On the RP2350 it is driven by a 2-instruction PIO
// program that clocks out one nibble on D0..D3 per SCLK; commands and register
// parameters are sent "single line" by spreading each bit onto D0 only (the
// same trick, init sequence and column offset as Waveshare's own example).
// Pixel data goes out over DMA.
class CO5300_PIO {
public:
  bool begin();
  // 0..255, same meaning as Arduino_CO5300::setBrightness on the ESP32 build
  void setBrightness(uint8_t b);

  // Pixel streaming: open a window, push RGB565 big-endian bytes, close it.
  void beginPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void pushAsync(const void *bytes, uint32_t len);  // starts a DMA transfer
  void waitDma();
  void endPixels();  // waits for the last bit to leave the PIO, raises CS

private:
  void cmdParams(uint8_t reg, const uint8_t *data, uint8_t n);
  void putLine1(uint8_t v);   // one byte on D0 only (4 FIFO pushes)
  void putQuad(uint8_t v);    // one byte on D0..D3 (1 FIFO push)
  void drain();               // FIFO empty and SM stalled
  void select();
  void deselect();

  PIO _pio = nullptr;
  uint _sm = 0;
  int _dma = -1;
  bool _ok = false;
};

extern CO5300_PIO amoled;
#endif
