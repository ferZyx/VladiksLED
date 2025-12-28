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
#include "logger.h"
#include "diagnostics.h"

// Названия режимов (должны совпадать с frontend)
const char* MODE_NAMES[] = {
  "Смешанные волны",    // Blendwave
  "Радужная пульсация", // Rainbow Beat
  "Две синусоиды",      // Two Sin
  "Конфетти",           // Confetti
  "Огонь",              // Fire
  "Радужный марш",      // Rainbow March
  "Плазма",             // Plasma
  "Шум",                // Noise
  "Жонглирование",      // Juggle
  "Один цвет"           // Solid Color
};

// Авто-переключение режимов
unsigned long lastModeSwitch = 0;

// Отслеживание последней проверки расписания
int lastCheckedMinute = -1;

// Функция синхронизации времени через HTTP API
bool syncTimeViaHTTP() {
  WiFiClient client;
  HTTPClient http;
  
  LOG_PRINTLN("🌐 Fetching time via HTTP API...");
  
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
        
        LOG_PRINT("✅ Time synced via HTTP: ");
        LOG_PRINTLN(String(timestamp));
        
        http.end();
        return true;
      }
    }
  } else {
    LOG_PRINT("❌ HTTP request failed: ");
    LOG_PRINTLN(String(httpCode));
  }
  
  http.end();
  return false;
}

void setup() {
  Serial.begin(115200);
  LOG_PRINTLN("\n\n🎄 WiFi LED Garland Starting...");
  LOG_PRINTLN("BOOT: System Restarted");
  LOG_PRINTF("Reset Reason: %s\n", ESP.getResetReason().c_str());
  
  // Инициализация LED state
  initLEDState();
  loadLEDState();
  
  // Инициализация LED ленты ПЕРЕД подключением к WiFi для анимации
  initLEDs();
  
  // Подключение к WiFi
  LOG_PRINT("Connecting to WiFi: ");
  LOG_PRINTLN(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  
#ifdef USE_STATIC_IP
  // Настройка статического IP
  IPAddress local_IP(STATIC_IP);
  IPAddress gateway(GATEWAY_IP);
  IPAddress subnet(SUBNET_MASK);
  
  if (!WiFi.config(local_IP, gateway, subnet)) {
    LOG_PRINTLN("Failed to configure static IP");
  }
#endif
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Анимация подключения на LED
  int dotCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    LOG_PRINT(".");
    
    // Бегущий огонёк во время подключения
    FastLED.clear();
    leds[dotCount % ledState.numLeds] = CRGB::Blue;
    FastLED.show();
    dotCount++;
    
    // Таймаут 30 секунд
    if (dotCount > 60) {
      LOG_PRINTLN("\n❌ WiFi connection failed!");
      LOG_PRINTLN("Please check WIFI_SSID and WIFI_PASSWORD in config.h");
      
      // Красная вспышка - ошибка
      fill_solid(leds, ledState.numLeds, CRGB::Red);
      FastLED.show();
      delay(2000);
      
      ESP.restart();
    }
  }
  
  LOG_PRINTLN("\n✅ WiFi connected!");
  LOG_PRINT("IP address: ");
  LOG_PRINTLN(WiFi.localIP().toString());
  
  // Зелёная вспышка - успех
  fill_solid(leds, ledState.numLeds, CRGB::Green);
  FastLED.show();
  delay(1000);
  FastLED.clear();
  FastLED.show();
  
  // Инициализация NTP через встроенные функции ESP8266
  LOG_PRINTLN("🕐 Initializing NTP...");
  LOG_PRINT("NTP Server: ");
  LOG_PRINTLN(NTP_SERVER);
  LOG_PRINT("Timezone: UTC+");
  LOG_PRINTLN(String(NTP_OFFSET / 3600));
  
  // Настраиваем NTP (сервер, смещение в секундах, летнее время = 0)
  configTime(NTP_OFFSET, 0, NTP_SERVER);
  
  // Ждем синхронизации времени
  LOG_PRINT("Waiting for NTP sync");
  int ntpRetries = 0;
  time_t now = time(nullptr);
  
  while (now < 1000000000 && ntpRetries < 10) {  // Уменьшили попытки NTP
    delay(500);
    LOG_PRINT(".");
    now = time(nullptr);
    ntpRetries++;
  }
  
  if (now >= 1000000000) {
    LOG_PRINTLN("\n✅ NTP time synchronized");
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    LOG_PRINT("Current time: ");
    LOG_PRINTLN(String(asctime(&timeinfo)));
  } else {
    LOG_PRINTLN("\n⚠️ NTP sync failed, trying HTTP API...");
    
    // Пробуем синхронизацию через HTTP
    if (syncTimeViaHTTP()) {
      now = time(nullptr);
      struct tm timeinfo;
      localtime_r(&now, &timeinfo);
      LOG_PRINT("Current time: ");
      LOG_PRINTLN(String(asctime(&timeinfo)));
    } else {
      LOG_PRINTLN("⚠️ HTTP sync also failed, time will be set from browser");
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
    LOG_PRINTLN("Start updating " + type);
    // Выключаем LED во время обновления
    FastLED.clear();
    FastLED.show();
  });
  
  ArduinoOTA.onEnd([]() {
    LOG_PRINTLN("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    LOG_PRINTF("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    LOG_PRINTF("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) LOG_PRINTLN("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) LOG_PRINTLN("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) LOG_PRINTLN("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) LOG_PRINTLN("Receive Failed");
    else if (error == OTA_END_ERROR) LOG_PRINTLN("End Failed");
  });
  
  ArduinoOTA.begin();
  LOG_PRINTLN("✅ OTA Ready");
  
  // Запуск веб-сервера
  setupWebServer();
  
  LOG_PRINTLN("\n🌐 Web server started");
  LOG_PRINT("Open http://");
  LOG_PRINT(WiFi.localIP().toString());
  LOG_PRINTLN("/ in your browser\n");
  
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
    settingsChanged = true;
    
    LOG_PRINT("⏰ Schedule triggered: ");
    LOG_PRINT(schedule.action ? "ON" : "OFF");
    LOG_PRINT(" at ");
    LOG_PRINT(String(currentHour));
    LOG_PRINT(":");
    LOG_PRINTLN(String(currentMinute));
  }
}

void loop() {
  diag.loopStart();

  // Обработка OTA запросов
  diag.taskStart("OTA");
  ArduinoOTA.handle();
  diag.taskEnd();
  
  // Обработка HTTP запросов
  diag.taskStart("WebServer");
  handleWebServer();
  diag.taskEnd();
  
  // Ресинхронизация времени каждый час через HTTP
  EVERY_N_SECONDS(3600) {
    time_t now = time(nullptr);
    if (now < 1000000000) {
      // Время не синхронизировано, пробуем снова
      LOG_PRINTLN("⏰ Time not synced, attempting HTTP sync...");
      syncTimeViaHTTP();
    }
  }
  
  // Проверка расписаний каждую секунду
  EVERY_N_SECONDS(1) {
    diag.taskStart("Schedules");
    checkSchedules();
    diag.taskEnd();
  }
  
  // Set brightness once per frame to avoid flickering
  FastLED.setBrightness(ledState.brightness);
  
  // Запуск текущего режима LED
  EVERY_N_MILLISECONDS(20) {
    diag.taskStart("LEDs");
    
    // Check if time is synchronized (year > 2001)
    if (time(nullptr) > 1000000000) {
      runMode(ledState.currentMode);
    } else {
      // Time not synced yet - show solid green to indicate waiting state
      // This matches the "success connection" color, keeping it until we get time
      fill_solid(leds, ledState.numLeds, CRGB::Green);
    }
    
    // Show the frame
    FastLED.show();
    diag.taskEnd();
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
          // Ищем текущий режим в массиве активных
          int currentIndex = -1;
          for (uint8_t i = 0; i < activeCount; i++) {
            if (activeModes[i] == ledState.currentMode) {
              currentIndex = i;
              break;
            }
          }
          
          // Переключаемся на следующий в списке активных
          if (currentIndex >= 0) {
            // Текущий режим найден, берем следующий (с циклом)
            currentIndex = (currentIndex + 1) % activeCount;
          } else {
            // Текущий режим не в списке активных (возможно архивирован), берем первый
            currentIndex = 0;
          }
          
          ledState.currentMode = activeModes[currentIndex];
        }
        
        
        // Проверка границ для безопасности
        if (ledState.currentMode >= TOTAL_MODES) {
          ledState.currentMode = 0;  // Fallback to first mode
        }
        
        // Используем sprintf вместо String для избежания фрагментации кучи
        char logMsg[100];
        snprintf(logMsg, sizeof(logMsg), "Auto-switched to mode: %s (%d)", 
                 MODE_NAMES[ledState.currentMode], 
                 ledState.currentMode);
        LOG_PRINTLN(logMsg);
        
        // НЕ сохраняем в EEPROM при автопереключении!
        // Частые записи в EEPROM вызывают watchdog reset и крэши.
        // Режим сохраняется только при ручном выборе через веб-интерфейс.
        // saveLEDState();  // ← REMOVED to prevent crashes
      }
    }
  }
  

  
  // Сохранение настроек в главном цикле (безопасно для EEPROM)
  if (settingsChanged) {
    LOG_PRINTLN("💾 Settings changed, saving to EEPROM...");
    saveLEDState();
    settingsChanged = false;
    LOG_PRINTLN("✅ Settings saved");
  }

  diag.loopEnd();
}
