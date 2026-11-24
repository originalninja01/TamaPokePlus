#if defined(ARDUINO_ARCH_RP2040)
// RP2350 version of audio.cpp: same sound effects, same ES8311 register
// sequence (slave mode, clock derived from BCLK), but
//  - I2S comes from a small PIO driver (i2s_pio_rp: no DMA/IRQ, see its header),
//    with the RP2350 as master driving BCLK (GPIO4) and LRCLK (GPIO5), data on GPIO1;
//  - the tone synth runs on the second core (setup1/loop1) instead of a
//    FreeRTOS task, fed through a small lock-free queue.
#include "audio.h"
#include "pin_config.h"
#include "prefs_rp.h"
#include <Arduino.h>
#include <Wire.h>
#include "i2s_pio_rp.h"
#include "dex.h"

#define ES8311_ADDR 0x18
#define SAMPLE_RATE 16000

static volatile bool gReady = false;
static volatile bool gOn = true;
static volatile bool gSleeping = false;
static bool gIdle = false;
static uint32_t gAmpHoldUntil = 0;
static bool gAmpHold = false;
static volatile uint32_t gAmpOnAt = 0;
static volatile bool gStartI2S = false;  // core 0 -> core 1: bring I2S up
static volatile bool gI2SUp = false;     // core 1 -> core 0: I2S running

// single-producer (core 0) / single-consumer (core 1) ring of effect ids
#define AQ_LEN 8
#define CHIRP_BASE 1000  // queue ids >= this are species cries (1000 + dex)
#define PAD_BASE 900     // 900..903: memory game pad tones
static volatile uint16_t q[AQ_LEN];
static volatile uint8_t qHead = 0, qTail = 0;

static void updateAmplifierPower() {
  if (gAmpHold && (int32_t)(gAmpHoldUntil - millis()) <= 0) gAmpHold = false;
  bool idleOff = gIdle && !gAmpHold;
  static int last = -1;
  int want = (gReady && gOn && !gSleeping && !idleOff) ? HIGH : LOW;
  if (want != last) {
    last = want;
    if (want == HIGH) gAmpOnAt = millis();
    digitalWrite(PA, want);
  }
}

// ---- I2C del códec ----
static bool esW(uint8_t reg, uint8_t val) {
  I2C_BUS.beginTransmission(ES8311_ADDR);
  I2C_BUS.write(reg);
  I2C_BUS.write(val);
  return I2C_BUS.endTransmission() == 0;
}
static uint8_t esR(uint8_t reg) {
  I2C_BUS.beginTransmission(ES8311_ADDR);
  I2C_BUS.write(reg);
  I2C_BUS.endTransmission(false);
  I2C_BUS.requestFrom(ES8311_ADDR, 1);
  return I2C_BUS.available() ? I2C_BUS.read() : 0;
}

// identical to the ESP32 build (verified there): slave, clock from BCLK,
// 16 kHz, 16-bit I2S
static bool es8311Init() {
  I2C_BUS.beginTransmission(ES8311_ADDR);
  if (I2C_BUS.endTransmission() != 0) return false;

  esW(0x0D, 0xFA); esW(0x44, 0x08); esW(0x44, 0x08);
  esW(0x01, 0x30); esW(0x02, 0x00); esW(0x03, 0x10); esW(0x16, 0x24);
  esW(0x04, 0x10); esW(0x05, 0x00); esW(0x0B, 0x00); esW(0x0C, 0x00);
  esW(0x10, 0x1F); esW(0x11, 0x7F);
  esW(0x00, 0x80); esW(0x00, 0x80);
  esW(0x01, 0xBF);
  { uint8_t r = esR(0x06); r &= ~0x20; esW(0x06, r); }
  esW(0x13, 0x10); esW(0x1B, 0x0A); esW(0x1C, 0x6A);
  esW(0x44, 0x58);

  esW(0x02, 0x18); esW(0x05, 0x00); esW(0x03, 0x10); esW(0x04, 0x20);
  { uint8_t r = esR(0x07); r &= 0xC0; esW(0x07, r); }
  esW(0x08, 0xFF);
  { uint8_t r = esR(0x06); r &= 0xE0; r |= 0x03; esW(0x06, r); }

  esW(0x09, 0x0C); esW(0x0A, 0x0C);

  esW(0x00, 0x80); esW(0x01, 0xBF); esW(0x09, 0x0C); esW(0x0A, 0x0C);
  esW(0x17, 0xBF); esW(0x0E, 0x02); esW(0x12, 0x00); esW(0x14, 0x1A);
  esW(0x0D, 0x01); esW(0x15, 0x40); esW(0x37, 0x08); esW(0x45, 0x00);

  esW(0x32, 0xBF);
  { uint8_t r = esR(0x31); r &= 0x9F; esW(0x31, r); }
  return true;
}

// ---- sintetizador de tono cuadrado (igual que en ESP32) ----
struct Note { uint16_t f, ms; };

static const Note N_TAP[]    = {{880, 35}};
static const Note N_EAT[]    = {{660, 45}, {0, 12}, {660, 45}};
static const Note N_PLAY[]   = {{784, 45}, {988, 60}};
static const Note N_HEART[]  = {{1047, 55}, {1319, 90}};
static const Note N_HATCH[]  = {{523, 80}, {659, 80}, {784, 110}, {1047, 170}};
static const Note N_EVOLVE[] = {{523, 80}, {659, 80}, {784, 80}, {1047, 90}, {1319, 230}};
static const Note N_MEDAL[]  = {{784, 70}, {0, 25}, {784, 70}, {0, 25}, {1047, 200}};
static const Note N_DENY[]   = {{300, 110}, {200, 170}};
static const Note N_BYE[]    = {{784, 150}, {659, 150}, {523, 280}};
static const Note N_LEVEL[]  = {{784, 70}, {1047, 130}};

struct SfxDef { const Note *n; uint8_t len; };
static const SfxDef SFX[SFX_COUNT] = {
  {N_TAP, 1}, {N_EAT, 3}, {N_PLAY, 2}, {N_HEART, 2}, {N_HATCH, 4},
  {N_EVOLVE, 5}, {N_MEDAL, 5}, {N_DENY, 2}, {N_BYE, 3}, {N_LEVEL, 2},
};

static void playTone(uint16_t f, uint16_t ms) {
  int total = SAMPLE_RATE * ms / 1000;
  int half = f ? (SAMPLE_RATE / (2 * f)) : 0;
  const int16_t amp = 5000;
  int phase = 0;
  bool high = true;
  for (int idx = 0; idx < total; idx++) {
    int16_t s = 0;
    if (f) {
      s = high ? amp : -amp;
      if (idx < 64) s = (int16_t)(s * idx / 64);
      else if (idx > total - 96) s = (int16_t)(s * (total - idx) / 96);
      if (++phase >= half) { phase = 0; high = !high; }
    }
    i2sPioWrite(s, s);  // blocks while the PIO FIFO is full: paces the synth
  }
}

// ---- species cries (TamaPoke Plus) ----
// A short synthesized call per species, made up from its number, type and size:
// no recorded or game audio. Square or triangle wave with a pitch slide.
static void playSlide(uint16_t f0, uint16_t f1, uint16_t ms, bool tri) {
  int total = SAMPLE_RATE * ms / 1000;
  if (total < 1) return;
  uint32_t phase = 0;
  const int16_t amp = tri ? 7000 : 4200;
  for (int idx = 0; idx < total; idx++) {
    uint32_t f = f0 + (int32_t)(f1 - f0) * idx / total;
    phase += (f << 16) / SAMPLE_RATE;  // cycles per sample, 16.16 fixed point
    uint16_t ph = (uint16_t)(phase & 0xFFFF);
    int32_t s;
    if (tri) s = ph < 32768 ? (int32_t)ph * 2 - 32768 : 98303 - (int32_t)ph * 2;  // -32768..32767
    else s = ph < 32768 ? 32767 : -32768;
    s = s * amp / 32768;
    if (idx < 48) s = s * idx / 48;
    else if (idx > total - 80) s = s * (total - idx) / 80;
    i2sPioWrite((int16_t)s, (int16_t)s);
  }
}

static int typePitch(uint8_t t) {
  switch (t) {
    case TYPE_ELECTRIC: return 220; case TYPE_FLYING: return 160; case TYPE_ICE: return 170;
    case TYPE_FAIRY: return 190; case TYPE_PSYCHIC: return 120; case TYPE_FIRE: return 100;
    case TYPE_WATER: return -40; case TYPE_GRASS: return -60; case TYPE_BUG: return 60;
    case TYPE_POISON: return -90; case TYPE_ROCK: return -210; case TYPE_GROUND: return -170;
    case TYPE_STEEL: return -120; case TYPE_GHOST: return -100; case TYPE_DRAGON: return -150;
    case TYPE_FIGHTING: return -80; default: return 0;
  }
}

static void playChirp(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return;
  uint32_t h = (uint32_t)dex * 2654435761u;
  int base = 560 + (int)((h >> 8) % 9) * 45 + typePitch(DEX_TBL[dex].type1);
  uint8_t hp = DEX_TBL[dex].bHp;
  base -= hp > 90 ? 170 : (hp > 65 ? 80 : 0);  // big ones sound deeper
  if (base < 180) base = 180;
  bool tri = (h >> 13) & 1;
  uint8_t shape = (h >> 17) % 3;  // 0 rising call, 1 falling call, 2 trill
  uint8_t n = 2 + ((h >> 20) & 1);
  for (uint8_t i = 0; i < n; i++) {
    int f0 = base, f1 = base;
    if (shape == 0) { f0 = base + i * 60; f1 = f0 + 180; }
    else if (shape == 1) { f0 = base + 200 - i * 50; f1 = f0 - 220; }
    else { f0 = base + (i & 1) * 160; f1 = f0 + 40; }
    if (f1 < 150) f1 = 150;
    uint16_t ms = 70 + ((h >> (i * 3)) & 7) * 8 + (i == n - 1 ? 40 : 0);
    playSlide((uint16_t)f0, (uint16_t)f1, ms, tri);
    if (i + 1 < n) i2sPioSilence(18, SAMPLE_RATE);
  }
}

// ---- core 1 ----
void setup1() {
  while (!gStartI2S) delay(5);
  gI2SUp = i2sPioBegin(SAMPLE_RATE);
  if (!gI2SUp) gStartI2S = false;
}

void loop1() {
  if (!gI2SUp) { delay(50); return; }
  // idle: keep feeding silence so the codec always sees a steady BCLK/LRCLK
  if (qHead == qTail) { i2sPioSilence(2, SAMPLE_RATE); return; }
  uint16_t id = q[qTail];
  qTail = (qTail + 1) % AQ_LEN;
  bool chirp = id >= CHIRP_BASE;
  bool pad = id >= PAD_BASE && id < PAD_BASE + 4;
  if (!(gOn && !gSleeping && gReady && (id < SFX_COUNT || chirp || pad))) return;
  int32_t since = (int32_t)(millis() - gAmpOnAt);
  if (since >= 0 && since < 50) i2sPioSilence(50 - since, SAMPLE_RATE);  // NS4150B settling time
  if (chirp) { playChirp((int16_t)(id - CHIRP_BASE)); return; }
  if (pad) { static const uint16_t PF[4] = { 523, 659, 784, 988 }; playTone(PF[id - PAD_BASE], 230); return; }
  const SfxDef &d = SFX[id];
  for (uint8_t i = 0; i < d.len; i++) playTone(d.n[i].f, d.n[i].ms);
}

// ---- API (core 0) ----
void audioBegin() {
  pinMode(PA, OUTPUT);
  digitalWrite(PA, LOW);

  gStartI2S = true;
  uint32_t t0 = millis();
  while (!gI2SUp && millis() - t0 < 500) delay(2);
  if (!gI2SUp) { Serial.println("I2S begin fallo"); return; }
  if (!es8311Init()) { Serial.println("ES8311 no responde (audio off)"); return; }

  Preferences p;
  p.begin("tamapoke", true);
  gOn = p.getBool("snd", true);
  p.end();

  gReady = true;
  updateAmplifierPower();
  sfxPlay(SFX_HATCH);
}

bool audioBusy() { return (gAmpHold && (int32_t)(gAmpHoldUntil - millis()) > 0) || qHead != qTail; }

static void queuePush(uint16_t id) {
  if (!(gReady && gOn)) return;
  if (gIdle) {
    gAmpHoldUntil = millis() + 4000;
    gAmpHold = true;
    updateAmplifierPower();
  }
  uint8_t next = (qHead + 1) % AQ_LEN;
  if (next == qTail) return;  // full: drop (same as the ESP32 queue)
  q[qHead] = id;
  __dmb();
  qHead = next;
}

void sfxPlay(uint8_t id) { queuePush(id); }
void padTonePlay(uint8_t pad) { if (pad < 4) queuePush((uint16_t)(PAD_BASE + pad)); }

void chirpPlay(int16_t dex) {
  if (dex >= 1 && dex <= DEX_COUNT) queuePush((uint16_t)(CHIRP_BASE + dex));
}

void audioSetEnabled(bool on) {
  gOn = on;
  updateAmplifierPower();
  Preferences p;
  p.begin("tamapoke", false);
  p.putBool("snd", on);
  p.end();
}
bool audioEnabled() { return gOn; }

void audioSetSleeping(bool sleeping) {
  if (gSleeping == sleeping) return;
  gSleeping = sleeping;
  updateAmplifierPower();
}

void audioSetIdle(bool idle) {
  gIdle = idle;
  updateAmplifierPower();
}
#endif
