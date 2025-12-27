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
  }
}

void saveLEDState() {
  // Сохранение в EEPROM
  EEPROM.begin(512);
  EEPROM.put(0, ledState);
  EEPROM.commit();
  EEPROM.end();
}

void loadLEDState() {
  // Загрузка из EEPROM
  EEPROM.begin(512);
  
  // Проверяем, есть ли сохраненные данные
  uint8_t magic;
  EEPROM.get(0, magic);
  
  if (magic == 0x55) { // Магическое число для проверки инициализации
    EEPROM.get(0, ledState);
  } else {
    // Первый запуск - инициализируем и сохраняем
    initLEDState();
    ledState.brightness = 0x55; // Временно устанавливаем magic byte
    EEPROM.put(0, ledState);
    EEPROM.commit();
    ledState.brightness = DEFAULT_BRIGHTNESS; // Возвращаем нормальную яркость
  }
  
  EEPROM.end();
}
