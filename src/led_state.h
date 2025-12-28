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
#define EEPROM_VERSION 3

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

// Глобальное состояние гирлянды
struct LEDState {
  bool power;                     // Вкл/выкл
  uint8_t brightness;             // Общая яркость 0-255
  uint16_t numLeds;               // Количество диодов
  uint8_t currentMode;            // Текущий режим (0-40)
  uint16_t autoSwitchDelay;       // Delay переключения в секундах (0 = выкл)
  bool randomOrder;               // Случайный порядок режимов
  ModeSettings modeSettings[TOTAL_MODES];  // Настройки каждого из режимов
  Schedule schedules[10];         // Расписания включения/выключения
};

// Глобальная переменная состояния
extern LEDState ledState;
extern volatile bool settingsChanged;

// Функции для работы с состоянием
void initLEDState();
void saveLEDState();
void loadLEDState();

#endif
