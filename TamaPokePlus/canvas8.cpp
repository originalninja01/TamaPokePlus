#if defined(ARDUINO_ARCH_RP2040)
#include "canvas8.h"
#include "co5300_pio.h"
#include "pin_config.h"

// framebuffer in .bss so the linker report shows the real RAM budget
static uint8_t s_fb[LCD_WIDTH * LCD_HEIGHT];
// two lines in flight: one being converted while the other streams out
static uint16_t s_line[2][LCD_WIDTH];

// free slots, used as a stack
static uint8_t s_free[256];
static uint16_t s_nfree = 0;
static bool s_noCompact = false;  // a compaction freed almost nothing this frame: don't thrash
static uint16_t s_hcount = 0;     // entries in the colour hash

Canvas8::Canvas8(int16_t w, int16_t h) : Arduino_GFX(w, h), _fb(s_fb) {}

static inline uint16_t hashOf(uint16_t c) { return (uint16_t)(((uint32_t)c * 2654435761u) >> 22); }

int16_t Canvas8::hashFind(uint16_t c) const {
  uint16_t h = hashOf(c) & (HSIZE - 1);
  while (_hVal[h] >= 0) {
    if (_hKey[h] == c) return _hVal[h];
    h = (h + 1) & (HSIZE - 1);
  }
  return -1;
}

void Canvas8::hashInsert(uint16_t c, uint8_t i) {
  uint16_t h = hashOf(c) & (HSIZE - 1);
  while (_hVal[h] >= 0) h = (h + 1) & (HSIZE - 1);
  _hKey[h] = c;
  _hVal[h] = i;
  s_hcount++;
}

void Canvas8::rebuildHash() {
  for (uint16_t i = 0; i < HSIZE; i++) _hVal[i] = -1;
  s_hcount = 0;
  for (uint16_t s = 0; s < 256; s++)
    if (_slotUsed[s]) hashInsert(_pal[s], (uint8_t)s);
  _lastValid = false;
}

static void resetFree() {
  s_nfree = 0;
  for (int s = 255; s >= 0; s--) s_free[s_nfree++] = (uint8_t)s;  // pops 0 first
}

// reclaim every slot that no pixel uses any more
void Canvas8::compact() {
  static bool seen[256];
  memset(seen, 0, sizeof(seen));
  const uint32_t *p = (const uint32_t *)_fb;
  const uint32_t n = (uint32_t)WIDTH * HEIGHT / 4;
  for (uint32_t i = 0; i < n; i++) {
    uint32_t v = p[i];
    seen[v & 0xFF] = true;
    seen[(v >> 8) & 0xFF] = true;
    seen[(v >> 16) & 0xFF] = true;
    seen[v >> 24] = true;
  }
  for (uint32_t i = n * 4; i < (uint32_t)WIDTH * HEIGHT; i++) seen[_fb[i]] = true;
  uint16_t freed = 0;
  s_nfree = 0;
  for (int s = 255; s >= 0; s--) {
    if (_slotUsed[s] && !seen[s]) { _slotUsed[s] = false; freed++; }
    if (!_slotUsed[s]) s_free[s_nfree++] = (uint8_t)s;
  }
  _count = 256 - s_nfree;
  if (freed < 16) s_noCompact = true;
  rebuildHash();
}

uint8_t Canvas8::idxSlow(uint16_t c) {
  int16_t f = hashFind(c);
  if (f < 0) {
    if (s_nfree == 0 && !s_noCompact) compact();
    if (s_nfree > 0) {
      uint8_t s = s_free[--s_nfree];
      _slotUsed[s] = true;
      _pal[s] = c;
      _palBE[s] = (uint16_t)((c >> 8) | (c << 8));
      _count++;
      hashInsert(c, s);
      f = s;
    } else {
      // more than 256 colours on screen at once: use the closest one
      _overflows++;
      int r = c >> 11, g = (c >> 5) & 0x3F, b = c & 0x1F;
      uint32_t best = UINT32_MAX;
      f = 0;
      for (int s = 0; s < 256; s++) {
        uint16_t q = _pal[s];
        int dr = r - (q >> 11), dg = g - ((q >> 5) & 0x3F), db = b - (q & 0x1F);
        uint32_t d = 4 * dr * dr + dg * dg + 4 * db * db;  // 5-bit channels weigh double
        if (d < best) { best = d; f = s; }
      }
      // remember the mapping so the next call is fast (until the next compaction)
      if (s_hcount < HSIZE * 3 / 4) hashInsert(c, (uint8_t)f);
    }
  }
  _lastC = c;
  _lastI = (uint8_t)f;
  _lastValid = true;
  return (uint8_t)f;
}

inline uint8_t Canvas8::idx(uint16_t c) {
  if (_lastValid && c == _lastC) return _lastI;
  return idxSlow(c);
}

bool Canvas8::begin(int32_t) {
  memset(_slotUsed, 0, sizeof(_slotUsed));
  resetFree();
  _count = 0;
  rebuildHash();
  memset(_fb, 0, (size_t)WIDTH * HEIGHT);
  idx(0x0000);  // slot 0 = black, matches the cleared framebuffer
  return amoled.begin();
}

void Canvas8::fillScreen(uint16_t color) {
  // the whole screen is overwritten: start the palette from scratch
  memset(_slotUsed, 0, sizeof(_slotUsed));
  resetFree();
  _count = 0;
  s_noCompact = false;
  rebuildHash();
  uint8_t i = idx(color);
  memset(_fb, i, (size_t)WIDTH * HEIGHT);
}

void Canvas8::writePixelPreclipped(int16_t x, int16_t y, uint16_t color) {
  _fb[(int32_t)y * WIDTH + x] = idx(color);
}

void Canvas8::writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (y < 0 || y >= HEIGHT || w == 0) return;
  if (w < 0) { x += w + 1; w = -w; }
  if (x >= WIDTH) return;
  int16_t x2 = x + w - 1;
  if (x2 < 0) return;
  if (x < 0) { x = 0; w = x2 + 1; }
  if (x2 >= WIDTH) w = WIDTH - x;
  memset(_fb + (int32_t)y * WIDTH + x, idx(color), w);
}

void Canvas8::writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (x < 0 || x >= WIDTH || h == 0) return;
  if (h < 0) { y += h + 1; h = -h; }
  if (y >= HEIGHT) return;
  int16_t y2 = y + h - 1;
  if (y2 < 0) return;
  if (y < 0) { y = 0; h = y2 + 1; }
  if (y2 >= HEIGHT) h = HEIGHT - y;
  uint8_t i = idx(color);
  uint8_t *p = _fb + (int32_t)y * WIDTH + x;
  while (h--) { *p = i; p += WIDTH; }
}

void Canvas8::writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  uint8_t i = idx(color);
  uint8_t *row = _fb + (int32_t)y * WIDTH + x;
  for (int16_t j = 0; j < h; j++) {
    memset(row, i, w);
    row += WIDTH;
  }
}

void Canvas8::flush(bool) {
  amoled.beginPixels(0, 0, WIDTH, HEIGHT);
  const uint8_t *src = _fb;
  for (int16_t y = 0; y < HEIGHT; y++) {
    uint16_t *dst = s_line[y & 1];
    for (int16_t x = 0; x < WIDTH; x++) dst[x] = _palBE[src[x]];
    src += WIDTH;
    amoled.waitDma();  // the other line has finished streaming
    amoled.pushAsync(dst, (uint32_t)WIDTH * 2);
  }
  amoled.endPixels();
  s_noCompact = false;
}
#endif
