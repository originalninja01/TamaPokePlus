#pragma once
#if defined(ARDUINO_ARCH_RP2040)
#include <Arduino_GFX_Library.h>

// Full-screen canvas with ONE BYTE per pixel, for boards without PSRAM.
//
// The ESP32 build draws into a 466x466 RGB565 framebuffer (434 KB, in PSRAM).
// The RP2350A has 520 KB of SRAM in total, so this canvas stores a palette
// index per pixel instead (217 KB) and expands it back to RGB565 line by line
// while the frame streams to the panel over DMA.
//
// The palette is built on the fly: every RGB565 colour the game draws gets the
// next free slot. When all 256 slots are taken, slots no longer present in the
// framebuffer are reclaimed. A typical TamaPoke frame (sky, ground, one sprite,
// UI) uses well under 256 colours, so nothing is lost; if a frame ever needs
// more, the extra colours snap to the nearest colour already on screen.
//
// Drawing API = Arduino_GFX, so the game code is unchanged.
class Canvas8 : public Arduino_GFX {
public:
  uint16_t pixel565(int16_t x, int16_t y) const { return _pal[_fb[(int32_t)y * WIDTH + x]]; }  // read back (tests)
  Canvas8(int16_t w, int16_t h);

  bool begin(int32_t speed = GFX_NOT_DEFINED) override;
  void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override;
  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void fillScreen(uint16_t color);  // hides the base one (not virtual there); gfx is a Canvas8*
  void flush(bool force_flush = false) override;

  uint8_t paletteUsed() const { return (uint8_t)(_count > 255 ? 255 : _count); }
  uint32_t paletteOverflows() const { return _overflows; }

private:
  uint8_t idx(uint16_t c);
  uint8_t idxSlow(uint16_t c);
  void rebuildHash();
  void compact();
  int16_t hashFind(uint16_t c) const;
  void hashInsert(uint16_t c, uint8_t i);

  uint8_t *_fb;
  uint16_t _pal[256];       // RGB565 per slot
  uint16_t _palBE[256];     // same, byte-swapped: stored little-endian it sends high byte first
  uint16_t _count = 0;      // slots in use (0..256)
  bool _slotUsed[256];
  // open-addressing hash colour -> slot
  static const uint16_t HSIZE = 1024;
  uint16_t _hKey[HSIZE];
  int16_t _hVal[HSIZE];     // -1 = empty
  uint16_t _lastC = 0;
  uint8_t _lastI = 0;
  bool _lastValid = false;
  uint32_t _overflows = 0;
};
#endif
