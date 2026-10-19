#include "ollama_api.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <math.h>

#include "gts_root_r1.h"
#include "secrets.h"

namespace {

constexpr char kBalanceUrl[] = "https://ollama.com/api/balance";

}  // namespace

BalanceResponse fetchOllamaBalance() {
  BalanceResponse out;

  WiFiClientSecure client;
  client.setCACert(GTS_ROOT_R1_PEM);
  client.setHandshakeTimeout(15);

  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(10000);

  if (!http.begin(client, kBalanceUrl)) {
    Serial.println("[api] http.begin failed");
    return out;
  }

  http.addHeader("Authorization", String("Bearer ") + OLLAMA_API_KEY);
  http.addHeader("User-Agent", "esp32-ollama-display/1.0");

  const int code = http.GET();
  out.httpCode = code;

  if (code == 200) {
    const String payload = http.getString();
    Serial.printf("[api] HTTP 200, %u bytes\n", (unsigned)payload.length());

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.printf("[api] JSON error: %s\n", err.c_str());
      out.result = ApiResult::ParseError;
      http.end();
      return out;
    }

    float usd = NAN;
    JsonVariant purchased = doc["purchased"];
    if (!purchased.isNull()) {
      JsonVariant v = purchased["balance_usd"];
      if (!v.isNull()) usd = v.as<float>();
    }
    if (isnan(usd)) {
      JsonVariant included = doc["included"];
      if (!included.isNull()) {
        JsonVariant v = included["balance_usd"];
        if (!v.isNull()) usd = v.as<float>();
      }
    }
    if (isnan(usd)) {
      JsonVariant v = doc["balance_usd"];
      if (!v.isNull()) usd = v.as<float>();
    }

    if (isnan(usd)) {
      Serial.printf("[api] no balance field in: %.200s\n", payload.c_str());
      out.result = ApiResult::ParseError;
    } else {
      out.result = ApiResult::Ok;
      out.balanceUsd = usd;
    }
  } else if (code == 401 || code == 403) {
    Serial.printf("[api] HTTP %d: check the API key\n", code);
    out.result = ApiResult::AuthError;
  } else if (code == 429) {
    const String retryAfter = http.header("Retry-After");
    out.retryAfterSec =
        retryAfter.length() > 0 ? (uint32_t)retryAfter.toInt() : 0;
    Serial.printf("[api] HTTP 429, retry after %lu s\n",
                  (unsigned long)out.retryAfterSec);
    out.result = ApiResult::RateLimited;
  } else {
    Serial.printf("[api] HTTP %d\n", code);
    out.result = ApiResult::NetworkError;
  }

  http.end();
  return out;
}
