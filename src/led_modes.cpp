#include "led_modes.h"
#include "led_state.h"
#include <math.h>

CRGB leds[MAX_LEDS];   // выходной буфер: именно он уходит в ленту (FastLED.show)
static CRGB frame[MAX_LEDS];  // буфер, в который рисуют эффекты (цель для сглаживания)

// Аккумуляторы сглаживания в формате 8.8 (по каналу на светодиод): дробная часть
// нужна, чтобы плавное приближение к цели не «залипало» на единицу от неё
static uint16_t smoothAcc[MAX_LEDS * 3];

// Сколько светодиодов сейчас реально отправляется в ленту при FastLED.show()
static uint16_t appliedLedCount = MAX_LEDS;

/**
 * Регистрирует ленту в FastLED и гасит её.
 * Контроллер создаётся на MAX_LEDS, затем длина вывода сужается до ledState.numLeds.
 */
void initLEDs() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, MAX_LEDS);
  FastLED.setBrightness(ledState.brightness);
  // Временной дизеринг FastLED при яркости < 255 даёт заметное мерцание
  // на тусклых участках (особенно у медленных эффектов) — отключаем
  FastLED.setDither(DISABLE_DITHER);
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
    fill_solid(frame + target, appliedLedCount - target, CRGB::Black);
    memset(smoothAcc + target * 3, 0, (appliedLedCount - target) * 3 * sizeof(uint16_t));
    FastLED.show();
  }
  FastLED[0].setLeds(leds, target);
  appliedLedCount = target;
}

// =====================================================
// ===== ВИРТУАЛЬНЫЕ ЧАСЫ АНИМАЦИИ (общая скорость) =====
// =====================================================
// Все эффекты берут время не из millis(), а из animMillis(): эти часы идут
// быстрее или медленнее реальных в зависимости от общей скорости режима работы.
// beatsin8/beatsin16 из FastLED читают fl::millis() напрямую, поэтому им
// передаётся timebase = millis() - animMillis(): тогда (millis - timebase) = animMillis.

static uint32_t animTimeMs = 0;      // текущее виртуальное время, мс
static float animFraction = 0.0f;    // дробный остаток виртуальных мс
static uint32_t animLastRealMs = 0;  // реальное время прошлого шага
static uint32_t animDeltaMs = 0;     // сколько виртуальных мс прошло за этот кадр
static uint32_t animFrameRealMs = 0; // millis() на начало кадра (для стабильного timebase)

// Эталонный кадр, под который исторически подбирались «шаги на кадр» в эффектах
static const float ANIM_REFERENCE_FRAME_MS = 20.0f;

/**
 * Продвигает виртуальные часы анимации. Вызывается один раз за кадр до эффекта.
 * @param speedPercent общая скорость в процентах (100 = родная скорость эффектов)
 */
void advanceAnimationClock(uint8_t speedPercent) {
  uint32_t now = millis();
  if (animLastRealMs == 0) animLastRealMs = now;
  uint32_t realDt = now - animLastRealMs;
  animLastRealMs = now;
  // После долгой паузы (OTA, запись EEPROM) не прыгаем вперёд рывком
  if (realDt > 100) realDt = 100;

  animFraction += realDt * (speedPercent / 100.0f);
  animDeltaMs = (uint32_t)animFraction;
  animFraction -= animDeltaMs;
  animTimeMs += animDeltaMs;
  animFrameRealMs = now;
}

/**
 * Виртуальное время анимации, мс.
 * @return время с учётом общей скорости
 */
static inline uint32_t animMillis() {
  return animTimeMs;
}

/**
 * timebase для beatsin8/beatsin16, чтобы они шли по виртуальным часам.
 * @return значение timebase
 */
static inline uint32_t animTimebase() {
  return animFrameRealMs - animTimeMs;
}

/**
 * Накопитель «шагов на кадр»: переводит величину, рассчитанную на эталонный
 * кадр 20 мс, в целое число шагов за реально прошедшее виртуальное время.
 * @param acc       дробный накопитель (static в эффекте)
 * @param perFrame  сколько единиц добавлялось за эталонный кадр
 * @return целое число единиц для этого кадра
 */
static uint32_t animSteps(float& acc, float perFrame) {
  acc += perFrame * (animDeltaMs / ANIM_REFERENCE_FRAME_MS);
  uint32_t whole = (uint32_t)acc;
  acc -= whole;
  return whole;
}

// =====================================================
// ===== СГЛАЖИВАНИЕ ВЫВОДА (плавное зажигание/угасание) =====
// =====================================================
// Каждый светодиод не прыгает к новому цвету, а экспоненциально к нему
// приближается. Это убирает мерцание от резких вспышек/провалов в эффектах
// и даёт мягкие переходы при смене режима и вкл/выкл питания.

// Постоянная времени при смене режима и вкл/выкл (мс) и сколько длится кроссфейд
static const float CROSSFADE_TAU_MS = 280.0f;
static const uint32_t CROSSFADE_DURATION_MS = 900;
// Минимальная постоянная времени для подсветки (у неё свои плавные переходы цвета)
static const float BACKLIGHT_TAU_MS = 40.0f;

/**
 * Переводит постоянную времени в коэффициент приближения за кадр (0..65535).
 * @param tauMs   постоянная времени, мс (0 = без сглаживания)
 * @param frameMs длительность кадра, мс
 * @return коэффициент в формате 0.16
 */
static uint32_t smoothingCoef(float tauMs, float frameMs) {
  if (tauMs <= 1.0f) return 65535;
  float k = 1.0f - expf(-frameMs / tauMs);
  if (k >= 1.0f) return 65535;
  return (uint32_t)(k * 65535.0f) + 1;
}

/**
 * Переносит кадр эффекта (frame) в выходной буфер (leds) со сглаживанием.
 * @return true, если выходной буфер изменился (кадр стоит показать)
 */
bool presentFrame() {
  static uint32_t lastPresentMs = 0;
  static uint32_t crossfadeUntil = 0;
  static int16_t lastSignature = -1;

  uint32_t now = millis();
  float frameMs = lastPresentMs == 0 ? ANIM_REFERENCE_FRAME_MS : (float)(now - lastPresentMs);
  if (frameMs > 100.0f) frameMs = 100.0f;
  lastPresentMs = now;

  // Смена режима / режима работы / питания — включаем мягкий кроссфейд
  int16_t signature = (ledState.power ? 0x4000 : 0) |
                      ((ledState.operationMode == MODE_GARLAND) ? 0x2000 : 0) |
                      (ledState.operationMode == MODE_GARLAND ? ledState.currentMode : ledState.backlight.currentMode);
  if (lastSignature >= 0 && signature != lastSignature) {
    crossfadeUntil = now + CROSSFADE_DURATION_MS;
  }
  lastSignature = signature;

  float tau;
  if (ledState.operationMode == MODE_GARLAND) {
    // 0..255 -> 0..~300 мс: чем больше, тем мягче загораются и гаснут пиксели
    tau = ledState.garlandSmoothing * 1.2f;
  } else {
    tau = BACKLIGHT_TAU_MS;
  }
  if ((int32_t)(crossfadeUntil - now) > 0 && tau < CROSSFADE_TAU_MS) {
    tau = CROSSFADE_TAU_MS;
  }
  uint32_t k = smoothingCoef(tau, frameMs);

  bool changed = false;
  uint16_t count = ledState.numLeds;
  uint8_t* target = (uint8_t*)frame;
  uint8_t* out = (uint8_t*)leds;
  for (uint16_t i = 0; i < count * 3; i++) {
    int32_t goal = (int32_t)target[i] << 8;
    int32_t acc = smoothAcc[i];
    if (k >= 65535) {
      acc = goal;
    } else {
      acc += (int32_t)(((int64_t)(goal - acc) * (int64_t)k) >> 16);
      // Добиваем остаток, чтобы не зависать в дробной окрестности цели
      if (acc != goal && abs(goal - acc) < 64) acc = goal;
    }
    smoothAcc[i] = (uint16_t)acc;
    uint8_t value = (uint8_t)((acc + 128) >> 8);
    if (out[i] != value) {
      out[i] = value;
      changed = true;
    }
  }
  return changed;
}

/**
 * Заливает буфер кадра одним цветом (служебные индикаторы в main.cpp).
 * @param color цвет
 */
void fillFrameSolid(const CRGB& color) {
  fill_solid(frame, ledState.numLeds, color);
}

void runMode(uint8_t mode) {
  if (!ledState.power) {
    // Только гасим буфер; вывод кадра централизован в main.cpp
    fill_solid(frame, ledState.numLeds, CRGB::Black);
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
  uint8_t beat = beatsin8(speed / 10, 64, 255, animTimebase());
  
  // Scale controls color spacing (1-10)
  uint8_t colorSpacing = map(scale, 0, 255, 1, 10);
  
  for (int i = 0; i < ledState.numLeds; i++) {
    frame[i] = ColorFromPalette(RainbowColors_p, (i * colorSpacing) + beat, beat);
  }
}

// Blendwave - смешанные волны
void mode_blendwave() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  
  // Scale controls wave density (5-50)
  uint8_t waveDensity = map(scale, 0, 255, 5, 50);
  
  for (int i = 0; i < ledState.numLeds; i++) {
    uint8_t bright = beatsin8(speed / 10, 0, 255, animTimebase(), i * waveDensity);
    frame[i] = CHSV((animMillis() / 20) + i * 5, 255, bright);
  }
}

// Two Sin - две синусоиды
void mode_two_sin() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  // Оттенок сдвигается на 1 за эталонный кадр 20 мс (по виртуальным часам)
  uint8_t hue = animMillis() / 20;
  
  // Scale controls wave frequency (2-20)
  uint8_t waveFreq = map(scale, 0, 255, 2, 20);
  
  for (int i = 0; i < ledState.numLeds; i++) {
    uint8_t bright1 = beatsin8(speed / 15, 0, 255, animTimebase(), i * waveFreq);
    uint8_t bright2 = beatsin8(speed / 20, 0, 255, animTimebase(), i * (waveFreq + 2) + 128);
    uint8_t bright = (bright1 + bright2) / 2;
    
    frame[i] = CHSV(hue + i * 3, 255, bright);
  }
}

// Confetti - конфетти
void mode_confetti() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  
  // Speed controls fade rate (1-30)
  uint8_t fadeAmount = map(speed, 0, 255, 1, 30);
  // Scale controls number of confetti particles (1-8)
  uint8_t numConfetti = map(scale, 0, 255, 1, 8);
  // Speed also affects spawn probability
  uint8_t spawnChance = map(speed, 0, 255, 50, 255);

  // Один «тик» симуляции = эталонный кадр; число тиков зависит от общей скорости
  static float tickAcc = 0;
  uint32_t ticks = animSteps(tickAcc, 1.0f);
  if (ticks > 5) ticks = 5;
  while (ticks--) {
    fadeToBlackBy(frame, ledState.numLeds, fadeAmount);
    for (uint8_t i = 0; i < numConfetti; i++) {
      if (random8() < spawnChance) {
        int pos = random16(ledState.numLeds);
        // Use direct assignment to prevent brightness overflow flashes
        frame[pos] = CHSV(random8(), 200, 255);
      }
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
  
  unsigned long now = animMillis();  // виртуальные часы (общая скорость)
  if (now - lastUpdate < updateDelay) {
    // Just redraw without updating heat
    for (int j = 0; j < ledState.numLeds; j++) {
      CRGB color = HeatColor(heat[j]);
      frame[j] = color;
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
    frame[j] = color;
  }
}

// Rainbow March - радужный марш
void mode_rainbow_march() {
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  static uint8_t hue = 0;
  static float hueAcc = 0;
  hue += animSteps(hueAcc, ledState.modeSettings[ledState.currentMode].speed / 50);
  
  // Scale controls color spacing (1-20)
  uint8_t colorSpacing = map(scale, 0, 255, 1, 20);
  
  fill_rainbow(frame, ledState.numLeds, hue, colorSpacing);
}

// Plasma - плазма
void mode_plasma() {
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  // 16-бит: у 8-битного счётчика шум «прыгал» при переполнении каждые ~5 с
  uint16_t offset = animMillis() / 20;
  
  // Scale controls noise scale (10-100)
  uint8_t noiseScale = map(scale, 0, 255, 10, 100);
  
  for (int i = 0; i < ledState.numLeds; i++) {
    uint8_t bright = inoise8(i * noiseScale, (uint16_t)(offset * 3));
    frame[i] = CHSV((i * 7) + (uint8_t)offset, 255, bright);
  }
}

// Noise - шум
void mode_noise() {
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  static uint16_t x = 0;
  static float xAcc = 0;
  x += animSteps(xAcc, ledState.modeSettings[ledState.currentMode].speed);
  
  // Scale controls noise density (50-300)
  uint16_t noiseDensity = map(scale, 0, 255, 50, 300);
  
  for (int i = 0; i < ledState.numLeds; i++) {
    uint8_t bright = inoise8(x + i * noiseDensity);
    frame[i] = CHSV((i * 8) + (x / 100), 255, bright);
  }
}

// Juggle - жонглирование
void mode_juggle() {
  uint8_t speed = ledState.modeSettings[ledState.currentMode].speed;
  uint8_t scale = ledState.modeSettings[ledState.currentMode].scale;
  
  // Затухание «хвостов» — 20 за эталонный кадр, масштабируется общей скоростью
  static float fadeAcc = 0;
  uint32_t fade = animSteps(fadeAcc, 20.0f);
  if (fade > 0) fadeToBlackBy(frame, ledState.numLeds, fade > 255 ? 255 : fade);
  
  // Scale controls number of juggling dots (1-16)
  uint8_t numDots = map(scale, 0, 255, 1, 16);
  
  byte dothue = 0;
  for (int i = 0; i < numDots; i++) {
    // Speed controls BPM (beats per minute: 10-60)
    uint8_t bpm = map(speed, 0, 255, 10, 60);
    frame[beatsin16(bpm + i * 2, 0, ledState.numLeds - 1, animTimebase())] |= CHSV(dothue, 200, 255);
    dothue += (256 / numDots);  // Distribute colors evenly
  }
}

// Solid Color - один цвет
void mode_solid_color() {
  CRGB color = CRGB::White;
  fill_solid(frame, ledState.numLeds, color);
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
  
  unsigned long now = animMillis();  // виртуальные часы (общая скорость)
  
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
      // Мерцание — плавная синусоида вместо случайных скачков каждый кадр
      // (раньше random8() на каждом кадре давал «дребезг» яркости).
      // Значение snow[i] у снежинки постоянно, поэтому оно же служит её «семенем»:
      // своя фаза мерцания и свой оттенок, которые не меняются при падении.
      uint8_t seed = snow[i];
      uint8_t wave = sin8((uint8_t)(animMillis() / 6) + seed * 11);
      uint8_t twinkle = scale8(seed, 190 + scale8(wave, 65));  // 75%..100% яркости

      // Цвет снежинки: белый с лёгким голубым оттенком
      uint8_t hue = 160 + (seed % 20);          // Голубоватый оттенок
      uint8_t sat = (uint8_t)(seed * 7) % 80;   // Низкая насыщенность (ближе к белому)
      frame[i] = CHSV(hue, sat, twinkle);
    } else {
      frame[i] = CRGB::Black;
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
  static float auroraAcc = 0;
  auroraTime += animSteps(auroraAcc, waveSpeed);
  
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
    
    // Случайные покадровые «вспышки» пикселей убраны: они выглядели как мерцание.
    // Живость сиянию дают волны, шум и плавные «занавески» ниже.
    
    frame[i] = CHSV(hue, saturation, brightness);
  }
  
  // Добавляем редкие "занавески" - вертикальные полосы повышенной яркости
  static uint8_t curtainPos = 0;
  static uint8_t curtainWidth = 5;
  static unsigned long lastCurtain = 0;
  
  uint32_t nowAnim = animMillis();
  if (nowAnim - lastCurtain > 2000) {  // Новая занавеска каждые 2 секунды
    if (random8() < 30) {  // 12% шанс появления
      curtainPos = random8(ledState.numLeds);
      curtainWidth = random8(3, 10);
      lastCurtain = nowAnim;
    }
  }
  
  // Рисуем "занавеску" с затуханием
  uint32_t curtainAgeMs = nowAnim - lastCurtain;
  uint8_t curtainAge = curtainAgeMs > 2550 ? 255 : curtainAgeMs / 10;
  if (curtainAge < 100) {
    uint8_t curtainBright = 255 - curtainAge * 2;
    for (int j = 0; j < curtainWidth && (curtainPos + j) < ledState.numLeds; j++) {
      uint8_t fade = sin8(j * 128 / curtainWidth);  // Плавное затухание к краям
      uint8_t addBright = (curtainBright * fade) / 256;
      frame[curtainPos + j] = frame[curtainPos + j].lerp8(CHSV(baseHue + 32, 180, 255), addBright);
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
  
  unsigned long now = animMillis();  // виртуальные часы (общая скорость)
  if (now - lastUpdate < updateInterval) {
    // Просто перерисовываем без обновления состояния
    fill_solid(frame, ledState.numLeds, CRGB::Black);
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
        frame[fireflies[i].pos] = CHSV(fireflies[i].hue, fireflies[i].sat, brightness);
        
        // Добавляем легкое свечение на соседние пиксели (ореол)
        if (brightness > 50) {
          uint8_t glowBright = brightness / 3;  // Усилили ореол
          uint8_t glowSat = fireflies[i].sat > 50 ? fireflies[i].sat - 30 : 0;
          if (fireflies[i].pos > 0) {
            frame[fireflies[i].pos - 1] += CHSV(fireflies[i].hue, glowSat, glowBright);
          }
          if (fireflies[i].pos < ledState.numLeds - 1) {
            frame[fireflies[i].pos + 1] += CHSV(fireflies[i].hue, glowSat, glowBright);
          }
        }
      }
    }
    return;
  }
  lastUpdate = now;
  
  // Очищаем ленту
  fill_solid(frame, ledState.numLeds, CRGB::Black);
  
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
        frame[fireflies[i].pos] = CHSV(fireflies[i].hue, fireflies[i].sat, brightness);
        
        // Ореол свечения на соседних пикселях для магического эффекта
        if (brightness > 50) {
          uint8_t glowBright = brightness / 3;  // Усилили ореол
          uint8_t glowSat = fireflies[i].sat > 50 ? fireflies[i].sat - 30 : 0;
          if (fireflies[i].pos > 0) {
            frame[fireflies[i].pos - 1] += CHSV(fireflies[i].hue, glowSat, glowBright);
          }
          if (fireflies[i].pos < ledState.numLeds - 1) {
            frame[fireflies[i].pos + 1] += CHSV(fireflies[i].hue, glowSat, glowBright);
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
            frame[pos] = CHSV(fireflies[i].hue, fireflies[i].sat, 255);
            // Расширенный белый ореол при вспышке
            if (pos > 0) frame[pos - 1] = CHSV(fireflies[i].hue, fireflies[i].sat / 2, 180);
            if (pos > 1) frame[pos - 2] += CHSV(fireflies[i].hue, fireflies[i].sat / 3, 90);
            if (pos < ledState.numLeds - 1) frame[pos + 1] = CHSV(fireflies[i].hue, fireflies[i].sat / 2, 180);
            if (pos < ledState.numLeds - 2) frame[pos + 2] += CHSV(fireflies[i].hue, fireflies[i].sat / 3, 90);
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
    fill_solid(frame, ledState.numLeds, CRGB::Black);
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
    uint8_t wave1 = beatsin8(waveBPM, minBright, 255, animTimebase(), i * waveCount * 3);
    uint8_t wave2 = beatsin8(waveBPM * 0.7, 0, 40, animTimebase(), i * waveCount * 5 + 64);
    
    // Комбинируем - wave1 основная, wave2 добавляет вариацию
    uint8_t brightness = qadd8(wave1, wave2 / 4);
    if (brightness > 255) brightness = 255;
    
    // Применяем яркость к оригинальному цвету через масштабирование RGB
    // Это сохраняет насыщенность цвета!
    frame[i].r = scale8(baseColor.r, brightness);
    frame[i].g = scale8(baseColor.g, brightness);
    frame[i].b = scale8(baseColor.b, brightness);
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
  uint8_t mainPulse = beatsin8(pulseBPM, minBright, 255, animTimebase());
  
  // Добавляем вторичную пульсацию для более органичного эффекта
  uint8_t secondPulse = beatsin8(pulseBPM * 1.3, 0, 20, animTimebase());
  
  uint8_t brightness = qadd8(mainPulse, secondPulse / 4);
  if (brightness > 255) brightness = 255;
  
  for (int i = 0; i < ledState.numLeds; i++) {
    // Небольшое смещение яркости по позиции для "живости"
    uint8_t posOffset = sin8(i * 3 + animMillis() / 100) / 64;  // 0-4
    uint8_t pixelBright = qadd8(brightness, posOffset);
    if (pixelBright > 255) pixelBright = 255;
    
    // Применяем яркость к оригинальному RGB цвету - сохраняет насыщенность!
    frame[i].r = scale8(baseColor.r, pixelBright);
    frame[i].g = scale8(baseColor.g, pixelBright);
    frame[i].b = scale8(baseColor.b, pixelBright);
  }
}
