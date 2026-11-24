Global animation speed + output smoothing (Nov 2026, EEPROM v5).

State (src/led_state.h, appended at END of LEDState so v4 prefix stays valid): garlandSpeed, backlightSpeed (percent, ANIM_SPEED_MIN 10 .. ANIM_SPEED_MAX 250, default 100), garlandSmoothing (0 = off .. 255, default GARLAND_SMOOTHING_DEFAULT 120). EEPROM_VERSION = 5; loadLEDState migrates v4->v5 by reading the struct and calling initAnimationDefaults(); v3/v1 migrations also call it; current-version load runs sanitizeAnimationSettings().

API: GET /api/state includes garlandSpeed/backlightSpeed/garlandSmoothing. POST /api/animation with any of those keys (clamped). UI (webpage.h): garland panel card "Анимация" (speed + smoothing sliders), backlight panel "Общая скорость" slider; updateAnimation() always sends all three values (safe with request coalescing).

Virtual animation clock (src/led_modes.cpp): main loop calls advanceAnimationClock(speedPercent of current operation mode) once per frame. Effects must use animMillis() instead of millis(); FastLED 3.10 beatsin8/beatsin16 read fl::millis() directly (USE_GET_MILLISECOND_TIMER hook no longer works), so always pass timebase = animTimebase(). Per-frame increments use animSteps(floatAcc, perReferenceFrame) (reference frame = 20 ms). Backlight color transitions (getBacklightColor) intentionally stay on real millis().

Buffers: effects draw into static `frame[]` (inside led_modes.cpp all effect code uses `frame`, not `leds`). `leds[]` is the FastLED output buffer. presentFrame() exponentially moves leds toward frame using 8.8 accumulators smoothAcc[] (tau = garlandSmoothing*1.2 ms in garland mode, 40 ms in backlight; 280 ms crossfade for 900 ms after mode/operation-mode/power change) and returns whether output changed; main keeps calling FastLED.show() while powered or while fading out after power off. Service fills from main.cpp go through fillFrameSolid(). setup() WiFi indicator still writes leds[] + FastLED.show() directly (before the frame loop) — fine.

Anti-flicker fixes: FastLED.setDither(DISABLE_DITHER) in initLEDs; snowfall twinkle is a deterministic sin8 per flake (no per-frame random); aurora per-pixel random flashes removed; plasma uses 16-bit time offset (8-bit wrap caused a noise jump every ~5 s).

When adding a new effect: draw into `frame`, use animMillis()/animTimebase()/animSteps(), never call FastLED.show().
