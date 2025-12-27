#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include <time.h>  // Встроенная библиотека для работы со временем
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "config.h"
#include "led_state.h"
#include "led_modes.h"
#include "webserver.h"

// Авто-переключение режимов
unsigned long lastModeSwitch = 0;

// Отслеживание последней проверки расписания
int lastCheckedMinute = -1;

// Функция синхронизации времени через HTTP API
bool syncTimeViaHTTP() {
  WiFiClient client;
  HTTPClient http;
  
  Serial.println("🌐 Fetching time via HTTP API...");
  
  // Используем worldtimeapi.org для получения времени
  // Timezone: Asia/Yekaterinburg (UTC+5)
  http.begin(client, "http://worldtimeapi.org/api/timezone/Asia/Yekaterinburg");
  http.setTimeout(5000);  // 5 секунд таймаут
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Ищем "unixtime": в JSON ответе
    int timePos = payload.indexOf("\"unixtime\":");
    if (timePos != -1) {
      int startPos = timePos + 11;  // После "unixtime":
      int endPos = payload.indexOf(",", startPos);
      String timeStr = payload.substring(startPos, endPos);
      
      time_t timestamp = timeStr.toInt();
      
      if (timestamp > 1000000000) {
        // Устанавливаем время
        timeval tv = { timestamp, 0 };
        settimeofday(&tv, nullptr);
        
        Serial.print("✅ Time synced via HTTP: ");
        Serial.println(timestamp);
        
        http.end();
        return true;
      }
    }
  } else {
    Serial.print("❌ HTTP request failed: ");
    Serial.println(httpCode);
  }
  
  http.end();
  return false;
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n🎄 WiFi LED Garland Starting...");
  
  // Инициализация LED state
  initLEDState();
  loadLEDState();
  
  // Инициализация LED ленты ПЕРЕД подключением к WiFi для анимации
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
  
  // Инициализация NTP через встроенные функции ESP8266
  Serial.println("🕐 Initializing NTP...");
  Serial.print("NTP Server: ");
  Serial.println(NTP_SERVER);
  Serial.print("Timezone: UTC+");
  Serial.println(NTP_OFFSET / 3600);
  
  // Настраиваем NTP (сервер, смещение в секундах, летнее время = 0)
  configTime(NTP_OFFSET, 0, NTP_SERVER);
  
  // Ждем синхронизации времени
  Serial.print("Waiting for NTP sync");
  int ntpRetries = 0;
  time_t now = time(nullptr);
  
  while (now < 1000000000 && ntpRetries < 10) {  // Уменьшили попытки NTP
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    ntpRetries++;
  }
  
  if (now >= 1000000000) {
    Serial.println("\n✅ NTP time synchronized");
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    Serial.print("Current time: ");
    Serial.println(asctime(&timeinfo));
  } else {
    Serial.println("\n⚠️ NTP sync failed, trying HTTP API...");
    
    // Пробуем синхронизацию через HTTP
    if (syncTimeViaHTTP()) {
      now = time(nullptr);
      struct tm timeinfo;
      localtime_r(&now, &timeinfo);
      Serial.print("Current time: ");
      Serial.println(asctime(&timeinfo));
    } else {
      Serial.println("⚠️ HTTP sync also failed, time will be set from browser");
    }
  }
  
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

// Проверка и выполнение расписаний
void checkSchedules() {
  // Получаем текущее время через встроенные функции
  time_t now = time(nullptr);
  
  // Проверяем что время синхронизировано
  if (now < 1000000000) {
    return;  // Время еще не синхронизировано
  }
  
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;
  int currentDayOfWeek = timeinfo.tm_wday;  // 0 = Воскресенье, 1 = Понедельник, ..., 6 = Суббота
  
  // Преобразуем день недели: tm_wday (0=Вс) -> наш формат (0=Пн, 6=Вс)
  int dayBit = (currentDayOfWeek == 0) ? 6 : (currentDayOfWeek - 1);
  
  // Проверяем, не проверяли ли мы уже эту минуту
  if (lastCheckedMinute == currentMinute) {
    return;  // Уже проверяли в эту минуту
  }
  
  lastCheckedMinute = currentMinute;
  
  // Проходим по всем расписаниям
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    Schedule &schedule = ledState.schedules[i];
    
    // Пропускаем неактивные расписания
    if (!schedule.enabled) {
      continue;
    }
    
    // Проверяем совпадение времени
    if (schedule.hour != currentHour || schedule.minute != currentMinute) {
      continue;
    }
    
    // Проверяем день недели (битовая маска)
    if (!(schedule.daysOfWeek & (1 << dayBit))) {
      continue;  // Этот день недели не активен
    }
    
    // Выполняем действие
    ledState.power = schedule.action;
    saveLEDState();
    
    Serial.print("⏰ Schedule triggered: ");
    Serial.print(schedule.action ? "ON" : "OFF");
    Serial.print(" at ");
    Serial.print(currentHour);
    Serial.print(":");
    Serial.println(currentMinute);
  }
}

void loop() {
  // Обработка OTA запросов
  ArduinoOTA.handle();
  
  // Обработка HTTP запросов
  handleWebServer();
  
  // Ресинхронизация времени каждый час через HTTP
  EVERY_N_SECONDS(3600) {
    time_t now = time(nullptr);
    if (now < 1000000000) {
      // Время не синхронизировано, пробуем снова
      Serial.println("⏰ Time not synced, attempting HTTP sync...");
      syncTimeViaHTTP();
    }
  }
  
  // Проверка расписаний каждую секунду
  EVERY_N_SECONDS(1) {
    checkSchedules();
  }
  
  // Set brightness once per frame to avoid flickering
  FastLED.setBrightness(ledState.brightness);
  
  // Запуск текущего режима LED
  EVERY_N_MILLISECONDS(20) {
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
