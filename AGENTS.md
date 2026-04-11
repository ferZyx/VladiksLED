# AGENTS.md

## Repo At A Glance
- Single PlatformIO firmware project for ESP8266 (`d1_mini`); no monorepo/package boundaries.
- Runtime entrypoint is `src/main.cpp` (`setup()` + `loop()`).
- HTTP/WebSocket API is wired in `src/webserver.cpp`; embedded web UI lives in `src/webpage.h`.
- Persistent device state and EEPROM migration logic are in `src/led_state.h` + `src/led_state.cpp`.
- LED effects are implemented in `src/led_modes.cpp` and selected via `runMode()` / `runBacklightMode()`.

## Canonical Commands
- Build firmware: `pio run -e d1_mini`
- Clean rebuild (if build cache gets weird): `pio run -e d1_mini -t clean && pio run -e d1_mini`
- Upload firmware: `pio run -e d1_mini -t upload`
- Serial monitor: `pio device monitor -b 115200`
- There is no repo test/lint/typecheck pipeline; `pio run` is the main automated verification step.

## Critical Repo Conventions
- Treat `src/config.h` as machine/local-network config (WiFi credentials, static IP, OTA settings); do not casually commit credential/network-only edits.
- Do not call `saveLEDState()` in request handlers/hot paths; set `settingsChanged = true` and let `src/main.cpp` persist in the main loop.
- Auto-switch mode transitions are intentionally not saved to EEPROM (to avoid watchdog/crash issues).
- In `setupWebServer()` keep specific routes registered before generic ones (example: `/api/mode/settings` before `/api/mode`).
- For POST body handlers that use chunked uploads, preserve the complete-body guard (`if (index + len != total) return;`) before JSON parsing.

## Mode Change Checklist
- Keep garland mode wiring synchronized across all four locations:
  - `src/config.h` (`TOTAL_MODES`)
  - `src/led_modes.h` + `src/led_modes.cpp` (`runMode()` switch)
  - `src/main.cpp` (`MODE_NAMES[]`)
  - `src/webpage.h` (`modeNames` array)
- Keep backlight mode wiring synchronized across:
  - `src/config.h` (`TOTAL_BACKLIGHT_MODES`)
  - `src/led_modes.cpp` (`runBacklightMode()`)
  - `src/webpage.h` backlight mode UI/labels
- Keep mode index UI dynamic (`id="currentModeIndex"` starts as `(-/-)`); do not hardcode totals in HTML/JS.
- New effect functions should update `leds[]`; frame output (`FastLED.show()`) is centralized in `src/main.cpp`.

## Existing Instruction Sources
- `rules/create-mode.rule.md` contains the required cross-file procedure for adding/removing LED modes.
- `.github/copilot-instructions.md` has useful architecture notes; when it conflicts with code, trust executable source files.
