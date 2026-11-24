#include "led_modes.h"
#include "led_state.h"
#include <math.h>

CRGB leds[MAX_LEDS];

// Сколько светодиодов сейчас реально отправляется в ленту при FastLED.show()
static uint16_t appliedLedCount = MAX_LEDS;

/**
 * Регистрирует ленту в FastLED и гасит её.
 * Контроллер создаётся на MAX_LEDS, затем длина вывода сужается до ledState.numLeds.
 */
void initLEDs() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, MAX_LEDS);
  FastLED.setBrightness(ledState.brightness);
  FastLED.clear();
  FastLED.show();
  appliedLedCount = MAX_LEDS;
  syncLedCount();
}

/**
 * Синхронизирует длину вывода FastLED с ledState.numLeds.
 *
 * Зачем: вывод WS2812 занимает ~30 мкс на светодиод и блокирует CPU.
 * Отправка всех MAX_LEDS (300 = ~9 мс) каждые 20 мс отнимала почти половину
 * процессорного времени у WiFi/TCP-стека, отсюда лаги веба.
 */
void syncLedCount() {
  uint16_t target = constrain(ledState.numLeds, (uint16_t)1, (uint16_t)MAX_LEDS);
  if (target == appliedLedCount) return;

  // При уменьшении один раз гасим «хвост» со старой длиной, иначе он
  // так и останется гореть последним показанным цветом
  if (target < appliedLedCount) {
    fill_solid(leds + target, appliedLedCount - target, CRGB::Black);
    FastLED.show();
  }
  FastLED[0].setLeds(leds, target);
  appliedLedCount = target;
}

void runMode(uint8_t mode) {
  if (!ledState.power) {
    // Только гасим буфер; вывод кадра централизован в main.cpp
    fill_solid(leds, ledState.numLeds, CRGB::Black);
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
    case 10: mode_snowfall(); break;
    case 11: mode_aurora(); break;
    case 12: mode_fireflies(); break;
    
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

// Snowfall - падающий снег с мерцанием
// Имитация снегопада: белые/голубые снежинки падают вниз и мерцают
void mode_snowfall() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  
  // Плотность снега: сколько снежинок генерируется (1-15)
  uint8_t density = map(scale, 0, 255, 1, 15);
  // Скорость падения (интервал в мс между шагами)
  uint8_t fallSpeed = map(speed, 0, 255, 80, 8);
  
  // Статический массив для хранения позиций снежинок (яркость каждого LED)
  static uint8_t snow[MAX_LEDS];
  static unsigned long lastUpdate = 0;
  
  unsigned long now = millis();
  
  // Обновление позиций снежинок
  if (now - lastUpdate >= fallSpeed) {
    lastUpdate = now;
    
    // Сдвигаем все снежинки вниз (к большему индексу)
    for (int i = ledState.numLeds - 1; i > 0; i--) {
      snow[i] = snow[i - 1];
    }
    
    // Генерируем новые снежинки в начале ленты
    // Случайная генерация с учётом плотности
    if (random8() < density * 12) {
      snow[0] = random8(180, 255);  // Яркость новой снежинки
    } else {
      snow[0] = 0;
    }
  }
  
  // Отрисовка снежинок с мерцанием
  for (int i = 0; i < ledState.numLeds; i++) {
    if (snow[i] > 0) {
      // Мерцание: добавляем случайное изменение яркости
      uint8_t twinkle = snow[i];
      if (random8() < 60) {
        twinkle = qadd8(twinkle, random8(20, 50));  // Вспышка
      }
      if (random8() < 40) {
        twinkle = qsub8(twinkle, random8(10, 30));  // Приглушение
      }
      
      // Цвет снежинки: белый с лёгким голубым оттенком
      // Hue 160-180 = голубой, низкая насыщенность = близко к белому
      uint8_t hue = 160 + random8(20);      // Голубоватый оттенок
      uint8_t sat = random8(0, 80);         // Низкая насыщенность (ближе к белому)
      leds[i] = CHSV(hue, sat, twinkle);
    } else {
      leds[i] = CRGB::Black;
    }
  }
}

// Aurora Borealis - Северное сияние
// Имитация полярного сияния с плавными переливами зелёного, голубого и фиолетового
void mode_aurora() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  
  // Статические переменные для плавной анимации
  static uint16_t auroraTime = 0;
  static uint8_t baseHue = 96;  // Начинаем с зелёного (характерный цвет сияния)
  
  // Speed контролирует скорость движения волн (1-10)
  uint8_t waveSpeed = map(speed, 0, 255, 1, 10);
  auroraTime += waveSpeed;
  
  // Медленное изменение базового оттенка для разнообразия
  if (random8() < 3) {
    baseHue = baseHue + random8(3) - 1;  // Случайный дрейф ±1
    // Ограничиваем палитру сияния: зелёный (96) - голубой (128) - фиолетовый (192)
    if (baseHue < 80) baseHue = 80;
    if (baseHue > 200) baseHue = 200;
  }
  
  // Scale контролирует "ширину" волн сияния (10-50)
  uint8_t waveWidth = map(scale, 0, 255, 10, 50);
  
  for (int i = 0; i < ledState.numLeds; i++) {
    // Создаём несколько накладывающихся волн с разными частотами
    // Это имитирует слоистую структуру полярного сияния
    
    // Основная волна - медленная и широкая
    uint8_t wave1 = sin8((i * waveWidth / 3) + auroraTime);
    
    // Вторая волна - быстрее и уже
    uint8_t wave2 = sin8((i * waveWidth / 2) - (auroraTime * 2) + 64);
    
    // Третья волна - ещё быстрее, для мерцания
    uint8_t wave3 = sin8((i * waveWidth) + (auroraTime * 3) + 128);
    
    // Комбинируем волны с разными весами
    uint16_t combinedWave = ((uint16_t)wave1 * 3 + (uint16_t)wave2 * 2 + (uint16_t)wave3) / 6;
    
    // Добавляем Perlin noise для органичного мерцания
    uint8_t noise = inoise8(i * 30, auroraTime * 2);
    
    // Яркость зависит от комбинированной волны и шума
    uint8_t brightness = (combinedWave * noise) / 256;
    
    // Применяем нелинейное преобразование для более драматичного эффекта
    // Это создаёт "пики" яркости как в настоящем сиянии
    if (brightness > 128) {
      brightness = 128 + (brightness - 128) * 2;  // Усиливаем яркие области
    } else {
      brightness = brightness / 2;  // Приглушаем тёмные области
    }
    brightness = constrain(brightness, 0, 255);
    
    // Цвет варьируется вдоль ленты с добавлением шума
    // Создаём характерные цвета сияния: зелёный -> голубой -> фиолетовый
    uint8_t hueNoise = inoise8(i * 20, auroraTime / 2);
    uint8_t hue = baseHue + (hueNoise / 4) - 32;  // Вариация ±32 от базового
    
    // Насыщенность высокая, но с небольшой вариацией
    uint8_t saturation = 200 + (noise / 8);  // 200-231
    
    // Добавляем случайные "вспышки" - характерная черта сияния
    if (random8() < 5 && brightness > 100) {
      brightness = qadd8(brightness, 50);  // Случайная вспышка
      saturation = qsub8(saturation, 40);  // Вспышки чуть белее
    }
    
    leds[i] = CHSV(hue, saturation, brightness);
  }
  
  // Добавляем редкие "занавески" - вертикальные полосы повышенной яркости
  static uint8_t curtainPos = 0;
  static uint8_t curtainWidth = 5;
  static unsigned long lastCurtain = 0;
  
  if (millis() - lastCurtain > 2000) {  // Новая занавеска каждые 2 секунды
    if (random8() < 30) {  // 12% шанс появления
      curtainPos = random8(ledState.numLeds);
      curtainWidth = random8(3, 10);
      lastCurtain = millis();
    }
  }
  
  // Рисуем "занавеску" с затуханием
  uint8_t curtainAge = (millis() - lastCurtain) / 10;
  if (curtainAge < 100) {
    uint8_t curtainBright = 255 - curtainAge * 2;
    for (int j = 0; j < curtainWidth && (curtainPos + j) < ledState.numLeds; j++) {
      uint8_t fade = sin8(j * 128 / curtainWidth);  // Плавное затухание к краям
      uint8_t addBright = (curtainBright * fade) / 256;
      leds[curtainPos + j] = leds[curtainPos + j].lerp8(CHSV(baseHue + 32, 180, 255), addBright);
    }
  }
}

// Fireflies - Светлячки
// Имитация волшебных светлячков: точки плавно загораются и угасают в случайных местах
// Новогодняя палитра: красные, зелёные, золотые и белые искорки
void mode_fireflies() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  
  // Максимальное количество одновременных светлячков (5-30)
  uint8_t maxFireflies = map(scale, 0, 255, 5, 30);
  
  // Структура для хранения состояния каждого светлячка
  // phase: 0=неактивен, 1-127=разгорается, 128-255=угасает
  static struct {
    uint16_t pos;      // Позиция на ленте
    uint8_t phase;     // Фаза жизненного цикла (0=мёртв)
    uint8_t hue;       // Индивидуальный оттенок
    uint8_t sat;       // Насыщенность (для белых искорок)
    uint8_t maxBright; // Максимальная яркость этого светлячка
    uint8_t speed;     // Индивидуальная скорость (разные светлячки мигают с разной скоростью)
  } fireflies[30];
  
  static unsigned long lastUpdate = 0;
  static bool initialized = false;
  
  // Новогодние цвета (hue): красный=0, зелёный=96, золотой=32
  // Массив новогодних оттенков
  static const uint8_t xmasHues[] = {0, 0, 96, 96, 32, 32, 160};  // красный, красный, зелёный, зелёный, золотой, золотой, голубой
  static const uint8_t numXmasHues = 7;
  
  // Инициализация при первом запуске
  if (!initialized) {
    for (int i = 0; i < 30; i++) {
      fireflies[i].phase = 0;
      fireflies[i].pos = 0;
    }
    initialized = true;
  }
  
  // Скорость обновления анимации (5-30ms)
  uint8_t updateInterval = map(speed, 0, 255, 30, 5);
  
  unsigned long now = millis();
  if (now - lastUpdate < updateInterval) {
    // Просто перерисовываем без обновления состояния
    fill_solid(leds, ledState.numLeds, CRGB::Black);
    for (int i = 0; i < maxFireflies; i++) {
      if (fireflies[i].phase > 0 && fireflies[i].pos < ledState.numLeds) {
        uint8_t brightness;
        if (fireflies[i].phase <= 127) {
          // Разгорается: 0->127 соответствует 0->255 яркости (но макс = maxBright)
          brightness = ease8InOutCubic(fireflies[i].phase * 2);
          brightness = (brightness * fireflies[i].maxBright) / 255;
        } else {
          // Угасает: 128->255 соответствует 255->0 яркости
          brightness = ease8InOutCubic((255 - fireflies[i].phase) * 2);
          brightness = (brightness * fireflies[i].maxBright) / 255;
        }
        leds[fireflies[i].pos] = CHSV(fireflies[i].hue, fireflies[i].sat, brightness);
        
        // Добавляем легкое свечение на соседние пиксели (ореол)
        if (brightness > 50) {
          uint8_t glowBright = brightness / 3;  // Усилили ореол
          uint8_t glowSat = fireflies[i].sat > 50 ? fireflies[i].sat - 30 : 0;
          if (fireflies[i].pos > 0) {
            leds[fireflies[i].pos - 1] += CHSV(fireflies[i].hue, glowSat, glowBright);
          }
          if (fireflies[i].pos < ledState.numLeds - 1) {
            leds[fireflies[i].pos + 1] += CHSV(fireflies[i].hue, glowSat, glowBright);
          }
        }
      }
    }
    return;
  }
  lastUpdate = now;
  
  // Очищаем ленту
  fill_solid(leds, ledState.numLeds, CRGB::Black);
  
  // Обновляем состояние каждого светлячка
  for (int i = 0; i < maxFireflies; i++) {
    if (fireflies[i].phase > 0) {
      // Светлячок активен - продвигаем фазу
      uint8_t phaseStep = 1 + fireflies[i].speed / 64;  // Индивидуальная скорость
      
      if (fireflies[i].phase <= 127) {
        // Фаза разгорания
        fireflies[i].phase += phaseStep;
        if (fireflies[i].phase > 127) fireflies[i].phase = 128;  // Переход к угасанию
      } else if (fireflies[i].phase < 255) {
        // Фаза угасания
        fireflies[i].phase += phaseStep;
        if (fireflies[i].phase < 128) fireflies[i].phase = 255;  // Overflow protection
      }
      
      // Светлячок полностью угас
      if (fireflies[i].phase >= 254) {
        fireflies[i].phase = 0;
      }
      
      // Рисуем светлячка
      if (fireflies[i].phase > 0 && fireflies[i].pos < ledState.numLeds) {
        uint8_t brightness;
        if (fireflies[i].phase <= 127) {
          // Разгорается с кубической интерполяцией для плавности
          brightness = ease8InOutCubic(fireflies[i].phase * 2);
          brightness = (brightness * fireflies[i].maxBright) / 255;
        } else {
          // Угасает
          brightness = ease8InOutCubic((255 - fireflies[i].phase) * 2);
          brightness = (brightness * fireflies[i].maxBright) / 255;
        }
        
        // Основная точка светлячка
        leds[fireflies[i].pos] = CHSV(fireflies[i].hue, fireflies[i].sat, brightness);
        
        // Ореол свечения на соседних пикселях для магического эффекта
        if (brightness > 50) {
          uint8_t glowBright = brightness / 3;  // Усилили ореол
          uint8_t glowSat = fireflies[i].sat > 50 ? fireflies[i].sat - 30 : 0;
          if (fireflies[i].pos > 0) {
            leds[fireflies[i].pos - 1] += CHSV(fireflies[i].hue, glowSat, glowBright);
          }
          if (fireflies[i].pos < ledState.numLeds - 1) {
            leds[fireflies[i].pos + 1] += CHSV(fireflies[i].hue, glowSat, glowBright);
          }
        }
      }
    } else {
      // Светлячок неактивен - может появиться новый
      // Вероятность появления зависит от speed (чаще при высокой скорости)
      uint8_t spawnChance = map(speed, 0, 255, 5, 40);
      
      if (random8() < spawnChance) {
        // Создаём нового светлячка
        fireflies[i].pos = random16(ledState.numLeds);
        fireflies[i].phase = 1;  // Начинаем разгораться
        
        // Новогодняя палитра!
        uint8_t colorChoice = random8(100);
        if (colorChoice < 15) {
          // 15% - белые/серебристые искорки (очень красиво!)
          fireflies[i].hue = random8();  // Любой оттенок, но...
          fireflies[i].sat = random8(0, 30);  // Почти белый!
        } else {
          // 85% - новогодние цвета
          uint8_t hueIdx = random8(numXmasHues);
          fireflies[i].hue = xmasHues[hueIdx] + random8(16) - 8;  // Небольшая вариация ±8
          fireflies[i].sat = 255;  // Максимальная насыщенность!
        }
        
        // Случайная максимальная яркость (разные светлячки - разная интенсивность)
        fireflies[i].maxBright = random8(180, 255);  // Повысили минимум
        
        // Случайная скорость мигания
        fireflies[i].speed = random8(64, 255);
      }
    }
  }
  
  // Добавляем редкие "вспышки" - когда светлячок особенно ярко мигает
  // Это создаёт эффект "общения" между светлячками
  static unsigned long lastFlash = 0;
  if (now - lastFlash > 400) {  // Чуть чаще вспышки
    if (random8() < 20) {  // ~8% шанс
      // Находим активного светлячка и делаем его ярче
      for (int i = 0; i < maxFireflies; i++) {
        if (fireflies[i].phase > 50 && fireflies[i].phase < 200) {
          // Вспышка! Добавляем яркость
          uint16_t pos = fireflies[i].pos;
          if (pos < ledState.numLeds) {
            // Яркая белая вспышка с цветным ядром
            leds[pos] = CHSV(fireflies[i].hue, fireflies[i].sat, 255);
            // Расширенный белый ореол при вспышке
            if (pos > 0) leds[pos - 1] = CHSV(fireflies[i].hue, fireflies[i].sat / 2, 180);
            if (pos > 1) leds[pos - 2] += CHSV(fireflies[i].hue, fireflies[i].sat / 3, 90);
            if (pos < ledState.numLeds - 1) leds[pos + 1] = CHSV(fireflies[i].hue, fireflies[i].sat / 2, 180);
            if (pos < ledState.numLeds - 2) leds[pos + 2] += CHSV(fireflies[i].hue, fireflies[i].sat / 3, 90);
          }
          lastFlash = now;
          break;
        }
      }
    }
  }
}

// =====================================================
// ===== РЕЖИМЫ ПОДСВЕТКИ (BACKLIGHT MODES) =====
// =====================================================

namespace {
  const uint32_t BACKLIGHT_TRANSITION_MIN_MS = 1200;
  const uint32_t BACKLIGHT_TRANSITION_MAX_MS = 11000;
  const uint16_t BACKLIGHT_MAX_RGB_DISTANCE = 442;

  bool backlightTransitionInitialized = false;
  bool backlightTransitionActive = false;
  uint8_t trackedBacklightColorIndex = 255;

  CRGB backlightCurrentColor = CRGB::Black;
  CRGB backlightTransitionStartColor = CRGB::Black;
  CRGB backlightTransitionTargetColor = CRGB::Black;
  CRGB trackedBacklightSlotColor = CRGB::Black;

  uint32_t backlightTransitionStartMs = 0;
  uint32_t backlightTransitionDurationMs = 0;
}

static bool areColorsEqual(const CRGB& a, const CRGB& b) {
  return a.r == b.r && a.g == b.g && a.b == b.b;
}

static CRGB resolveBacklightTargetColor() {
  uint8_t idx = ledState.backlight.activeColorIndex;
  if (idx < MAX_BACKLIGHT_COLORS && ledState.backlight.colors[idx].enabled) {
    return ledState.backlight.colors[idx].color;
  }

  for (int i = 0; i < MAX_BACKLIGHT_COLORS; i++) {
    if (ledState.backlight.colors[i].enabled) {
      return ledState.backlight.colors[i].color;
    }
  }

  return CRGB(255, 180, 107);
}

static uint16_t rgbDistance(const CRGB& a, const CRGB& b) {
  int16_t dr = static_cast<int16_t>(b.r) - static_cast<int16_t>(a.r);
  int16_t dg = static_cast<int16_t>(b.g) - static_cast<int16_t>(a.g);
  int16_t db = static_cast<int16_t>(b.b) - static_cast<int16_t>(a.b);

  uint32_t sum = static_cast<uint32_t>(dr * dr) + static_cast<uint32_t>(dg * dg) + static_cast<uint32_t>(db * db);
  return static_cast<uint16_t>(sqrtf(static_cast<float>(sum)));
}

static uint32_t calculateBacklightTransitionDurationMs(const CRGB& from, const CRGB& to) {
  uint16_t distance = rgbDistance(from, to);
  if (distance <= 1) {
    return 0;
  }

  uint32_t transitionRange = BACKLIGHT_TRANSITION_MAX_MS - BACKLIGHT_TRANSITION_MIN_MS;
  return BACKLIGHT_TRANSITION_MIN_MS + (static_cast<uint32_t>(distance) * transitionRange) / BACKLIGHT_MAX_RGB_DISTANCE;
}

static CRGB interpolateColor(const CRGB& from, const CRGB& to, float t) {
  float eased = t * t * (3.0f - 2.0f * t);  // smoothstep
  if (eased < 0.0f) eased = 0.0f;
  if (eased > 1.0f) eased = 1.0f;

  CRGB result;
  result.r = static_cast<uint8_t>(from.r + (static_cast<float>(to.r) - from.r) * eased + 0.5f);
  result.g = static_cast<uint8_t>(from.g + (static_cast<float>(to.g) - from.g) * eased + 0.5f);
  result.b = static_cast<uint8_t>(from.b + (static_cast<float>(to.b) - from.b) * eased + 0.5f);
  return result;
}

static void updateBacklightTransitionProgress(uint32_t nowMs) {
  if (!backlightTransitionActive) {
    return;
  }

  if (backlightTransitionDurationMs == 0) {
    backlightCurrentColor = backlightTransitionTargetColor;
    backlightTransitionActive = false;
    return;
  }

  uint32_t elapsed = nowMs - backlightTransitionStartMs;
  if (elapsed >= backlightTransitionDurationMs) {
    backlightCurrentColor = backlightTransitionTargetColor;
    backlightTransitionActive = false;
    return;
  }

  float t = static_cast<float>(elapsed) / static_cast<float>(backlightTransitionDurationMs);
  backlightCurrentColor = interpolateColor(backlightTransitionStartColor, backlightTransitionTargetColor, t);
}

// Получить активный цвет подсветки (с плавным переходом по RGB)
CRGB getBacklightColor() {
  uint32_t nowMs = millis();
  CRGB desiredColor = resolveBacklightTargetColor();
  uint8_t currentIndex = ledState.backlight.activeColorIndex;

  bool activeSlotValid = currentIndex < MAX_BACKLIGHT_COLORS && ledState.backlight.colors[currentIndex].enabled;
  CRGB activeSlotColor = activeSlotValid ? ledState.backlight.colors[currentIndex].color : desiredColor;

  if (!backlightTransitionInitialized) {
    backlightTransitionInitialized = true;
    trackedBacklightColorIndex = currentIndex;
    trackedBacklightSlotColor = activeSlotColor;

    backlightCurrentColor = desiredColor;
    backlightTransitionStartColor = desiredColor;
    backlightTransitionTargetColor = desiredColor;
    backlightTransitionActive = false;
    backlightTransitionDurationMs = 0;
    return backlightCurrentColor;
  }

  updateBacklightTransitionProgress(nowMs);

  bool colorIndexChanged = (currentIndex != trackedBacklightColorIndex);
  if (colorIndexChanged) {
    trackedBacklightColorIndex = currentIndex;
    trackedBacklightSlotColor = activeSlotColor;

    backlightTransitionStartColor = backlightCurrentColor;
    backlightTransitionTargetColor = desiredColor;
    backlightTransitionStartMs = nowMs;
    backlightTransitionDurationMs = calculateBacklightTransitionDurationMs(backlightTransitionStartColor, backlightTransitionTargetColor);

    if (backlightTransitionDurationMs == 0) {
      backlightCurrentColor = backlightTransitionTargetColor;
      backlightTransitionActive = false;
    } else {
      backlightTransitionActive = true;
    }

    return backlightCurrentColor;
  }

  if (activeSlotValid && !areColorsEqual(activeSlotColor, trackedBacklightSlotColor)) {
    trackedBacklightSlotColor = activeSlotColor;
    backlightCurrentColor = activeSlotColor;
    backlightTransitionStartColor = activeSlotColor;
    backlightTransitionTargetColor = activeSlotColor;
    backlightTransitionActive = false;
    backlightTransitionDurationMs = 0;
  }

  return backlightCurrentColor;
}

// Запуск режима подсветки
void runBacklightMode(uint8_t mode) {
  if (!ledState.power) {
    // Только гасим буфер; вывод кадра централизован в main.cpp
    fill_solid(leds, ledState.numLeds, CRGB::Black);
    return;
  }
  
  switch (mode) {
    case 0: backlight_waves(); break;
    case 1: backlight_pulse(); break;
    default: backlight_waves(); break;
  }
}

// Волны - плавное перемещение яркости по ленте
// Создает эффект "бегущих волн" одного цвета с вариацией яркости
void backlight_waves() {
  uint8_t speed = ledState.backlight.modeSettings[ledState.backlight.currentMode].speed;
  uint8_t intensity = ledState.backlight.modeSettings[ledState.backlight.currentMode].intensity;
  
  CRGB baseColor = getBacklightColor();
  
  // Скорость волны: от 5 до 60 BPM (очень медленно до умеренно)
  uint8_t waveBPM = map(speed, 0, 255, 5, 60);
  
  // Интенсивность влияет на глубину волны (насколько темнеет)
  // При низкой интенсивности - яркость 180-255, при высокой - 50-255
  uint8_t minBright = map(intensity, 0, 255, 180, 50);
  uint8_t waveCount = map(intensity, 0, 255, 1, 4);
  
  for (int i = 0; i < ledState.numLeds; i++) {
    // Генерируем волну яркости
    uint8_t wave1 = beatsin8(waveBPM, minBright, 255, 0, i * waveCount * 3);
    uint8_t wave2 = beatsin8(waveBPM * 0.7, 0, 40, 0, i * waveCount * 5 + 64);
    
    // Комбинируем - wave1 основная, wave2 добавляет вариацию
    uint8_t brightness = qadd8(wave1, wave2 / 4);
    if (brightness > 255) brightness = 255;
    
    // Применяем яркость к оригинальному цвету через масштабирование RGB
    // Это сохраняет насыщенность цвета!
    leds[i].r = scale8(baseColor.r, brightness);
    leds[i].g = scale8(baseColor.g, brightness);
    leds[i].b = scale8(baseColor.b, brightness);
  }
}

// Пульсация - эффект "дыхания" всей ленты одним цветом
// Вся лента плавно становится ярче и тусклее
void backlight_pulse() {
  uint8_t speed = ledState.backlight.modeSettings[ledState.backlight.currentMode].speed;
  uint8_t intensity = ledState.backlight.modeSettings[ledState.backlight.currentMode].intensity;
  
  CRGB baseColor = getBacklightColor();
  
  // Скорость пульсации: от 3 до 30 BPM (очень медленное "дыхание")
  uint8_t pulseBPM = map(speed, 0, 255, 3, 30);
  
  // Интенсивность определяет глубину пульсации
  // При низкой интенсивности - мягкое дыхание (яркость 180-255)
  // При высокой - глубокое (яркость 30-255)
  uint8_t minBright = map(intensity, 0, 255, 180, 30);
  
  // Главная пульсация
  uint8_t mainPulse = beatsin8(pulseBPM, minBright, 255);
  
  // Добавляем вторичную пульсацию для более органичного эффекта
  uint8_t secondPulse = beatsin8(pulseBPM * 1.3, 0, 20);
  
  uint8_t brightness = qadd8(mainPulse, secondPulse / 4);
  if (brightness > 255) brightness = 255;
  
  for (int i = 0; i < ledState.numLeds; i++) {
    // Небольшое смещение яркости по позиции для "живости"
    uint8_t posOffset = sin8(i * 3 + millis() / 100) / 64;  // 0-4
    uint8_t pixelBright = qadd8(brightness, posOffset);
    if (pixelBright > 255) pixelBright = 255;
    
    // Применяем яркость к оригинальному RGB цвету - сохраняет насыщенность!
    leds[i].r = scale8(baseColor.r, pixelBright);
    leds[i].g = scale8(baseColor.g, pixelBright);
    leds[i].b = scale8(baseColor.b, pixelBright);
  }
}
