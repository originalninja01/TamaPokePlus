#pragma once
#include <Arduino.h>
#if defined(ARDUINO_ARCH_RP2040)
#include <FS.h>
#endif

// Sprite animado TPK1 (formato heredado, camino de respaldo). El proyecto usa
// PMD/TPK2 (PmdMon) para todo; esta ruta queda inactiva si no hay NNN.bin en la SD.
// Los datos indexados viven en PSRAM; la paleta es RGB565.
struct SdMon {
  bool loaded = false;
  uint16_t w = 0, h = 0, frames = 0, frameMs = 100;
  uint8_t scale = 2;       // factor de zoom entero al dibujar
  uint16_t palCount = 0;
  uint16_t pal[256];
  uint8_t *data = nullptr;  // frames * w * h indices (0xFF = transparente)

  bool load(uint16_t dexNum, bool shiny = false);
  void unload();
};

// acciones de los sprites PMD (formato TPK2)
enum : uint8_t {
  PMD_IDLE = 0, PMD_WALKL, PMD_WALKR, PMD_SLEEP, PMD_EAT, PMD_HURT,
  PMD_ATTACK, PMD_POSE, PMD_HOP, PMD_NOD, PMD_BREATH, PMD_SIT,
  PMD_NACTS
};

struct PmdAct {
  uint8_t w = 0, h = 0, frames = 0;
  uint8_t base = 0;  // fila+1 del pixel mas bajo (anclar por los pies, no el lienzo)
  uint16_t ms[24];
#if defined(ARDUINO_ARCH_RP2040)
  uint32_t off = 0;  // RP2350: offset of frame 0 in the SD file (frames are read on demand)
#else
  const uint8_t *data = nullptr;  // frames * w * h en el blob
#endif
};

// sprite PMD multi-accion cargado de la SD a PSRAM
struct PmdMon {
  bool loaded = false;
  uint16_t palCount = 0;
  uint16_t pal[256];
#if defined(ARDUINO_ARCH_RP2040)
  // RP2350 (no PSRAM): only the index lives in RAM; frame() reads one frame
  // from the SD card into a small buffer and keeps it until a different one is asked for
  char path[28] = "";
  File file;
  uint8_t *frameBuf = nullptr;
  uint32_t frameBufSize = 0;
  int16_t cachedAct = -1, cachedFrame = -1;
  const uint8_t *frame(uint8_t act, uint8_t fi);  // w*h indices, or nullptr
#else
  uint8_t *blob = nullptr;
#endif
  PmdAct acts[PMD_NACTS];

  bool load(uint16_t dexNum, bool shiny = false);
  void unload();
  bool has(uint8_t a) const { return loaded && a < PMD_NACTS && acts[a].frames > 0; }
};

// miniaturas de la galeria (thumbs.bin entero en PSRAM)
struct SdThumbs {
  bool loaded = false;
#if defined(ARDUINO_ARCH_RP2040)
  uint32_t *offs = nullptr;  // RP2350: offset table only; each thumbnail is read on demand
#else
  uint8_t *data = nullptr;
#endif
  uint16_t count = 0;
  uint32_t size = 0;  // bytes leidos: acota los offsets del fichero
  bool load();
  void unload();  // libera el blob: recargar tras recibir un thumbs.bin nuevo
  const uint8_t *get(int16_t dex) const;  // blob: w,h,palCount,pal[],idx[]
};
extern SdThumbs thumbs;

bool sdBegin();                 // monta la SD (SDMMC 1-bit en ESP32, SPI en RP2350), true si hay tarjeta
bool sdSerialCommand(const String &line);  // PUT/LS por USB; true si la maneja
extern bool sdReady;
extern bool sdDirty;  // true tras recibir archivos: recargar sprite
