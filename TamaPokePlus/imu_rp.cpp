#include "imu_rp.h"
#include "pin_config.h"
#include <Wire.h>
#include <math.h>

// QMI8658 registers (datasheet rev. A)
#define QMI_WHO_AM_I 0x00   // reads 0x05
#define QMI_CTRL1 0x02      // bit6: register auto-increment
#define QMI_CTRL2 0x03      // accel full scale [6:4], output rate [3:0]
#define QMI_CTRL7 0x08      // bit0: accel enable
#define QMI_AX_L 0x35       // AX_L..AZ_H, little endian
#define QMI_RESET 0x60      // write 0xB0: soft reset

static bool ok = false;
static uint8_t addr = 0;
static uint32_t lastPoll = 0;
static uint8_t failures = 0;
static float lastG = 0;

// step detector state
static float baseline = 0;
static bool armed = true;
static uint32_t lastPeak = 0;
static uint8_t cadence = 0;
static uint16_t stepBank = 0;
// shake
static bool shakePending = false;
static uint32_t shakeQuietUntil = 0;
static uint32_t ignoreUntil = 0;

static bool wr(uint8_t reg, uint8_t v) {
  I2C_BUS.beginTransmission(addr);
  I2C_BUS.write(reg);
  I2C_BUS.write(v);
  return I2C_BUS.endTransmission() == 0;
}

// reads exactly n bytes into buf (fixed-size caller buffers), false otherwise
static bool rd(uint8_t reg, uint8_t *buf, uint8_t n) {
  I2C_BUS.beginTransmission(addr);
  I2C_BUS.write(reg);
  if (I2C_BUS.endTransmission(false) != 0) return false;
  uint8_t got = I2C_BUS.requestFrom(addr, n);
  if (got != n) {
    while (I2C_BUS.available()) I2C_BUS.read();
    return false;
  }
  for (uint8_t i = 0; i < n; i++) buf[i] = (uint8_t)I2C_BUS.read();
  return true;
}

static bool probe(uint8_t a) {
  addr = a;
  uint8_t id = 0;
  if (!rd(QMI_WHO_AM_I, &id, 1) || id != 0x05) return false;
  wr(QMI_RESET, 0xB0);
  delay(20);
  if (!wr(QMI_CTRL1, 0x40)) return false;  // auto-increment, little endian
  wr(QMI_CTRL2, 0x07);                       // +-2 g, 62.5 Hz
  wr(QMI_CTRL7, 0x01);                       // accelerometer on
  delay(30);
  return true;
}

bool imuBegin() {
  ok = probe(0x6B) || probe(0x6A);
  if (ok) Serial.printf("QMI8658 ok addr=0x%02X\n", addr);
  else Serial.println("QMI8658 not found (no step counter)");
  ignoreUntil = millis() + 1000;
  return ok;
}

bool imuOk() { return ok; }
float imuLastG() { return lastG; }

uint16_t imuTakeSteps() {
  uint16_t n = stepBank;
  stepBank = 0;
  return n;
}

bool imuShakeEdge() {
  bool s = shakePending;
  shakePending = false;
  return s;
}

void imuPoll(uint32_t now) {
  if (!ok) return;
  if (now - lastPoll < 40) return;
  lastPoll = now;
  uint8_t b[6];
  if (!rd(QMI_AX_L, b, 6)) {
    if (++failures >= 25) {  // the bus keeps refusing: stop polling for good
      ok = false;
      Serial.println("QMI8658 stopped answering: step counter off");
    }
    return;
  }
  failures = 0;
  int16_t x = (int16_t)(b[0] | (b[1] << 8));
  int16_t y = (int16_t)(b[2] | (b[3] << 8));
  int16_t z = (int16_t)(b[4] | (b[5] << 8));
  float g = sqrtf((float)x * x + (float)y * y + (float)z * z) / 16384.0f;
  lastG = g;
  if (baseline <= 0.01f) { baseline = g; return; }
  float sig = g - baseline;
  baseline += sig * (fabsf(sig) < 0.08f ? 0.10f : 0.02f);
  if ((int32_t)(now - ignoreUntil) < 0) return;

  // shake: a strong jolt (not walking)
  if ((g > 1.9f || fabsf(sig) > 0.95f) && (int32_t)(now - shakeQuietUntil) >= 0) {
    shakePending = true;
    shakeQuietUntil = now + 1200;
    cadence = 0;  // a shake is not a step
    return;
  }

  // steps: one peak per step, re-armed once the signal settles; at least two
  // rhythmic peaks before anything counts, so single bumps are ignored
  if (!armed) {
    if (sig < 0.03f) armed = true;
    return;
  }
  if (sig < 0.11f) return;
  armed = false;
  uint32_t gap = lastPeak ? now - lastPeak : 0;
  if (lastPeak && gap < 280) return;
  lastPeak = now;
  if (!gap || gap > 1500) { cadence = 1; return; }
  if (cadence == 1) { stepBank += 2; cadence = 2; }
  else { stepBank++; if (cadence < 250) cadence++; }
  if (stepBank > 60000) stepBank = 60000;
}
