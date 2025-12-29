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
