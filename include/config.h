#pragma once

#include <stdint.h>

namespace cfg {

// Waveshare 1.28" GC9A01 module wired to an ESP32-PICO-D4.
// BL is tied to 3V3 (backlight always on), so there is no backlight GPIO.
constexpr int PIN_SCLK = 18;  // CLK
constexpr int PIN_MOSI = 23;  // DIN
constexpr int PIN_CS   = 5;
constexpr int PIN_DC   = 21;
constexpr int PIN_RST  = 22;

// BOOT button on common ESP32 dev boards. Active LOW.
constexpr int PIN_BUTTON = 0;

constexpr uint32_t SPI_FREQ = 40000000;

// Ring scale: a full ring means this much prepaid balance.
constexpr float FULL_USD = 10.00f;
// Ring color bands.
constexpr float GREEN_USD = 5.00f;  // >= this: green
constexpr float LOW_USD   = 2.00f;  // >= this: amber, below: red

constexpr uint32_t POLL_INTERVAL_MS = 60000;
constexpr uint32_t AGE_REFRESH_MS   = 5000;
constexpr uint32_t WIFI_RETRY_MS    = 10000;

}  // namespace cfg
