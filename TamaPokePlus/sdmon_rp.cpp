#if defined(ARDUINO_ARCH_RP2040)
// RP2350 version of sdmon.cpp. Same file formats, same serial PUT/LS protocol,
// but nothing big is held in RAM: the RP2350A has 520 KB of SRAM and the
// framebuffer alone takes 217 KB.
//  - PmdMon keeps the palette and per-action index (~1 KB) and reads one
//    frame at a time from the SD card (largest frame in the set: 19 KB).
//  - SdThumbs keeps the offset table (~600 B) and reads thumbnails on demand.
//  - SdMon (legacy TPK1 path) is disabled: the PMD set covers every species.
#include "sdmon.h"
#include "pin_config.h"
#include <SPI.h>
#include <SD.h>

bool sdReady = false;
bool sdDirty = false;
SdThumbs thumbs;

static File thumbsFile;

// Sprites keep their SD file open between frames. Before anything writes to
// the card (USB upload), every open handle is closed: replacing a file that is
// still open would corrupt the FAT. Handles reopen lazily on the next read.
static PmdMon *gOpenMons[8];
static void trackOpen(PmdMon *m) {
  for (auto &slot : gOpenMons) if (slot == m) return;
  for (auto &slot : gOpenMons) if (!slot) { slot = m; return; }
}
static void untrack(PmdMon *m) {
  for (auto &slot : gOpenMons) if (slot == m) slot = nullptr;
}
static void sdReleaseFiles() {
  for (auto m : gOpenMons) {
    if (!m) continue;
    if (m->file) m->file.close();
    m->cachedAct = m->cachedFrame = -1;  // the file may be about to change
  }
  if (thumbsFile) thumbsFile.close();
}

// ---------------------------------------------------------------------------
// PMD sprites (TPK2)
// ---------------------------------------------------------------------------
bool PmdMon::load(uint16_t dexNum, bool shiny) {
  unload();
  if (!sdReady) return false;

  snprintf(path, sizeof(path), "/mons/p%s%03u.bin", shiny ? "s" : "", dexNum);
  file = SD.open(path, FILE_READ);
  if (!file && shiny) {  // sin shiny PMD: usa el normal
    snprintf(path, sizeof(path), "/mons/p%03u.bin", dexNum);
    file = SD.open(path, FILE_READ);
  }
  if (!file) return false;
  trackOpen(this);

  uint32_t size = file.size();
  uint8_t hdr[7];
  if (size < 7 || file.read(hdr, 7) != 7 || memcmp(hdr, "TPK2", 4) != 0) { unload(); return false; }
  uint8_t nActs = hdr[4];
  memcpy(&palCount, hdr + 5, 2);
  if (palCount > 256 || (uint32_t)7 + palCount * 2 > size) { unload(); return false; }
  if (file.read((uint8_t *)pal, palCount * 2) != palCount * 2) { unload(); return false; }

  // pass 1: the action index (sizes, timings, offsets)
  uint32_t p = 7 + palCount * 2;
  uint32_t maxFrame = 0;
  for (uint8_t i = 0; i < nActs && p + 4 <= size; i++) {
    uint8_t ah[4];
    file.seek(p);
    if (file.read(ah, 4) != 4) { unload(); return false; }
    uint8_t id = ah[0], w = ah[1], h = ah[2], nf = ah[3];
    p += 4;
    if (id >= PMD_NACTS || nf > 24) { unload(); return false; }
    uint32_t bytes = (uint32_t)nf * 2 + (uint32_t)w * h * nf;
    if (w == 0 || h == 0 || nf == 0 || p + bytes > size) { unload(); return false; }
    PmdAct &a = acts[id];
    a.w = w;
    a.h = h;
    a.frames = nf;
    uint8_t msb[48];
    if (file.read(msb, nf * 2) != nf * 2) { unload(); return false; }
    for (uint8_t k = 0; k < nf; k++) {
      a.ms[k] = msb[2 * k] | (msb[2 * k + 1] << 8);
      if (a.ms[k] == 0) a.ms[k] = 100;  // nunca 0: pmdFrameAt() giraria sin avanzar
    }
    p += nf * 2;
    a.off = p;
    p += (uint32_t)w * h * nf;
    if ((uint32_t)w * h > maxFrame) maxFrame = (uint32_t)w * h;
  }

  frameBuf = (uint8_t *)malloc(maxFrame);
  if (!frameBuf) {
    Serial.printf("sin RAM para el frame de %s (%u B)\n", path, maxFrame);
    unload();
    return false;
  }
  frameBufSize = maxFrame;

  // pass 2: lowest opaque row over every frame of each action (anchor by the feet)
  for (uint8_t id = 0; id < PMD_NACTS; id++) {
    PmdAct &a = acts[id];
    if (!a.frames) continue;
    uint8_t base = 1;
    for (uint8_t f = 0; f < a.frames; f++) {
      const uint8_t *fr = frame(id, f);
      if (!fr) { unload(); return false; }
      for (int r = a.h - 1; r >= base; r--) {
        bool any = false;
        for (int c = 0; c < a.w && !any; c++)
          if (fr[r * a.w + c] != 0xFF) any = true;
        if (any) { base = r + 1; break; }
      }
    }
    a.base = base;
  }
  loaded = true;
  Serial.printf("cargado %s (%u KB en SD, %u B en RAM)\n", path, size / 1024, maxFrame);
  return true;
}

const uint8_t *PmdMon::frame(uint8_t act, uint8_t fi) {
  if (act >= PMD_NACTS || !frameBuf) return nullptr;
  if (!file) {  // closed for an SD write: reopen
    if (!sdReady || !path[0]) return nullptr;
    file = SD.open(path, FILE_READ);
    if (!file) return nullptr;
  }
  const PmdAct &a = acts[act];
  if (fi >= a.frames) return nullptr;
  if (cachedAct == act && cachedFrame == fi) return frameBuf;
  uint32_t n = (uint32_t)a.w * a.h;
  if (n > frameBufSize) return nullptr;
  cachedAct = cachedFrame = -1;
  if (!file.seek(a.off + (uint32_t)fi * n) || file.read(frameBuf, n) != (int)n) return nullptr;
  cachedAct = act;
  cachedFrame = fi;
  return frameBuf;
}

void PmdMon::unload() {
  untrack(this);
  if (file) file.close();
  if (frameBuf) { free(frameBuf); frameBuf = nullptr; }
  frameBufSize = 0;
  cachedAct = cachedFrame = -1;
  for (auto &a : acts) {
    a.w = a.h = a.frames = a.base = 0;
    a.off = 0;
  }
  loaded = false;
}

// ---------------------------------------------------------------------------
// Pokedex thumbnails (TPTH): offset table in RAM, a few blobs cached
// ---------------------------------------------------------------------------
static const int THUMB_SLOTS = 4;
static const uint32_t THUMB_MAX = 3 + 256 * 2 + 64 * 64;  // generous upper bound per blob
static struct { int16_t dex; uint8_t *buf; } tcache[THUMB_SLOTS];
static uint8_t tnext = 0;

static void thumbCacheClear() {
  for (auto &c : tcache) {
    if (c.buf) free(c.buf);
    c.buf = nullptr;
    c.dex = 0;
  }
}

void SdThumbs::unload() {
  if (offs) { free(offs); offs = nullptr; }
  if (thumbsFile) thumbsFile.close();
  thumbCacheClear();
  loaded = false;
  count = 0;
  size = 0;
}

bool SdThumbs::load() {
  unload();
  if (!sdReady) return false;
  thumbsFile = SD.open("/mons/thumbs.bin", FILE_READ);
  if (!thumbsFile) {
    Serial.println("sin thumbs.bin (galeria sin miniaturas)");
    return false;
  }
  uint32_t sz = thumbsFile.size();
  uint8_t hdr[6];
  if (sz < 10 || sz > 2UL * 1024 * 1024 || thumbsFile.read(hdr, 6) != 6 || memcmp(hdr, "TPTH", 4) != 0) {
    Serial.println("thumbs.bin invalido");
    unload();
    return false;
  }
  memcpy(&count, hdr + 4, 2);
  if (count == 0 || count > 1024 || (uint32_t)6 + 4UL * count > sz) {
    Serial.println("thumbs.bin invalido (tabla de offsets)");
    unload();
    return false;
  }
  offs = (uint32_t *)malloc(4UL * count);
  if (!offs || thumbsFile.read((uint8_t *)offs, 4UL * count) != (int)(4UL * count)) {
    unload();
    return false;
  }
  size = sz;
  loaded = true;
  Serial.printf("miniaturas: %u (leidas bajo demanda)\n", count);
  return true;
}

// The pointer stays valid until THUMB_SLOTS other thumbnails have been asked
// for; every caller draws it straight away.
const uint8_t *SdThumbs::get(int16_t dex) const {
  if (!loaded || dex < 1 || dex > count) return nullptr;
  for (auto &c : tcache)
    if (c.dex == dex && c.buf) return c.buf;
  if (!thumbsFile) {  // closed for an SD write: reopen
    thumbsFile = SD.open("/mons/thumbs.bin", FILE_READ);
    if (!thumbsFile) return nullptr;
  }
  uint32_t off = offs[dex - 1];
  if (off > size - 3) return nullptr;
  uint8_t h3[3];
  if (!thumbsFile.seek(off) || thumbsFile.read(h3, 3) != 3) return nullptr;
  uint32_t need = 3 + (uint32_t)h3[2] * 2 + (uint32_t)h3[0] * h3[1];
  if (need > THUMB_MAX || need > size || off > size - need) return nullptr;
  auto &slot = tcache[tnext];
  tnext = (tnext + 1) % THUMB_SLOTS;
  if (slot.buf) free(slot.buf);
  slot.dex = 0;
  slot.buf = (uint8_t *)malloc(need);
  if (!slot.buf) return nullptr;
  memcpy(slot.buf, h3, 3);
  if (thumbsFile.read(slot.buf + 3, need - 3) != (int)(need - 3)) {
    free(slot.buf);
    slot.buf = nullptr;
    return nullptr;
  }
  slot.dex = dex;
  return slot.buf;
}

// ---------------------------------------------------------------------------
bool sdBegin() {
  SPI.setRX(SD_MISO);
  SPI.setTX(SD_MOSI);
  SPI.setSCK(SD_SCK);
  sdReady = SD.begin(SD_CS, SD_SCK_MHZ(25), SPI);
  if (!sdReady) sdReady = SD.begin(SD_CS, SD_SCK_MHZ(8), SPI);  // slower cards / long traces
  if (sdReady) {
    Serial.printf("SD montada: %llu MB\n", (unsigned long long)(SD.size64() / (1024ULL * 1024ULL)));
    if (!SD.exists("/mons")) SD.mkdir("/mons");
  } else {
    Serial.println("SD no detectada: usa una tarjeta FAT32 (el juego usa los sprites de flash)");
  }
  return sdReady;
}

// legacy TPK1 path: not used on the RP2350 (PMD sprites cover every species)
bool SdMon::load(uint16_t, bool) { return false; }
void SdMon::unload() { data = nullptr; loaded = false; }

// ---------------------------------------------------------------------------
// USB upload protocol (tools/send_sd.py, web installer "Load sprites"):
//   PUT <ruta> <bytes>\n  + datos crudos   -> "OK" ... "#" por bloque ... "DONE"
//   LS\n                                   -> listado de /mons
// ---------------------------------------------------------------------------
bool sdSerialCommand(const String &line) {
  if (line.startsWith("PUT ")) {
    sdReleaseFiles();  // before any write to the card
    int sp = line.lastIndexOf(' ');
    String path = line.substring(4, sp);
    uint32_t size = line.substring(sp + 1).toInt();
    if (!sdReady || size == 0 || size > 4 * 1024 * 1024) {
      Serial.println("ERR");
      return true;
    }
    if (!path.startsWith("/")) path = "/" + path;
    if (!path.startsWith("/mons/") || path.indexOf("..") >= 0) {
      Serial.println("ERR");
      return true;
    }
    if (SD.exists(path)) SD.remove(path);
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
      Serial.println("ERR");
      return true;
    }
    Serial.println("OK");
    static uint8_t buf[2048];
    uint32_t remaining = size;
    Serial.setTimeout(5000);
    while (remaining > 0) {
      size_t want = remaining > sizeof(buf) ? sizeof(buf) : remaining;
      size_t n = Serial.readBytes(buf, want);
      if (n == 0) break;  // timeout
      if (f.write(buf, n) != n) break;
      remaining -= n;
      Serial.println("#");
    }
    f.close();
    Serial.setTimeout(1000);
    sdDirty = (remaining == 0);
    Serial.println(remaining == 0 ? "DONE" : "ERR");
    return true;
  } else if (line == "SDINFO") {
    FSInfo info;
    if (SDFS.info(info))
      Serial.printf("total=%llu used=%llu\n", (unsigned long long)SD.size64(),
                    (unsigned long long)info.usedBytes);
    Serial.println("DONE");
    return true;
  } else if (line == "LS") {
    File dir = SD.open("/mons");
    if (dir) {
      File e;
      while ((e = dir.openNextFile())) {
        Serial.printf("%s %u\n", e.name(), (uint32_t)e.size());
        e.close();
      }
      dir.close();
    }
    Serial.println("DONE");
    return true;
  }
  return false;
}
#endif
