#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include "config.h"
#include "led_state.h"
#include "led_modes.h"
#include "webserver.h"

// Авто-переключение режимов
unsigned long lastModeSwitch = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n🎄 WiFi LED Garland Starting...");
  
  // Инициализация LED state
  initLEDState();
  loadLEDState();
  
  // Инициализация LED ленты
  initLEDs();
  
  // Подключение к WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  
#ifdef USE_STATIC_IP
  // Настройка статического IP
  IPAddress local_IP(STATIC_IP);
  IPAddress gateway(GATEWAY_IP);
  IPAddress subnet(SUBNET_MASK);
  
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Failed to configure static IP");
  }
#endif
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Анимация подключения на LED
  int dotCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
    // Бегущий огонёк во время подключения
    FastLED.clear();
    leds[dotCount % ledState.numLeds] = CRGB::Blue;
    FastLED.show();
    dotCount++;
    
    // Таймаут 30 секунд
    if (dotCount > 60) {
      Serial.println("\n❌ WiFi connection failed!");
      Serial.println("Please check WIFI_SSID and WIFI_PASSWORD in config.h");
      
      // Красная вспышка - ошибка
      fill_solid(leds, ledState.numLeds, CRGB::Red);
      FastLED.show();
      delay(2000);
      
      ESP.restart();
    }
  }
  
  Serial.println("\n✅ WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Зелёная вспышка - успех
  fill_solid(leds, ledState.numLeds, CRGB::Green);
  FastLED.show();
  delay(1000);
  FastLED.clear();
  FastLED.show();
  
  // Настройка OTA обновлений
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
    // Выключаем LED во время обновления
    FastLED.clear();
    FastLED.show();
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
  Serial.println("✅ OTA Ready");
  
  // Запуск веб-сервера
  setupWebServer();
  
  Serial.println("\n🌐 Web server started");
  Serial.print("Open http://");
  Serial.print(WiFi.localIP());
  Serial.println("/ in your browser\n");
  
  lastModeSwitch = millis();
}

void loop() {
  // Обработка OTA запросов
  ArduinoOTA.handle();
  
  // Обработка HTTP запросов
  handleWebServer();
  
  // Запуск текущего режима LED
  EVERY_N_MILLISECONDS(20) {
    // Set brightness once per frame to avoid flickering
    FastLED.setBrightness(ledState.brightness);
    
    runMode(ledState.currentMode);
    
    // Show the frame
    FastLED.show();
  }
  
  // Авто-переключение режимов
  if (ledState.autoSwitchDelay > 0) {
    unsigned long now = millis();
    if (now - lastModeSwitch >= (ledState.autoSwitchDelay * 1000UL)) {
      lastModeSwitch = now;
      
      // Собираем список активных (не архивных) режимов
      uint8_t activeModes[TOTAL_MODES];
      uint8_t activeCount = 0;
      
      for (uint8_t i = 0; i < TOTAL_MODES; i++) {
        if (!ledState.modeSettings[i].archived) {
          activeModes[activeCount++] = i;
        }
      }
      
      // Если есть активные режимы, переключаемся
      if (activeCount > 0) {
        if (ledState.randomOrder) {
          // Случайный режим из активных
          uint8_t randomIndex = random8(activeCount);
          ledState.currentMode = activeModes[randomIndex];
        } else {
          // Следующий по порядку среди активных
          bool found = false;
          for (uint8_t i = 0; i < activeCount; i++) {
            if (activeModes[i] > ledState.currentMode) {
              ledState.currentMode = activeModes[i];
              found = true;
              break;
            }
          }
          // Если не нашли следующий, берем первый активный
          if (!found) {
            ledState.currentMode = activeModes[0];
          }
        }
        
        Serial.print("Auto-switched to mode: ");
        Serial.println(ledState.currentMode);
        
        saveLEDState();
      }
    }
  }
}
