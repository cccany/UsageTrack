#pragma once

#include <stdint.h>

enum class UiState : uint8_t {
  Booting,
  Online,
  Offline,
  AuthError,
  RateLimited,
};

void displayBegin(uint8_t rotation);
void displayDraw(float usd, bool haveValue, UiState state, uint32_t ageSec,
                 bool pulseOn);

// Flips the logical orientation between 0 and 180 degrees.
// Returns the new rotation (0 or 2).
uint8_t displayToggleRotation();
