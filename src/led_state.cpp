#include "led_state.h"
#include "config.h"
#include <EEPROM.h>

LEDState ledState;

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
}

void saveLEDState() {
  // Сохранение в EEPROM
  EEPROM.begin(512);
  
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
  EEPROM.begin(512);
  
  // Читаем заголовок
  EEPROMHeader header;
  EEPROM.get(0, header);
  
  // Проверяем магическое число
  if (header.magic == EEPROM_MAGIC && header.version == EEPROM_VERSION) {
    // Данные валидны - загружаем состояние
    EEPROM.get(sizeof(EEPROMHeader), ledState);
    Serial.println("✅ LED state loaded from EEPROM");
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
