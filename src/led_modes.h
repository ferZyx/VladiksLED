#ifndef LED_MODES_H
#define LED_MODES_H

#include <FastLED.h>
#include "config.h"

extern CRGB leds[MAX_LEDS];

// Инициализация LED
void initLEDs();

// Запуск режима
void runMode(uint8_t mode);

// Базовые режимы (упрощенные версии из референса)
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

#endif
