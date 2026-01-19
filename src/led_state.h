#ifndef LED_STATE_H
#define LED_STATE_H

#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

// EEPROM validation structure
struct EEPROMHeader {
  uint32_t magic;    // Magic number: 0x4C454456 ("LEDV")
  uint8_t version;   // Data structure version
};

#define EEPROM_MAGIC 0x4C454456  // "LEDV" in hex
#define EEPROM_VERSION 4

// Режим работы устройства
enum OperationMode : uint8_t {
  MODE_GARLAND = 0,    // Гирлянда (существующие режимы)
  MODE_BACKLIGHT = 1   // Подсветка (новые режимы)
};

// Структура расписания
struct Schedule {
  bool enabled;        // Активно ли расписание
  uint8_t hour;        // Час (0-23)
  uint8_t minute;      // Минута (0-59)
  bool action;         // true = включить, false = выключить
  uint8_t daysOfWeek;  // Битовая маска дней недели (bit 0 = Пн, bit 6 = Вс, 0x7F = все дни)
};

// Настройки режима
struct ModeSettings {
  uint8_t speed;          // Скорость (0-255)
  uint8_t scale;          // Масштаб (0-255)
  CRGB color1;            // Основной цвет
  CRGB color2;            // Дополнительный цвет
  uint8_t brightness;     // Яркость режима (0-255)
  bool archived;          // Архивный режим (не показывать и не переключать)
};

// Настройки режима подсветки (упрощенные)
struct BacklightModeSettings {
  uint8_t speed;          // Скорость анимации (0-255)
  uint8_t intensity;      // Интенсивность эффекта (0-255)
  uint8_t brightness;     // Яркость режима (0-255)
  bool archived;          // Архивный режим
};

// Цвет для подсветки
struct BacklightColor {
  CRGB color;             // Цвет
  bool enabled;           // Активен ли цвет
};

// Настройки подсветки
struct BacklightSettings {
  uint8_t currentMode;              // Текущий режим подсветки (0 - TOTAL_BACKLIGHT_MODES-1)
  uint16_t modeAutoSwitchDelay;     // Задержка авто-переключения режимов (0 = выкл)
  bool modeRandomOrder;             // Случайный порядок режимов
  
  uint8_t activeColorIndex;         // Индекс активного цвета
  uint16_t colorAutoSwitchDelay;    // Задержка авто-переключения цветов (0 = выкл)
  bool colorRandomOrder;            // Случайный порядок цветов
  
  BacklightColor colors[MAX_BACKLIGHT_COLORS];    // Доступные цвета
  BacklightModeSettings modeSettings[TOTAL_BACKLIGHT_MODES];  // Настройки режимов подсветки
};

// Глобальное состояние гирлянды
struct LEDState {
  bool power;                     // Вкл/выкл
  uint8_t brightness;             // Общая яркость 0-255
  uint16_t numLeds;               // Количество диодов
  uint8_t currentMode;            // Текущий режим гирлянды (0-TOTAL_MODES-1)
  uint16_t autoSwitchDelay;       // Delay переключения в секундах (0 = выкл)
  bool randomOrder;               // Случайный порядок режимов
  ModeSettings modeSettings[TOTAL_MODES];  // Настройки каждого из режимов гирлянды
  Schedule schedules[10];         // Расписания включения/выключения
  
  // Новые поля для подсветки
  OperationMode operationMode;    // Режим работы: гирлянда или подсветка
  BacklightSettings backlight;    // Настройки подсветки
};

// Глобальная переменная состояния
extern LEDState ledState;
extern volatile bool settingsChanged;

// Функции для работы с состоянием
void initLEDState();
void saveLEDState();
void loadLEDState();

#endif
