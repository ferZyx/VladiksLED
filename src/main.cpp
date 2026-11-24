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

// Названия режимов гирлянды (должны совпадать с frontend)
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
  "Один цвет",          // Solid Color
  "Снегопад",           // Snowfall
  "Северное сияние",    // Aurora
  "Светлячки"           // Fireflies
};

// Названия режимов подсветки
const char* BACKLIGHT_MODE_NAMES[] = {
  "Волны",              // Waves
  "Пульсация"           // Pulse
};

// Авто-переключение режимов гирлянды
unsigned long lastModeSwitch = 0;

// Авто-переключение для подсветки
unsigned long lastBacklightModeSwitch = 0;
unsigned long lastBacklightColorSwitch = 0;

// Отслеживание последней проверки расписания
int lastCheckedMinute = -1;

// Отложенное сохранение настроек: пишем в EEPROM только после паузы в изменениях.
// Запись стирает и перешивает сектор флеша (десятки мс, CPU и WiFi стоят),
// поэтому сохранять на каждое движение слайдера нельзя.
const unsigned long SETTINGS_SAVE_DELAY_MS = 3000;
bool settingsSavePending = false;
unsigned long lastSettingsChangeAt = 0;

// Был ли уже выведен «чёрный» кадр при выключенном питании (дальше show не нужен)
bool powerOffFrameShown = false;

// Функция синхронизации времени через HTTP API
bool syncTimeViaHTTP() {
  WiFiClient client;
  HTTPClient http;
  
  LOG_PRINTLN(F("🌐 Fetching time via HTTP API..."));
  
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
        
        LOG_PRINT(F("✅ Time synced via HTTP: "));
        LOG_PRINTLN(String(timestamp));
        
        http.end();
        return true;
      }
    }
  } else {
    LOG_PRINT(F("❌ HTTP request failed: "));
    LOG_PRINTLN(String(httpCode));
  }
  
  http.end();
  return false;
}

void setup() {
  Serial.begin(115200);
  LOG_PRINTLN(F("\n\n🎄 WiFi LED Garland Starting..."));
  LOG_PRINTLN(F("BOOT: System Restarted"));
  LOG_PRINTF("Reset Reason: %s\n", ESP.getResetReason().c_str());
  
  // Инициализация LED state
  initLEDState();
  loadLEDState();
  
  // Инициализация LED ленты ПЕРЕД подключением к WiFi для анимации
  initLEDs();
  
  // Подключение к WiFi
  LOG_PRINT(F("Connecting to WiFi: "));
  LOG_PRINTLN(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  
#ifdef USE_STATIC_IP
  // Настройка статического IP
  IPAddress local_IP(STATIC_IP);
  IPAddress gateway(GATEWAY_IP);
  IPAddress subnet(SUBNET_MASK);
  
  if (!WiFi.config(local_IP, gateway, subnet)) {
    LOG_PRINTLN(F("Failed to configure static IP"));
  }
#endif
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Анимация подключения на LED
  int dotCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    LOG_PRINT(F("."));
    
    // Бегущий огонёк во время подключения
    FastLED.clear();
    leds[dotCount % ledState.numLeds] = CRGB::Blue;
    FastLED.show();
    dotCount++;
    
    // Таймаут 30 секунд
    if (dotCount > 60) {
      LOG_PRINTLN(F("\n❌ WiFi connection failed!"));
      LOG_PRINTLN(F("Please check WIFI_SSID and WIFI_PASSWORD in config.h"));
      
      // Красная вспышка - ошибка
      fill_solid(leds, ledState.numLeds, CRGB::Red);
      FastLED.show();
      delay(2000);
      
      ESP.restart();
    }
  }
  
  LOG_PRINTLN(F("\n✅ WiFi connected!"));
  LOG_PRINT(F("IP address: "));
  LOG_PRINTLN(WiFi.localIP().toString());
  
  // Зелёная вспышка - успех
  fill_solid(leds, ledState.numLeds, CRGB::Green);
  FastLED.show();
  delay(1000);
  FastLED.clear();
  FastLED.show();
  
  // Инициализация NTP через встроенные функции ESP8266
  LOG_PRINTLN(F("🕐 Initializing NTP..."));
  LOG_PRINT(F("NTP Server: "));
  LOG_PRINTLN(NTP_SERVER);
  LOG_PRINT(F("Timezone: UTC+"));
  LOG_PRINTLN(String(NTP_OFFSET / 3600));
  
  // Настраиваем NTP (сервер, смещение в секундах, летнее время = 0)
  configTime(NTP_OFFSET, 0, NTP_SERVER);
  
  // Ждем синхронизации времени
  LOG_PRINT(F("Waiting for NTP sync"));
  int ntpRetries = 0;
  time_t now = time(nullptr);
  
  while (now < 1000000000 && ntpRetries < 10) {  // Уменьшили попытки NTP
    delay(500);
    LOG_PRINT(F("."));
    now = time(nullptr);
    ntpRetries++;
  }
  
  if (now >= 1000000000) {
    LOG_PRINTLN(F("\n✅ NTP time synchronized"));
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    LOG_PRINT(F("Current time: "));
    LOG_PRINTLN(String(asctime(&timeinfo)));
  } else {
    LOG_PRINTLN(F("\n⚠️ NTP sync failed, trying HTTP API..."));
    
    // Пробуем синхронизацию через HTTP
    if (syncTimeViaHTTP()) {
      now = time(nullptr);
      struct tm timeinfo;
      localtime_r(&now, &timeinfo);
      LOG_PRINT(F("Current time: "));
      LOG_PRINTLN(String(asctime(&timeinfo)));
    } else {
      LOG_PRINTLN(F("⚠️ HTTP sync also failed, time will be set from browser"));
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
    LOG_PRINTLN(F("\nEnd"));
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    LOG_PRINTF("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    LOG_PRINTF("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) LOG_PRINTLN(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) LOG_PRINTLN(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) LOG_PRINTLN(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) LOG_PRINTLN(F("Receive Failed"));
    else if (error == OTA_END_ERROR) LOG_PRINTLN(F("End Failed"));
  });
  
  ArduinoOTA.begin();
  LOG_PRINTLN(F("✅ OTA Ready"));
  
  // Запуск веб-сервера
  setupWebServer();
  
  LOG_PRINTLN(F("\n🌐 Web server started"));
  LOG_PRINT(F("Open http://"));
  LOG_PRINT(WiFi.localIP().toString());
  LOG_PRINTLN(F("/ in your browser\n"));
  
  lastModeSwitch = millis();
  lastBacklightModeSwitch = millis();
  lastBacklightColorSwitch = millis();
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
    
    LOG_PRINT(F("⏰ Schedule triggered: "));
    LOG_PRINT(schedule.action ? "ON" : "OFF");
    LOG_PRINT(F(" at "));
    LOG_PRINT(String(currentHour));
    LOG_PRINT(F(":"));
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
      LOG_PRINTLN(F("⏰ Time not synced, attempting HTTP sync..."));
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
    unsigned long frameStart = millis();

    // Длина вывода = реальное число LED (меняется через /api/leds)
    syncLedCount();
    
    // Check if time is synchronized (year > 2001) or if we don't need to wait
    if (!WAIT_FOR_TIME_SYNC || time(nullptr) > 1000000000) {
      // Выбираем режим в зависимости от operationMode
      if (ledState.operationMode == MODE_GARLAND) {
        runMode(ledState.currentMode);
      } else {
        runBacklightMode(ledState.backlight.currentMode);
      }
    } else {
      // Time not synced yet - show solid green to indicate waiting state
      // This matches the "success connection" color, keeping it until we get time
      fill_solid(leds, ledState.numLeds, CRGB::Green);
    }
    
    // Выводим кадр. При выключенном питании достаточно одного чёрного кадра —
    // дальше не тратим CPU на вывод, который ничего не меняет.
    if (ledState.power) {
      powerOffFrameShown = false;
      FastLED.show();
      diag.recordLedFrame(millis() - frameStart);
    } else if (!powerOffFrameShown) {
      FastLED.show();
      powerOffFrameShown = true;
    }
    diag.taskEnd();
  }
  
  // Авто-переключение режимов гирлянды (только если в режиме гирлянды)
  if (ledState.operationMode == MODE_GARLAND && ledState.autoSwitchDelay > 0) {
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
  
  // ===== Авто-переключение для режима подсветки =====
  if (ledState.operationMode == MODE_BACKLIGHT) {
    unsigned long now = millis();
    
    // Авто-переключение режимов подсветки
    if (ledState.backlight.modeAutoSwitchDelay > 0) {
      if (now - lastBacklightModeSwitch >= (ledState.backlight.modeAutoSwitchDelay * 1000UL)) {
        lastBacklightModeSwitch = now;
        
        // Собираем список активных режимов подсветки
        uint8_t activeModes[TOTAL_BACKLIGHT_MODES];
        uint8_t activeCount = 0;
        
        for (uint8_t i = 0; i < TOTAL_BACKLIGHT_MODES; i++) {
          if (!ledState.backlight.modeSettings[i].archived) {
            activeModes[activeCount++] = i;
          }
        }
        
        if (activeCount > 0) {
          if (ledState.backlight.modeRandomOrder) {
            uint8_t randomIndex = random8(activeCount);
            ledState.backlight.currentMode = activeModes[randomIndex];
          } else {
            int currentIndex = -1;
            for (uint8_t i = 0; i < activeCount; i++) {
              if (activeModes[i] == ledState.backlight.currentMode) {
                currentIndex = i;
                break;
              }
            }
            currentIndex = (currentIndex >= 0) ? (currentIndex + 1) % activeCount : 0;
            ledState.backlight.currentMode = activeModes[currentIndex];
          }
          
          if (ledState.backlight.currentMode >= TOTAL_BACKLIGHT_MODES) {
            ledState.backlight.currentMode = 0;
          }
          
          char logMsg[80];
          snprintf(logMsg, sizeof(logMsg), "Backlight mode auto-switched to: %s (%d)", 
                   BACKLIGHT_MODE_NAMES[ledState.backlight.currentMode], 
                   ledState.backlight.currentMode);
          LOG_PRINTLN(logMsg);
        }
      }
    }
    
    // Авто-переключение цветов подсветки
    if (ledState.backlight.colorAutoSwitchDelay > 0) {
      if (now - lastBacklightColorSwitch >= (ledState.backlight.colorAutoSwitchDelay * 1000UL)) {
        lastBacklightColorSwitch = now;
        
        // Собираем список активных цветов
        uint8_t activeColors[MAX_BACKLIGHT_COLORS];
        uint8_t activeCount = 0;
        
        for (uint8_t i = 0; i < MAX_BACKLIGHT_COLORS; i++) {
          if (ledState.backlight.colors[i].enabled) {
            activeColors[activeCount++] = i;
          }
        }
        
        if (activeCount > 0) {
          if (ledState.backlight.colorRandomOrder) {
            uint8_t randomIndex = random8(activeCount);
            ledState.backlight.activeColorIndex = activeColors[randomIndex];
          } else {
            int currentIndex = -1;
            for (uint8_t i = 0; i < activeCount; i++) {
              if (activeColors[i] == ledState.backlight.activeColorIndex) {
                currentIndex = i;
                break;
              }
            }
            currentIndex = (currentIndex >= 0) ? (currentIndex + 1) % activeCount : 0;
            ledState.backlight.activeColorIndex = activeColors[currentIndex];
          }
          
          CRGB c = ledState.backlight.colors[ledState.backlight.activeColorIndex].color;
          char logMsg[80];
          snprintf(logMsg, sizeof(logMsg), "Backlight color auto-switched to #%02X%02X%02X", c.r, c.g, c.b);
          LOG_PRINTLN(logMsg);
        }
      }
    }
  }
  
  // Отложенное сохранение настроек (debounce): каждое изменение из веба
  // сдвигает момент записи, во флеш пишем один раз после паузы
  if (settingsChanged) {
    settingsChanged = false;
    settingsSavePending = true;
    lastSettingsChangeAt = millis();
  }
  if (settingsSavePending && millis() - lastSettingsChangeAt >= SETTINGS_SAVE_DELAY_MS) {
    settingsSavePending = false;
    diag.taskStart("EEPROM");
    unsigned long saveStart = millis();
    saveLEDState();
    unsigned long saveMs = millis() - saveStart;
    diag.taskEnd();
    diag.recordEepromSave(saveMs);
    LOG_PRINTF("💾 Settings saved (%lums)\n", saveMs);
  }

  diag.loopEnd();
}
