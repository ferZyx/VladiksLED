#include "led_state.h"
#include "config.h"
#include <EEPROM.h>

LEDState ledState;
volatile bool settingsChanged = false;

// Предустановленные красивые цвета для подсветки
const CRGB DEFAULT_BACKLIGHT_COLORS[] = {
  CRGB(255, 180, 107),  // Теплый белый / Warm White
  CRGB(144, 238, 144),  // Мягкий зеленый / Soft Green
  CRGB(135, 206, 235),  // Небесно-голубой / Sky Blue
  CRGB(255, 140, 0),    // Янтарный / Amber
  CRGB(230, 190, 255),  // Лавандовый / Lavender
  CRGB(255, 127, 80),   // Коралловый / Coral
  CRGB(64, 224, 208),   // Бирюзовый / Turquoise
  CRGB(255, 182, 193),  // Розовый / Pink
};
const uint8_t NUM_DEFAULT_COLORS = sizeof(DEFAULT_BACKLIGHT_COLORS) / sizeof(DEFAULT_BACKLIGHT_COLORS[0]);

void initBacklightDefaults() {
  // Настройки режима подсветки
  ledState.backlight.currentMode = 0;
  ledState.backlight.modeAutoSwitchDelay = 0;
  ledState.backlight.modeRandomOrder = false;
  
  // Настройки цветов
  ledState.backlight.activeColorIndex = 0;
  ledState.backlight.colorAutoSwitchDelay = 0;
  ledState.backlight.colorRandomOrder = false;
  
  // Инициализация предустановленных цветов
  for (int i = 0; i < MAX_BACKLIGHT_COLORS; i++) {
    if (i < NUM_DEFAULT_COLORS) {
      ledState.backlight.colors[i].color = DEFAULT_BACKLIGHT_COLORS[i];
      ledState.backlight.colors[i].enabled = true;
    } else {
      ledState.backlight.colors[i].color = CRGB::Black;
      ledState.backlight.colors[i].enabled = false;
    }
  }
  
  // Настройки режимов подсветки
  for (int i = 0; i < TOTAL_BACKLIGHT_MODES; i++) {
    ledState.backlight.modeSettings[i].speed = 128;
    ledState.backlight.modeSettings[i].intensity = 128;
    ledState.backlight.modeSettings[i].brightness = 255;
    ledState.backlight.modeSettings[i].archived = false;
  }
}

/**
 * Значения по умолчанию для общих настроек анимации (поля EEPROM v5).
 */
void initAnimationDefaults() {
  ledState.garlandSpeed = ANIM_SPEED_DEFAULT;
  ledState.backlightSpeed = ANIM_SPEED_DEFAULT;
  ledState.garlandSmoothing = GARLAND_SMOOTHING_DEFAULT;
}

/**
 * Приводит загруженные из EEPROM настройки анимации к допустимому диапазону
 * (защита от мусора во флеше).
 */
static void sanitizeAnimationSettings() {
  if (ledState.garlandSpeed < ANIM_SPEED_MIN || ledState.garlandSpeed > ANIM_SPEED_MAX) {
    ledState.garlandSpeed = ANIM_SPEED_DEFAULT;
  }
  if (ledState.backlightSpeed < ANIM_SPEED_MIN || ledState.backlightSpeed > ANIM_SPEED_MAX) {
    ledState.backlightSpeed = ANIM_SPEED_DEFAULT;
  }
}

void initLEDState() {
  ledState.power = true;
  ledState.brightness = DEFAULT_BRIGHTNESS;
  ledState.numLeds = DEFAULT_LEDS;
  ledState.currentMode = 0;
  ledState.autoSwitchDelay = 0;  // Авто-переключение выключено
  ledState.randomOrder = false;
  ledState.operationMode = MODE_GARLAND;  // По умолчанию - гирлянда
  
  // Инициализация настроек режимов гирлянды по умолчанию
  for (int i = 0; i < TOTAL_MODES; i++) {
    ledState.modeSettings[i].speed = 128;
    ledState.modeSettings[i].scale = 128;
    ledState.modeSettings[i].color1 = CRGB::Red;
    ledState.modeSettings[i].color2 = CRGB::Blue;
    ledState.modeSettings[i].brightness = 255;
    ledState.modeSettings[i].archived = false;
  }
  
  // Инициализация расписаний (все выключены)
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    ledState.schedules[i].enabled = false;
    ledState.schedules[i].hour = 0;
    ledState.schedules[i].minute = 0;
    ledState.schedules[i].action = true;
    ledState.schedules[i].daysOfWeek = 0x7F;  // Все дни недели
  }
  
  // Инициализация настроек подсветки
  initBacklightDefaults();
  initAnimationDefaults();
}

void saveLEDState() {
  // Сохранение в EEPROM
  EEPROM.begin(2048);  // Увеличили размер для подсветки
  
  // Сохраняем заголовок с магическим числом
  EEPROMHeader header;
  header.magic = EEPROM_MAGIC;
  header.version = EEPROM_VERSION;
  EEPROM.put(0, header);
  
  // Сохраняем состояние LED после заголовка
  EEPROM.put(sizeof(EEPROMHeader), ledState);
  
  EEPROM.commit();
  EEPROM.end();
}


void loadLEDState() {
  // Загрузка из EEPROM
  EEPROM.begin(2048);  // Увеличили размер для подсветки
  
  // Читаем заголовок
  EEPROMHeader header;
  EEPROM.get(0, header);
  
  // Проверяем магическое число
  if (header.magic == EEPROM_MAGIC) {
    if (header.version == EEPROM_VERSION) {
      // Данные валидны - загружаем состояние
      EEPROM.get(sizeof(EEPROMHeader), ledState);
      sanitizeAnimationSettings();
      Serial.println("✅ LED state loaded from EEPROM");
    } else if (header.version == 4) {
      // Миграция v4 -> v5: новые поля дописаны в конец LEDState, префикс совпадает.
      // Читаем всю структуру (хвост — мусор) и задаём новым полям значения по умолчанию.
      Serial.println("🔄 Migrating EEPROM from v4 to v5...");
      EEPROM.get(sizeof(EEPROMHeader), ledState);
      initAnimationDefaults();
      EEPROM.end();
      saveLEDState();
      Serial.println("✅ Migration v4->v5 complete");
      return;
    } else if (header.version == 3) {
      // Миграция с версии 3 на версию 4 (добавление подсветки)
      Serial.println("🔄 Migrating EEPROM from v3 to v4...");
      
      // Загружаем старые данные (без подсветки)
      EEPROM.get(sizeof(EEPROMHeader), ledState);
      
      // Инициализируем новые поля (подсветка)
      ledState.operationMode = MODE_GARLAND;
      initBacklightDefaults();
      initAnimationDefaults();
      
      EEPROM.end();
      saveLEDState();  // Сохраняем с новой версией
      Serial.println("✅ Migration v3->v5 complete");
      return;
    } else if (header.version == 1) {
      // Миграция с версии 1 на версию 4
      Serial.println("🔄 Migrating EEPROM from v1 to v4...");
      
      // Загружаем старые данные (без расписаний и подсветки)
      EEPROM.get(sizeof(EEPROMHeader), ledState);
      
      // Инициализируем расписания
      for (int i = 0; i < MAX_SCHEDULES; i++) {
        ledState.schedules[i].enabled = false;
        ledState.schedules[i].hour = 0;
        ledState.schedules[i].minute = 0;
        ledState.schedules[i].action = true;
        ledState.schedules[i].daysOfWeek = 0x7F;
      }
      
      // Инициализируем подсветку
      ledState.operationMode = MODE_GARLAND;
      initBacklightDefaults();
      initAnimationDefaults();
      
      EEPROM.end();
      saveLEDState();  // Сохраняем с новой версией
      Serial.println("✅ Migration v1->v5 complete");
      return;
    } else {
      // Неизвестная версия
      Serial.println("⚠️ Unknown EEPROM version, initializing defaults");
      initLEDState();
      EEPROM.end();
      saveLEDState();
      return;
    }
  } else {
    // Первый запуск или неверные данные - инициализируем и сохраняем
    Serial.println("⚠️ No valid EEPROM data found, initializing defaults");
    initLEDState();
    EEPROM.end();
    saveLEDState();  // Сохраняем с правильным заголовком
    return;
  }
  
  EEPROM.end();
}
