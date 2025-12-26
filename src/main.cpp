/*
   VladiksLED - Простой дебаг проект
   Белый огонек бегает от первого до последнего светодиода и обратно
*/

#include <FastLED.h>

// Настройки
#define LED_PIN       4           // Пин для подключения ленты (GPIO 4 = D2 на D1 Mini). GPIO 6 (SD_CLK) нельзя использовать!
#define NUM_LEDS      50          // Количество светодиодов
#define BRIGHTNESS    100         // Яркость (0-255)
#define SPEED         50          // Скорость движения (меньше = быстрее, мс)
const bool ENABLE_BLINK = true;   // Включить мерцание встроенным светодиодом

// Возможные пины встроенного светодиода для разных плат
int builtinLedPins[] = {LED_BUILTIN, 13, 2, 16, 1, 3, 5};
int numBuiltinPins = sizeof(builtinLedPins) / sizeof(builtinLedPins[0]);
int currentPinIndex = 0;
int blinkCounter = 0;

CRGB leds[NUM_LEDS];

int position = 0;           // Текущая позиция светодиода
int direction = 1;          // Направление движения (1 = вперед, -1 = назад)

void setup() {
  // Инициализация FastLED
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
  
  // Инициализируем все возможные пины встроенного светодиода
  if (ENABLE_BLINK) {
    for (int i = 0; i < numBuiltinPins; i++) {
      pinMode(builtinLedPins[i], OUTPUT);
      digitalWrite(builtinLedPins[i], LOW);
    }
  }
}

void loop() {
  // Очистка всех светодиодов
  FastLED.clear();
  
  // Включаем белый светодиод на текущей позиции
  leds[position] = CRGB::White;
  
  // Обновляем ленту
  FastLED.show();
  
  // Мигаем встроенным светодиодом (перебираем все пины)
  // Каждые 20 миганий переключаемся на следующий пин
  if (ENABLE_BLINK) {
    bool blinkState = (blinkCounter % 2 == 0);
    for (int i = 0; i < numBuiltinPins; i++) {
      if (i == currentPinIndex) {
        digitalWrite(builtinLedPins[i], blinkState ? HIGH : LOW);
      } else {
        digitalWrite(builtinLedPins[i], LOW);
      }
    }
    
    blinkCounter++;
    if (blinkCounter >= 40) {  // Через 40 миганий (20 полных циклов) переходим к следующему пину
      blinkCounter = 0;
      currentPinIndex = (currentPinIndex + 1) % numBuiltinPins;
    }
  }
  
  // Задержка для контроля скорости
  delay(SPEED);
  
  // Двигаем позицию
  position += direction;
  
  // Проверяем границы и меняем направление
  if (position >= NUM_LEDS - 1) {
    direction = -1;  // Разворот назад
  } else if (position <= 0) {
    direction = 1;   // Разворот вперед
  }
}
