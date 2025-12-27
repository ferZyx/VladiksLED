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
  
  FastLED.setBrightness(ledState.brightness);
  
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
    
    // Остальные режимы - дубликаты с вариациями
    case 10: case 11: case 12: case 13: case 14:
    case 15: case 16: case 17: case 18: case 19:
      mode_rainbow_beat(); break;
    
    case 20: case 21: case 22: case 23: case 24:
    case 25: case 26: case 27: case 28: case 29:
      mode_confetti(); break;
    
    case 30: case 31: case 32: case 33: case 34:
    case 35: case 36: case 37: case 38:
      mode_fire(); break;
    
    case 39: mode_fire(); break;
    case 40: mode_solid_color(); break;
    
    default: mode_rainbow_beat(); break;
  }
  
  FastLED.show();
}

// Rainbow Beat - радужная волна
void mode_rainbow_beat() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  uint8_t beat = beatsin8(speed / 10, 64, 255);
  
  for (int i = 0; i < ledState.numLeds; i++) {
    leds[i] = ColorFromPalette(RainbowColors_p, (i * 2) + beat, beat);
  }
}

// Blendwave - смешанные волны
void mode_blendwave() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  
  for (int i = 0; i < ledState.numLeds; i++) {
    uint8_t bright = beatsin8(speed / 10, 0, 255, 0, i * 10);
    leds[i] = CHSV((millis() / 20) + i * 5, 255, bright);
  }
}

// Two Sin - две синусоиды
void mode_two_sin() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  static uint8_t hue = 0;
  hue++;
  
  for (int i = 0; i < ledState.numLeds; i++) {
    uint8_t bright1 = beatsin8(speed / 15, 0, 255, 0, i * 5);
    uint8_t bright2 = beatsin8(speed / 20, 0, 255, 0, i * 7 + 128);
    uint8_t bright = (bright1 + bright2) / 2;
    
    leds[i] = CHSV(hue + i * 3, 255, bright);
  }
}

// Confetti - конфетти
void mode_confetti() {
  fadeToBlackBy(leds, ledState.numLeds, 10);
  
  int pos = random16(ledState.numLeds);
  leds[pos] += CHSV(random8(), 200, 255);
}

// Fire - огонь
void mode_fire() {
  static byte heat[MAX_LEDS];
  uint8_t cooling = 55;
  uint8_t sparking = 120;
  
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
  static uint8_t hue = 0;
  hue += ledState.modeSettings[ledState.currentMode].speed / 50;
  
  fill_rainbow(leds, ledState.numLeds, hue, 7);
}

// Plasma - плазма
void mode_plasma() {
  static uint8_t offset = 0;
  offset++;
  
  for (int i = 0; i < ledState.numLeds; i++) {
    uint8_t bright = inoise8(i * 30, offset * 3);
    leds[i] = CHSV((i * 7) + offset, 255, bright);
  }
}

// Noise - шум
void mode_noise() {
  static uint16_t x = 0;
  x += ledState.modeSettings[ledState.currentMode].speed;
  
  for (int i = 0; i < ledState.numLeds; i++) {
    uint8_t bright = inoise8(x + i * 100);
    leds[i] = CHSV((i * 8) + (x / 100), 255, bright);
  }
}

// Juggle - жонглирование
void mode_juggle() {
  fadeToBlackBy(leds, ledState.numLeds, 20);
  
  byte dothue = 0;
  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, ledState.numLeds - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

// Solid Color - один цвет
void mode_solid_color() {
  CRGB color = ledState.modeSettings[ledState.currentMode].color1;
  fill_solid(leds, ledState.numLeds, color);
}
