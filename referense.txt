#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include "webpage_content.h"
#include "digits.h"

#define LED_PIN D1
#define NUM_LEDS_X 11
#define NUM_LEDS_Y 9
#define NUM_LEDS (NUM_LEDS_X * NUM_LEDS_Y)

CRGB leds[NUM_LEDS];

const char* ssid = "Beeline 33";
const char* password = "87774604633";

int player_x = 5;
int player_y = 4;
int score = 0;
int apple_x = -1;  // Координаты яблока
int apple_y = -1;
unsigned long lastAppleSpawnTime = 0;
bool isGameStarted = false;
int satietyTimeout = 5000;

ESP8266WebServer server(80);

void setup() {
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(10000);
    ESP.restart();
  }
  ArduinoOTA.begin();

  FastLED.addLeds<WS2812, LED_PIN, BGR>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/moveUp", HTTP_GET, moveUp);
  server.on("/moveDown", HTTP_GET, moveDown);
  server.on("/moveLeft", HTTP_GET, moveLeft);
  server.on("/moveRight", HTTP_GET, moveRight);
  server.on("/start", HTTP_GET, startGame);

  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  if (isGameStarted) {
    draw();
    handleSatiety();
  }

  FastLED.show();
}

void displayNumber(int number, int x, int y) {
  FastLED.clear();
  // Преобразуем число в строку
  String numberString = String(number);

  // Выводим каждую цифру на матрицу с отступом
  for (int i = 0; i < numberString.length(); i++) {
    int digit = numberString.charAt(i) - '0';
    for (int row = 0; row < 5; row++) {
      for (int col = 0; col < 3; col++) {
        if (digitPatterns[digit][row] & (1 << col)) {
          leds[calculateIndex(x + col + i * 3, y + row)] = CRGB::White;
        }
      }
    }
  }
}



void spawnApple() {
  apple_x = random(NUM_LEDS_X);
  apple_y = random(NUM_LEDS_Y);

  unsigned long currentTime = millis();
  lastAppleSpawnTime = currentTime;
}

void handleSatiety() {
  unsigned long currentTime = millis();
  if (currentTime - lastAppleSpawnTime >= satietyTimeout) {
    // game lose
    apple_x = -1;
    apple_y = -1;
    player_x = 5;
    player_y = 4;
    isGameStarted = false;
    displayNumber(score, 0, 2);
    score = 0;
    satietyTimeout = 5000;
  }
}


int calculateIndex(int x, int y) {
  if (x % 2 == 0) {
    return NUM_LEDS - NUM_LEDS_Y * (x + 1) + y;
  } else {
    return NUM_LEDS - 1 - NUM_LEDS_Y * x - y;
  }
}


void hasAteCheck() {
  if (player_x == apple_x && player_y == apple_y) {
    score++;
    satietyTimeout = satietyTimeout - 100;
    spawnApple();
  }
}

void draw() {
  FastLED.clear();
  leds[calculateIndex(player_x, player_y)] = CRGB::White;
  if (apple_x != -1 && apple_y != -1) {
    if (millis() - lastAppleSpawnTime < satietyTimeout - 3000) {
      leds[calculateIndex(apple_x, apple_y)] = CRGB::Red;
    } else {
      if ((millis() / 500) % 2 == 0) {
        leds[calculateIndex(apple_x, apple_y)] = CRGB::Black;
      } else {
        leds[calculateIndex(apple_x, apple_y)] = CRGB::Red;
      }
    }
  }
}




void handleRoot() {
  server.send(200, "text/html", getIndexHtml());
}

void moveUp() {
  if (isGameStarted) {
    player_y++;
    if (player_y >= NUM_LEDS_Y) player_y = 0;
    hasAteCheck();
  }

  server.send(200, "text/plain", "OK");
}

void moveDown() {
  if (isGameStarted) {
    player_y--;
    if (player_y < 0) player_y = NUM_LEDS_Y - 1;
    hasAteCheck();
  }

  server.send(200, "text/plain", "OK");
}

void moveLeft() {
  if (isGameStarted) {
    player_x--;
    if (player_x < 0) player_x = NUM_LEDS_X - 1;
    hasAteCheck();
  }

  server.send(200, "text/plain", "OK");
}

void moveRight() {
  if (isGameStarted) {
    player_x++;
    if (player_x >= NUM_LEDS_X) player_x = 0;
    hasAteCheck();
  }

  server.send(200, "text/plain", "OK");
}

void startGame() {
  if (!isGameStarted) {
    isGameStarted = true;
    spawnApple();
  }


  server.send(200, "text/plain", "OK");
}
