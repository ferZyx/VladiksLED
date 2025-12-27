#include "led_modes.h"
#include "led_state.h"

CRGB leds[MAX_LEDS];

void initLEDs() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, MAX_LEDS);
  FastLED.setBrightness(ledState.brightness);
  FastLED.clear();
  FastLED.show();
}

void runMode(uint8_t mode) {
  if (!ledState.power) {
    FastLED.clear();
    FastLED.show();
    return;
  }
  
  // Запуск соответствующего режима
  switch (mode) {
    case 0: mode_blendwave(); break;
    case 1: mode_rainbow_beat(); break;
    case 2: mode_two_sin(); break;
    case 3: mode_confetti(); break;
    case 4: mode_fire(); break;
    case 5: mode_rainbow_march(); break;
    case 6: mode_plasma(); break;
    case 7: mode_noise(); break;
    case 8: mode_juggle(); break;
    case 9: mode_solid_color(); break;
    
    default: mode_rainbow_beat(); break;
  }
}

// Rainbow Beat - радужная волна
void mode_rainbow_beat() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  uint8_t beat = beatsin8(speed / 10, 64, 255);
  
  // Scale controls color spacing (1-10)
  uint8_t colorSpacing = map(scale, 0, 255, 1, 10);
  
  for (int i = 0; i < ledState.numLeds; i++) {
    leds[i] = ColorFromPalette(RainbowColors_p, (i * colorSpacing) + beat, beat);
  }
}

// Blendwave - смешанные волны
void mode_blendwave() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  
  // Scale controls wave density (5-50)
  uint8_t waveDensity = map(scale, 0, 255, 5, 50);
  
  for (int i = 0; i < ledState.numLeds; i++) {
    uint8_t bright = beatsin8(speed / 10, 0, 255, 0, i * waveDensity);
    leds[i] = CHSV((millis() / 20) + i * 5, 255, bright);
  }
}

// Two Sin - две синусоиды
void mode_two_sin() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  static uint8_t hue = 0;
  hue++;
  
  // Scale controls wave frequency (2-20)
  uint8_t waveFreq = map(scale, 0, 255, 2, 20);
  
  for (int i = 0; i < ledState.numLeds; i++) {
    uint8_t bright1 = beatsin8(speed / 15, 0, 255, 0, i * waveFreq);
    uint8_t bright2 = beatsin8(speed / 20, 0, 255, 0, i * (waveFreq + 2) + 128);
    uint8_t bright = (bright1 + bright2) / 2;
    
    leds[i] = CHSV(hue + i * 3, 255, bright);
  }
}

// Confetti - конфетти
void mode_confetti() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  
  // Speed controls fade rate (1-30)
  uint8_t fadeAmount = map(speed, 0, 255, 1, 30);
  fadeToBlackBy(leds, ledState.numLeds, fadeAmount);
  
  // Scale controls number of confetti particles (1-8)
  uint8_t numConfetti = map(scale, 0, 255, 1, 8);
  
  // Speed also affects spawn probability
  uint8_t spawnChance = map(speed, 0, 255, 50, 255);
  
  for (uint8_t i = 0; i < numConfetti; i++) {
    if (random8() < spawnChance) {
      int pos = random16(ledState.numLeds);
      // Use direct assignment to prevent brightness overflow flashes
      leds[pos] = CHSV(random8(), 200, 255);
    }
  }
}

// Fire - огонь
void mode_fire() {
  static byte heat[MAX_LEDS];
  static unsigned long lastUpdate = 0;
  
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  
  // Speed controls update rate (delay between frames: 10-100ms)
  uint8_t updateDelay = map(speed, 0, 255, 100, 10);
  
  unsigned long now = millis();
  if (now - lastUpdate < updateDelay) {
    // Just redraw without updating heat
    for (int j = 0; j < ledState.numLeds; j++) {
      CRGB color = HeatColor(heat[j]);
      leds[j] = color;
    }
    return;
  }
  lastUpdate = now;
  
  // Scale controls fire intensity
  uint8_t cooling = map(scale, 0, 255, 20, 100);   // Lower scale = calmer fire
  uint8_t sparking = map(scale, 0, 255, 50, 200);  // Lower scale = fewer sparks
  
  // Охлаждение
  for (int i = 0; i < ledState.numLeds; i++) {
    heat[i] = qsub8(heat[i], random8(0, ((cooling * 10) / ledState.numLeds) + 2));
  }
  
  // Распространение
  for (int k = ledState.numLeds - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }
  
  // Искры
  if (random8() < sparking) {
    int y = random8(7);
    heat[y] = qadd8(heat[y], random8(160, 255));
  }
  
  // Отображение
  for (int j = 0; j < ledState.numLeds; j++) {
    CRGB color = HeatColor(heat[j]);
    leds[j] = color;
  }
}

// Rainbow March - радужный марш
void mode_rainbow_march() {
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  static uint8_t hue = 0;
  hue += ledState.modeSettings[ledState.currentMode].speed / 50;
  
  // Scale controls color spacing (1-20)
  uint8_t colorSpacing = map(scale, 0, 255, 1, 20);
  
  fill_rainbow(leds, ledState.numLeds, hue, colorSpacing);
}

// Plasma - плазма
void mode_plasma() {
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  static uint8_t offset = 0;
  offset++;
  
  // Scale controls noise scale (10-100)
  uint8_t noiseScale = map(scale, 0, 255, 10, 100);
  
  for (int i = 0; i < ledState.numLeds; i++) {
    uint8_t bright = inoise8(i * noiseScale, offset * 3);
    leds[i] = CHSV((i * 7) + offset, 255, bright);
  }
}

// Noise - шум
void mode_noise() {
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  static uint16_t x = 0;
  x += ledState.modeSettings[ledState.currentMode].speed;
  
  // Scale controls noise density (50-300)
  uint16_t noiseDensity = map(scale, 0, 255, 50, 300);
  
  for (int i = 0; i < ledState.numLeds; i++) {
    uint8_t bright = inoise8(x + i * noiseDensity);
    leds[i] = CHSV((i * 8) + (x / 100), 255, bright);
  }
}

// Juggle - жонглирование
void mode_juggle() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  
  fadeToBlackBy(leds, ledState.numLeds, 20);
  
  // Scale controls number of juggling dots (1-16)
  uint8_t numDots = map(scale, 0, 255, 1, 16);
  
  byte dothue = 0;
  for (int i = 0; i < numDots; i++) {
    // Speed controls BPM (beats per minute: 10-60)
    uint8_t bpm = map(speed, 0, 255, 10, 60);
    leds[beatsin16(bpm + i * 2, 0, ledState.numLeds - 1)] |= CHSV(dothue, 200, 255);
    dothue += (256 / numDots);  // Distribute colors evenly
  }
}

// Solid Color - один цвет
void mode_solid_color() {
  CRGB color = CRGB::White;
  fill_solid(leds, ledState.numLeds, color);
}
