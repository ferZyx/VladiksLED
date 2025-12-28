#include "led_state.h"
#include "config.h"
#include <EEPROM.h>

LEDState ledState;
volatile bool settingsChanged = false;

void initLEDState() {
  ledState.power = true;
  ledState.brightness = DEFAULT_BRIGHTNESS;
  ledState.numLeds = DEFAULT_LEDS;
  ledState.currentMode = 0;
  ledState.autoSwitchDelay = 0;  // Авто-переключение выключено
  ledState.randomOrder = false;
  
  // Инициализация настроек режимов по умолчанию
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
}

void saveLEDState() {
  // Сохранение в EEPROM
  EEPROM.begin(1024);  // Увеличили размер для расписаний
  
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
  EEPROM.begin(1024);  // Увеличили размер для расписаний
  
  // Читаем заголовок
  EEPROMHeader header;
  EEPROM.get(0, header);
  
  // Проверяем магическое число
  if (header.magic == EEPROM_MAGIC) {
    if (header.version == EEPROM_VERSION) {
      // Данные валидны - загружаем состояние
      EEPROM.get(sizeof(EEPROMHeader), ledState);
      Serial.println("✅ LED state loaded from EEPROM");
    } else if (header.version == 1) {
      // Миграция с версии 1 на версию 2
      Serial.println("🔄 Migrating EEPROM from v1 to v2...");
      
      // Загружаем старые данные (без расписаний)
      EEPROM.get(sizeof(EEPROMHeader), ledState);
      
      // Инициализируем новые поля (расписания)
      for (int i = 0; i < MAX_SCHEDULES; i++) {
        ledState.schedules[i].enabled = false;
        ledState.schedules[i].hour = 0;
        ledState.schedules[i].minute = 0;
        ledState.schedules[i].action = true;
        ledState.schedules[i].daysOfWeek = 0x7F;
      }
      
      EEPROM.end();
      saveLEDState();  // Сохраняем с новой версией
      Serial.println("✅ Migration complete");
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
