# Ollama Balance Display

ESP32-PICO-D4 + Waveshare 1.28" round LCD (GC9A01, 240×240) showing the
prepaid **Ollama Cloud balance in USD**, with a ring gauge that turns
green → amber → red as the balance drops.

The ESP32 polls the documented `GET https://ollama.com/api/balance` endpoint
directly over TLS every 60 seconds. No PC helper or browser session is needed.

## Wiring

Display module pins: `VCC GND DIN CLK CS DC RST BL` (3.3 V only).

| Display | ESP32-PICO-D4 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| DIN (MOSI) | GPIO23 |
| CLK (SCK) | GPIO18 |
| CS | GPIO5 |
| DC | GPIO21 |
| RST | GPIO22 |
| BL | 3V3 (always on) |

No MISO, no touch. **Never use 5 V.**

## Setup

1. Create a new API key at <https://ollama.com/settings/keys>.
   If a key was ever shared or committed, revoke it first.
2. Copy the example secrets file and fill it in:

   ```bash
   cp include/secrets.example.h include/secrets.h
   ```

   Edit `include/secrets.h`:

   ```cpp
   #define WIFI_SSID "your-2.4ghz-ssid"
   #define WIFI_PASSWORD "your-wifi-password"
   #define OLLAMA_API_KEY "your-new-api-key"
   ```

   `secrets.h` is gitignored. Never commit it.

3. Install the PlatformIO IDE extension (optional if you use the `pio` CLI):

   ```bash
   code --install-extension platformio.platformio-ide
   ```

4. Confirm the key works from the PC (keep the key in an environment variable,
   do not paste it into chat or commit it):

   ```bash
   curl -s -H "Authorization: Bearer $OLLAMA_KEY" \
     https://ollama.com/api/balance
   ```

   Expected shape:

   ```json
   {
     "included": { "balance_usd": 0, "allowance_usd": 0, "period": { ... } },
     "purchased": { "balance_usd": 6.37 }
   }
   ```

## Build and flash

```bash
pio run                 # compile
pio run -t upload       # flash over micro-USB
pio device monitor -b 115200
```

If upload fails at 921600 baud, set `upload_speed = 460800` (or `115200`)
in `platformio.ini`.

## Display

- Ring: balance ÷ `FULL_USD` (`10.00`), starting at 12 o'clock, clockwise.
- Green ≥ $5.00, amber $2.00–$4.99, red < $2.00.
- Center: balance in dollars. `LOW` appears below $2.00.
- Bottom: `updated Ns ago` plus a status dot.
- `$0.00` pulses red with `TOP UP`.
- Offline: last known balance grayed out with `OFFLINE`; recovers automatically.
- `CHECK API KEY` means the key is invalid or revoked.
- Press the board's **BOOT button** (GPIO0) to flip the screen 180°. The chosen
  orientation is saved in NVS and survives reboots. Holding BOOT while
  resetting still enters the serial bootloader, as usual.

Tune the scale and bands in `include/config.h`:

```cpp
constexpr float FULL_USD = 10.00f;
constexpr float GREEN_USD = 5.00f;
constexpr float LOW_USD = 2.00f;
```

## Behavior notes

- Poll interval: 60 s. Ollama limits the balance endpoint to 10 requests per
  minute per account (shared across keys/devices); `Retry-After` is honored.
- The last successful balance is stored in NVS, so a reboot while offline
  still shows the last known value.
- TLS is validated against the Google Trust Services Root R1 certificate
  (`include/gts_root_r1.h`). SNTP is used to set the clock before the first
  request, because certificate validation checks dates.
- The API key is included in every request; treat the physical device as
  trusted. Revoke the key if the device is lost.

## Troubleshooting

| Symptom | Check |
|---|---|
| Black screen | RST/DC/CS wiring; `BL` tied to 3V3; 3.3 V supply |
| Red/blue swapped | `cfg.rgb_order` in `src/display.cpp` |
| Crash during display init | SPI host/pin config in `src/display.cpp` |
| `CONNECTING...` forever | 2.4 GHz WiFi only; WPA2; correct credentials |
| `OFFLINE` with WiFi connected | Serial log HTTP code; TLS/CA; try `curl` from PC |
| `CHECK API KEY` | Key revoked or mistyped in `secrets.h` |
| `RATE LIMITED` | Another device is polling too often |
| Balance looks stale | Serial shows poll results once per minute |

## API reference

- Balance: <https://docs.ollama.com/api/balance>
- Usage history: <https://docs.ollama.com/api/usage>
- Keys: <https://ollama.com/settings/keys>
