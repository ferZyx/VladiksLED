# VladiksLED - WiFi LED Garland Copilot Instructions

## Project Overview

ESP8266-based WiFi LED garland controller with web interface. Uses PlatformIO + Arduino framework targeting Wemos D1 Mini. Firmware controls WS2812B LED strips via FastLED with a Tailwind CSS web UI served from embedded HTML.

## Architecture

### Core Components

- **main.cpp** - Entry point: WiFi connection, OTA setup, NTP/HTTP time sync, main loop orchestration
- **led_state.h/.cpp** - Global `LEDState` struct persisted to EEPROM (modes, schedules, settings)
- **led_modes.cpp** - 10 LED animation modes (fire, plasma, confetti, etc.) using FastLED
- **webserver.cpp** - AsyncWebServer REST API + WebSocket for real-time log streaming
- **webpage.h** - Full HTML/JS/Tailwind UI stored as PROGMEM string literal
- **logger.h/.cpp** - Ring buffer logger with WebSocket broadcast (`LOG_PRINT`/`LOG_PRINTLN` macros)
- **diagnostics.h/.cpp** - Loop timing diagnostics for debugging performance issues

### Data Flow

1. Web UI → REST API (JSON) → `ledState` struct → EEPROM save (debounced via `settingsChanged` flag)
2. Main loop: OTA → WebServer → Schedules → LED animations (20ms intervals) → Auto-switch logic
3. Time sync: NTP primary, HTTP fallback (worldtimeapi.org), browser fallback

## Key Patterns

### State Management

```cpp
extern LEDState ledState;           // Global state in led_state.h
extern volatile bool settingsChanged; // Set true to trigger EEPROM save in main loop
```

Never call `saveLEDState()` directly in handlers - set `settingsChanged = true` instead to batch writes.

### EEPROM Safety

- Magic number `0x4C454456` ("LEDV") validates stored data
- Version field for migration support
- Auto-switch does NOT save to EEPROM (prevents wear/crashes)

### Mode Settings Structure

Each of 10 modes has: `speed`, `scale`, `color1`, `color2`, `brightness`, `archived`
Archived modes are skipped during auto-switch.

### Throttling

`MIN_REQUEST_INTERVAL` (100ms) prevents API spam. Check via `checkThrottle()` in handlers.

## Build & Deploy

### Commands

```bash
# Build
pio run

# Upload via USB
pio run --target upload

# Upload via OTA (after first flash)
pio run --target upload  # Uses upload_port: 192.168.1.222

# Monitor serial
pio device monitor
```

### OTA Updates

Configured in `platformio.ini` with static IP `192.168.1.222`. Update `config.h` for different network.

## Configuration (src/config.h)

- `WIFI_SSID` / `WIFI_PASSWORD` - Network credentials
- `USE_STATIC_IP`, `STATIC_IP` - Static IP configuration
- `LED_PIN` (GPIO2/D4), `MAX_LEDS` (300), `DEFAULT_LEDS` (50)
- `NTP_OFFSET` - Timezone offset (18000 = UTC+5)

## Web Interface Notes

- Single-page app in `webpage.h` as raw string literal
- Uses CDN Tailwind CSS (requires internet on client)
- WebSocket at `/ws/logs` for live log streaming
- API endpoints prefixed `/api/` (see webserver.cpp for full list)

## Adding New LED Modes

1. Add mode function in `led_modes.cpp` (use `ledState.modeSettings[mode].speed/scale`)
2. Add case in `runMode()` switch statement
3. Increment `TOTAL_MODES` in `config.h`
4. Add mode name to `MODE_NAMES[]` in `main.cpp`
5. Update frontend modes array in `webpage.h`

## Common Issues

- **Watchdog resets**: Avoid EEPROM writes in interrupt context or rapid succession
- **Heap fragmentation**: Use `snprintf` over String concatenation in loops
- **Time sync fails**: Falls back to HTTP API, then browser-set time via `/api/time/set`
