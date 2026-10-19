#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <math.h>
#include <time.h>

#include "config.h"
#include "display.h"
#include "ollama_api.h"
#include "secrets.h"

namespace {

Preferences prefs;

float lastUsd = 0.0f;
bool haveValue = false;

UiState state = UiState::Booting;

uint32_t bootMs = 0;
uint32_t lastSuccessMs = 0;
uint32_t nextPollMs = 0;
uint32_t lastDrawMs = 0;
uint32_t lastPulseMs = 0;
uint32_t lastWifiAttemptMs = 0;

bool pulseOn = false;
bool wasConnected = false;
bool ntpStarted = false;
bool timeSynced = false;
bool buttonDown = false;
uint32_t lastButtonMs = 0;

uint32_t ageSeconds() {
  if (!haveValue) return 0;
  return (millis() - lastSuccessMs) / 1000;
}

void draw() {
  const bool online = state == UiState::Online;
  displayDraw(lastUsd, haveValue, state, online ? ageSeconds() : 0, pulseOn);
  lastDrawMs = millis();
}

void serviceButton() {
  const uint32_t now = millis();
  const bool down = digitalRead(cfg::PIN_BUTTON) == LOW;
  if (down == buttonDown) return;
  if (now - lastButtonMs < 40) return;  // debounce

  lastButtonMs = now;
  buttonDown = down;
  if (!down) return;  // act on press only

  const uint8_t rotation = displayToggleRotation();
  prefs.putUChar("rotation", rotation);
  draw();
  Serial.printf("[ui] rotation %u\n", (unsigned)rotation);
}

void beginWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("ollama-widget");
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttemptMs = millis();
  Serial.printf("[wifi] connecting to '%s'\n", WIFI_SSID);
}

void serviceNtp() {
  if (timeSynced) return;

  static uint32_t lastAttemptMs = 0;
  const uint32_t now = millis();
  if (!ntpStarted || now - lastAttemptMs > 30000) {
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    ntpStarted = true;
    lastAttemptMs = now;
    Serial.println("[ntp] sync started");
  }

  const time_t t = time(nullptr);
  if (t > 1700000000) {
    timeSynced = true;
    Serial.printf("[ntp] clock set to %lld\n", (long long)t);
  }
}

void saveBalanceIfChanged() {
  float saved = -1.0f;
  if (prefs.isKey("last_usd")) saved = prefs.getFloat("last_usd", -1.0f);
  if (fabsf(saved - lastUsd) >= 0.005f) {
    prefs.putFloat("last_usd", lastUsd);
  }
}

void applyResult(const BalanceResponse &r) {
  switch (r.result) {
    case ApiResult::Ok:
      lastUsd = r.balanceUsd;
      haveValue = true;
      lastSuccessMs = millis();
      state = UiState::Online;
      saveBalanceIfChanged();
      nextPollMs = millis() + cfg::POLL_INTERVAL_MS;
      Serial.printf("[api] balance $%.2f\n", lastUsd);
      break;

    case ApiResult::AuthError:
      state = UiState::AuthError;
      nextPollMs = millis() + cfg::POLL_INTERVAL_MS;
      break;

    case ApiResult::RateLimited: {
      state = UiState::RateLimited;
      uint32_t waitMs = cfg::POLL_INTERVAL_MS;
      if (r.retryAfterSec > 0) {
        waitMs = r.retryAfterSec * 1000UL + 2000UL;
        if (waitMs < cfg::POLL_INTERVAL_MS) waitMs = cfg::POLL_INTERVAL_MS;
      }
      nextPollMs = millis() + waitMs;
      break;
    }

    default:
      state = UiState::Offline;
      nextPollMs = millis() + cfg::POLL_INTERVAL_MS;
      Serial.printf("[api] failed (HTTP %d, result %u)\n", r.httpCode,
                    (unsigned)r.result);
      break;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  bootMs = millis();
  Serial.println("\n[boot] Ollama balance display");

  prefs.begin("ollama", false);
  if (prefs.isKey("last_usd")) {
    lastUsd = prefs.getFloat("last_usd", 0.0f);
    haveValue = true;
    Serial.printf("[boot] last known balance $%.2f\n", lastUsd);
  }

  const uint8_t rotation = prefs.getUChar("rotation", 0);
  displayBegin(rotation);

  pinMode(cfg::PIN_BUTTON, INPUT_PULLUP);
  buttonDown = digitalRead(cfg::PIN_BUTTON) == LOW;
  lastButtonMs = millis();

  displayDraw(lastUsd, haveValue, UiState::Booting, 0, false);

  beginWifi();
  nextPollMs = millis();
}

void loop() {
  const uint32_t now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;

  serviceButton();

  if (connected) {
    if (!wasConnected) {
      Serial.printf("[wifi] connected, IP %s\n",
                    WiFi.localIP().toString().c_str());
      nextPollMs = now;
      wasConnected = true;
    }

    serviceNtp();

    // mbedTLS validates certificate dates, so wait briefly for SNTP before
    // the first request. After 20 s, try anyway.
    const bool clockReady = timeSynced || (now - bootMs > 20000);
    if (clockReady && (int32_t)(now - nextPollMs) >= 0) {
      const BalanceResponse r = fetchOllamaBalance();
      applyResult(r);
      draw();
    }
  } else {
    wasConnected = false;

    if (state == UiState::Online || state == UiState::AuthError ||
        state == UiState::RateLimited) {
      state = UiState::Offline;
      draw();
    } else if (state == UiState::Booting && now - bootMs > 15000) {
      state = UiState::Offline;
      draw();
    }

    if (now - lastWifiAttemptMs > cfg::WIFI_RETRY_MS) {
      Serial.println("[wifi] retrying");
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      lastWifiAttemptMs = now;
    }
  }

  const bool empty =
      state == UiState::Online && haveValue && lastUsd < 0.005f;

  if (empty) {
    if (now - lastPulseMs >= 500) {
      pulseOn = !pulseOn;
      lastPulseMs = now;
      draw();
    }
  } else {
    pulseOn = false;
    if (state == UiState::Online && now - lastDrawMs >= cfg::AGE_REFRESH_MS) {
      draw();
    }
  }

  delay(10);
}
