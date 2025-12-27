#ifndef LED_STATE_H
#define LED_STATE_H

#include <Arduino.h>
#include <FastLED.h>

// Настройки режима
struct ModeSettings {
  uint8_t speed;          // Скорость (0-255)
  uint8_t scale;          // Масштаб (0-255)
  CRGB color1;            // Основной цвет
  CRGB color2;            // Дополнительный цвет
  uint8_t brightness;     // Яркость режима (0-255)
  bool archived;          // Архивный режим (не показывать и не переключать)
};

// Глобальное состояние гирлянды
struct LEDState {
  bool power;                     // Вкл/выкл
  uint8_t brightness;             // Общая яркость 0-255
  uint16_t numLeds;               // Количество диодов
  uint8_t currentMode;            // Текущий режим (0-40)
  uint16_t autoSwitchDelay;       // Delay переключения в секундах (0 = выкл)
  bool randomOrder;               // Случайный порядок режимов
  ModeSettings modeSettings[41];  // Настройки каждого из 41 режимов
};

// Глобальная переменная состояния
extern LEDState ledState;

// Функции для работы с состоянием
void initLEDState();
void saveLEDState();
void loadLEDState();

#endif
