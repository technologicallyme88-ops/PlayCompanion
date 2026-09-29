#pragma once

#include <cstdint>

// ESP-IDF supplies this on-device. The simulator only needs changing values
// for non-security UI choices such as the rotating Home quote, so a small,
// deterministic generator keeps host runs reproducible without heap state.
inline uint32_t esp_random() {
  static uint32_t state = 0x6D2B79F5u;
  state = state * 1664525u + 1013904223u;
  return state;
}
