#ifndef LED_MODES_H
#define LED_MODES_H

#include <FastLED.h>
#include "config.h"

extern CRGB leds[MAX_LEDS];

// Инициализация LED
void initLEDs();

// Запуск режима гирлянды
void runMode(uint8_t mode);

// Запуск режима подсветки
void runBacklightMode(uint8_t mode);

// ===== Режимы гирлянды =====
void mode_blendwave();
void mode_rainbow_beat();
void mode_two_sin();
void mode_confetti();
void mode_fire();
void mode_rainbow_march();
void mode_plasma();
void mode_noise();
void mode_juggle();
void mode_solid_color();
void mode_snowfall();
void mode_aurora();
void mode_fireflies();

// ===== Режимы подсветки =====
void backlight_waves();     // Волны - плавное перемещение света
void backlight_pulse();     // Пульсация - дыхание цвета

#endif
