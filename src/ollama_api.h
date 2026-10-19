#pragma once

#include <stdint.h>

enum class ApiResult : uint8_t {
  Ok,
  AuthError,
  RateLimited,
  NetworkError,
  ParseError,
};

struct BalanceResponse {
  ApiResult result = ApiResult::NetworkError;
  float balanceUsd = 0.0f;
  uint32_t retryAfterSec = 0;
  int httpCode = 0;
};

// GET https://ollama.com/api/balance with the API key from secrets.h.
BalanceResponse fetchOllamaBalance();
