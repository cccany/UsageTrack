#include "display.h"

#include <LovyanGFX.hpp>

#include "config.h"

namespace {

class DisplayDevice : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 panel_;
  lgfx::Bus_SPI bus_;

 public:
  DisplayDevice() {
    {
      auto cfg = bus_.config();
      cfg.spi_host = SPI3_HOST;  // VSPI
      cfg.spi_mode = 0;
      cfg.freq_write = cfg::SPI_FREQ;
      cfg.freq_read = 16000000;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.use_lock = true;
      cfg.pin_sclk = cfg::PIN_SCLK;
      cfg.pin_mosi = cfg::PIN_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = cfg::PIN_DC;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
      cfg.pin_cs = cfg::PIN_CS;
      cfg.pin_rst = cfg::PIN_RST;
      cfg.pin_busy = -1;
      cfg.panel_width = 240;
      cfg.panel_height = 240;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;  // BGR
      cfg.dlen_16bit = false;
      panel_.config(cfg);
    }
    setPanel(&panel_);
  }
};

DisplayDevice lcd;

uint8_t currentRotation = 0;

constexpr int CX = 120;
constexpr int CY = 120;
constexpr int R_OUT = 112;
constexpr int R_IN = 96;
constexpr float START_DEG = 270.0f;  // 12 o'clock; angles increase clockwise

// LovyanGFX draws with the panel's native RGB565 values.
constexpr uint16_t C_BG = 0x0000;
constexpr uint16_t C_TRACK = 0x2104;
constexpr uint16_t C_GREEN = 0x07E0;
constexpr uint16_t C_AMBER = 0xFD20;
constexpr uint16_t C_RED = 0xF800;
constexpr uint16_t C_DARK_RED = 0x6000;
constexpr uint16_t C_GRAY = 0x8410;
constexpr uint16_t C_WHITE = 0xFFFF;

uint16_t ringColor(float usd) {
  if (usd >= cfg::GREEN_USD) return C_GREEN;
  if (usd >= cfg::LOW_USD) return C_AMBER;
  return C_RED;
}

void drawRing(float usd, bool haveValue, UiState state, bool pulseOn) {
  lcd.fillArc(CX, CY, R_OUT, R_IN, 0.0f, 360.0f, C_TRACK);
  if (!haveValue) return;

  float frac = usd / cfg::FULL_USD;
  if (frac < 0.0f) frac = 0.0f;
  if (frac > 1.0f) frac = 1.0f;

  const bool empty = usd < 0.005f;
  if (empty) {
    lcd.fillArc(CX, CY, R_OUT, R_IN, 0.0f, 360.0f,
                pulseOn ? C_RED : C_DARK_RED);
    return;
  }

  uint16_t color = (state == UiState::Online) ? ringColor(usd) : C_GRAY;
  lcd.fillArc(CX, CY, R_OUT, R_IN, START_DEG, START_DEG + 360.0f * frac,
              color);
}

void drawValue(float usd, bool haveValue, UiState state) {
  char buf[24];
  if (!haveValue) {
    snprintf(buf, sizeof(buf), "--");
  } else {
    snprintf(buf, sizeof(buf), "$%.2f", usd);
  }

  uint16_t color = C_WHITE;
  if (!haveValue || state != UiState::Online) {
    color = C_GRAY;
  } else if (usd < 0.005f) {
    color = C_RED;
  }

  lcd.setFont(&lgfx::fonts::FreeSansBold24pt7b);
  if (lcd.textWidth(buf) > 190) {
    lcd.setFont(&lgfx::fonts::FreeSansBold18pt7b);
  }
  lcd.setTextColor(color, C_BG);
  lcd.drawString(buf, CX, 108);

  if (haveValue && state == UiState::Online && usd >= 0.005f &&
      usd < cfg::LOW_USD) {
    lcd.setFont(&lgfx::fonts::FreeSansBold9pt7b);
    lcd.setTextColor(C_RED, C_BG);
    lcd.drawString("LOW", CX, 140);
  }
}

void drawBottomLine(float usd, bool haveValue, UiState state, uint32_t ageSec) {
  char text[40];
  uint16_t dotColor = C_GRAY;
  uint16_t textColor = C_WHITE;

  switch (state) {
    case UiState::Booting:
      snprintf(text, sizeof(text), "CONNECTING...");
      dotColor = C_AMBER;
      break;

    case UiState::Online:
      if (haveValue && usd < 0.005f) {
        snprintf(text, sizeof(text), "TOP UP");
        dotColor = C_RED;
        textColor = C_RED;
      } else if (ageSec < 120) {
        snprintf(text, sizeof(text), "updated %lus ago",
                 (unsigned long)ageSec);
        dotColor = C_GREEN;
      } else {
        snprintf(text, sizeof(text), "updated %lum ago",
                 (unsigned long)(ageSec / 60));
        dotColor = C_GREEN;
      }
      break;

    case UiState::Offline:
      snprintf(text, sizeof(text), "OFFLINE");
      dotColor = C_GRAY;
      textColor = C_GRAY;
      break;

    case UiState::AuthError:
      snprintf(text, sizeof(text), "CHECK API KEY");
      dotColor = C_RED;
      textColor = C_RED;
      break;

    case UiState::RateLimited:
      snprintf(text, sizeof(text), "RATE LIMITED");
      dotColor = C_AMBER;
      textColor = C_AMBER;
      break;
  }

  lcd.fillCircle(CX, 158, 3, dotColor);
  lcd.setFont(&lgfx::fonts::Font2);
  lcd.setTextColor(textColor, C_BG);
  lcd.drawString(text, CX, 180);
}

}  // namespace

void displayBegin(uint8_t rotation) {
  lcd.init();
  currentRotation = (rotation == 2) ? 2 : 0;
  lcd.setRotation(currentRotation);
  lcd.fillScreen(C_BG);
  lcd.setTextDatum(lgfx::textdatum_t::middle_center);
}

uint8_t displayToggleRotation() {
  currentRotation = (currentRotation == 0) ? 2 : 0;
  lcd.setRotation(currentRotation);
  lcd.fillScreen(C_BG);
  return currentRotation;
}

void displayDraw(float usd, bool haveValue, UiState state, uint32_t ageSec,
                 bool pulseOn) {
  drawRing(usd, haveValue, state, pulseOn);

  // Clear the inner disc so the previous text is removed without touching
  // the ring band.
  lcd.fillCircle(CX, CY, R_IN - 2, C_BG);

  drawValue(usd, haveValue, state);
  drawBottomLine(usd, haveValue, state, ageSec);
}
