#pragma once
#include <stdint.h>

// TamaPoke Plus: one weather per day, the same all day (worked out from the
// date, so it survives restarts and needs no saving). Shown on the main
// screen and used for Eevee's evolution when no berry colour leads.
enum : uint8_t { WX_SUNNY = 0, WX_CLOUDY, WX_RAIN, WX_STORM, WX_COUNT };

static inline uint8_t weatherForDay(uint32_t day) {
  uint32_t h = day * 2654435761u;
  h ^= h >> 15;
  h *= 2246822519u;
  h ^= h >> 13;
  uint32_t r = h % 100;
  if (r < 35) return WX_SUNNY;
  if (r < 65) return WX_CLOUDY;
  if (r < 90) return WX_RAIN;
  return WX_STORM;
}

static inline uint8_t weatherForEpoch(uint32_t localEpoch) {
  return localEpoch ? weatherForDay(localEpoch / 86400) : WX_CLOUDY;
}
