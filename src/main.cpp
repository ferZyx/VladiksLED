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

/**
 * Кэш параметров точки доступа в RTC-памяти для быстрого подключения к WiFi.
 * RTC user memory переживает программный рестарт и перезагрузку после OTA,
 * но стирается при отключении питания — тогда просто будет обычное подключение.
 */
struct WifiRtcCache {
  uint32_t magic;     // признак валидной записи
  uint8_t channel;    // канал WiFi точки доступа
  uint8_t bssid[6];   // MAC точки доступа
  uint8_t reserved;   // выравнивание до 4 байт
  uint32_t checksum;  // контроль целостности (мусор после холодного старта)
};

// Смещение в блоках по 4 байта. Первые ~128 байт RTC user memory использует eboot
// для команды OTA-копирования, поэтому берём область заведомо дальше.
static const uint32_t WIFI_RTC_OFFSET = 96;
static const uint32_t WIFI_RTC_MAGIC = 0x57494649;  // "WIFI"

/**
 * Простая контрольная сумма записи кэша (FNV-1a по всем полям, кроме checksum).
 * @param c запись кэша
 * @returns 32-битная сумма
 */
static uint32_t wifiCacheChecksum(const WifiRtcCache& c) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&c);
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < offsetof(WifiRtcCache, checksum); i++) {
    h = (h ^ p[i]) * 16777619u;
  }
  return h;
}

/**
 * Читает кэш точки доступа из RTC-памяти.
 * @param out куда положить данные
 * @returns true, если запись валидна
 */
static bool loadWifiCache(WifiRtcCache& out) {
  if (!ESP.rtcUserMemoryRead(WIFI_RTC_OFFSET, reinterpret_cast<uint32_t*>(&out), sizeof(out))) {
    return false;
  }
  return out.magic == WIFI_RTC_MAGIC && out.checksum == wifiCacheChecksum(out) &&
         out.channel >= 1 && out.channel <= 14;
}

/**
 * Сохраняет канал и BSSID текущей точки доступа в RTC-память.
 * @param channel канал WiFi
 * @param bssid MAC точки доступа (6 байт)
 */
static void saveWifiCache(uint8_t channel, const uint8_t* bssid) {
  if (bssid == nullptr) return;
  WifiRtcCache c = {};
  c.magic = WIFI_RTC_MAGIC;
  c.channel = channel;
  memcpy(c.bssid, bssid, sizeof(c.bssid));
  c.checksum = wifiCacheChecksum(c);
  ESP.rtcUserMemoryWrite(WIFI_RTC_OFFSET, reinterpret_cast<uint32_t*>(&c), sizeof(c));
}

/**
 * Инвалидирует кэш (например, если быстрое подключение не удалось).
 */
static void clearWifiCache() {
  WifiRtcCache c = {};
  ESP.rtcUserMemoryWrite(WIFI_RTC_OFFSET, reinterpret_cast<uint32_t*>(&c), sizeof(c));
}

/**
 * Переводит календарную дату (UTC) в число дней от 1970-01-01.
 * Алгоритм Говарда Хиннанта (days_from_civil): не зависит от TZ и не требует timegm(),
 * которого нет в newlib для ESP8266.
 * @param y год (например, 2026)
 * @param m месяц 1..12
 * @param d день месяца 1..31
 * @returns количество дней от эпохи Unix
 */
static long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097L + (long)doe - 719468L;
}

/**
 * Разбирает HTTP-заголовок Date (RFC 7231, например "Tue, 24 Nov 2026 17:56:35 GMT")
 * в Unix-время UTC.
 * @param date значение заголовка Date
 * @returns Unix-время или 0, если строку разобрать не удалось
 */
static time_t parseHttpDate(const String& date) {
  char mon[4] = {0};
  int day, year, hh, mm, ss;
  // Пропускаем день недели ("Tue, "), дальше фиксированный формат
  int comma = date.indexOf(',');
  if (comma < 0) return 0;
  if (sscanf(date.c_str() + comma + 1, " %d %3s %d %d:%d:%d", &day, mon, &year, &hh, &mm, &ss) != 6) {
    return 0;
  }
  static const char MONTHS[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* p = strstr(MONTHS, mon);
  if (p == nullptr) return 0;
  unsigned month = (unsigned)((p - MONTHS) / 3) + 1;
  return (time_t)(daysFromCivil(year, month, (unsigned)day) * 86400L + hh * 3600L + mm * 60L + ss);
}

/**
 * Фолбэк-синхронизация времени по HTTP, когда NTP (UDP 123) недоступен.
 * Берём заголовок Date из лёгкого ответа 204 от Google: он есть у любого HTTP-сервера,
 * не требует парсинга JSON и не зависит от сторонних time-API (worldtimeapi.org
 * нестабилен и рвал соединение). Точность — до секунды, для расписаний достаточно.
 * @returns true, если время установлено
 */
bool syncTimeViaHTTP() {
  WiFiClient client;
  HTTPClient http;

  LOG_PRINTLN(F("🌐 Fetching time via HTTP Date header..."));

  http.begin(client, "http://connectivitycheck.gstatic.com/generate_204");
  http.setTimeout(5000);  // 5 секунд таймаут: вызывается из loop(), дольше блокировать нельзя
  const char* headerKeys[] = {"Date"};
  http.collectHeaders(headerKeys, 1);

  int httpCode = http.GET();
  bool ok = false;

  if (httpCode > 0) {
    time_t timestamp = parseHttpDate(http.header("Date"));
    if (timestamp > 1000000000) {
      // Date приходит в UTC; смещение пояса применяет localtime() через TZ из configTime()
      timeval tv = { timestamp, 0 };
      settimeofday(&tv, nullptr);
      LOG_PRINTF("✅ Time synced via HTTP: %ld\n", (long)timestamp);
      ok = true;
    } else {
      LOG_PRINTLN(F("❌ HTTP Date header missing or invalid"));
    }
  } else {
    LOG_PRINTF("❌ HTTP request failed: %d\n", httpCode);
  }

  http.end();
  return ok;
}

void setup() {
  Serial.begin(115200);
  LOG_PRINTLN(F("\n\n🎄 WiFi LED Garland Starting..."));
  LOG_PRINTLN(F("BOOT: System Restarted"));
  LOG_PRINTF("Reset Reason: %s\n", ESP.getResetReason().c_str());
  // millis() считается от сброса чипа: после OTA сюда входит копирование образа
  // загрузчиком eboot, поэтому значение здесь показывает «стоимость» перезагрузки
  LOG_PRINTF("setup() started at %lu ms\n", millis());
  
  // Инициализация LED state
  initLEDState();
  loadLEDState();
  
  // Инициализация LED ленты ПЕРЕД подключением к WiFi для анимации
  initLEDs();
  
  // Подключение к WiFi
  LOG_PRINT(F("Connecting to WiFi: "));
  LOG_PRINTLN(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);

  // Диагностика загрузки: логируем события WiFi с отметкой millis()
  static WiFiEventHandler onConnectedHandler = WiFi.onStationModeConnected(
    [](const WiFiEventStationModeConnected&) { LOG_PRINTF("📶 WiFi associated at %lu ms\n", millis()); });
  static WiFiEventHandler onGotIpHandler = WiFi.onStationModeGotIP(
    [](const WiFiEventStationModeGotIP&) { LOG_PRINTF("📶 WiFi got IP at %lu ms\n", millis()); });
  static WiFiEventHandler onDisconnectedHandler = WiFi.onStationModeDisconnected(
    [](const WiFiEventStationModeDisconnected& e) { LOG_PRINTF("📶 WiFi disconnected at %lu ms, reason %d\n", millis(), e.reason); });

#ifdef USE_STATIC_IP
  // Настройка статического IP
  IPAddress local_IP(STATIC_IP);
  IPAddress gateway(GATEWAY_IP);
  IPAddress subnet(SUBNET_MASK);
  // DNS обязателен: без него при статическом IP lwIP остаётся с DNS 0.0.0.0,
  // и ни NTP (time.google.com), ни HTTP-фолбэк не могут разрешить имена хостов.
  // Основной — роутер, запасной — публичный DNS Google.
  IPAddress dns1(GATEWAY_IP);
  IPAddress dns2(8, 8, 8, 8);

  if (!WiFi.config(local_IP, gateway, subnet, dns1, dns2)) {
    LOG_PRINTLN(F("Failed to configure static IP"));
  }
#endif
  
  // Быстрое подключение: если в RTC-памяти есть канал/BSSID с прошлой загрузки,
  // подключаемся к ним напрямую без сканирования эфира (полное сканирование
  // занимало ~12 с из ~14 с загрузки и тормозило каждый OTA-деплой).
  WifiRtcCache wifiCache;
  bool fastConnect = loadWifiCache(wifiCache);
  if (fastConnect) {
    LOG_PRINTF("⚡ WiFi fast connect: channel %u\n", wifiCache.channel);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, wifiCache.channel, wifiCache.bssid, true);
  } else {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  // Ожидание подключения с анимацией на LED. Опрашиваем статус каждые 100 мс
  // (раньше 500 мс — до полусекунды лишнего ожидания), огонёк сдвигается раз в 500 мс.
  const unsigned long wifiStart = millis();
  const unsigned long FAST_CONNECT_TIMEOUT_MS = 5000;
  int dotCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);

    // Канал/BSSID устарели (роутер сменил канал, другая точка) — обычное подключение со сканированием
    if (fastConnect && millis() - wifiStart > FAST_CONNECT_TIMEOUT_MS) {
      LOG_PRINTLN(F("\n⚠️ Fast connect failed, falling back to full scan"));
      fastConnect = false;
      clearWifiCache();
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    if ((millis() - wifiStart) / 500 <= (unsigned long)dotCount) continue;
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
  
  LOG_PRINTF("\n✅ WiFi connected in %lu ms\n", millis() - wifiStart);
  LOG_PRINT(F("IP address: "));
  LOG_PRINTLN(WiFi.localIP().toString());

  // Запоминаем канал и BSSID для быстрого подключения при следующей перезагрузке
  saveWifiCache((uint8_t)WiFi.channel(), WiFi.BSSID());

  // Короткая зелёная вспышка — WiFi подключён (150 мс: заметно глазу, но не тормозит загрузку)
  fill_solid(leds, ledState.numLeds, CRGB::Green);
  FastLED.show();
  delay(150);
  FastLED.clear();
  FastLED.show();
  
  // Инициализация NTP через встроенные функции ESP8266
  LOG_PRINTLN(F("🕐 Initializing NTP..."));
  LOG_PRINT(F("NTP Server: "));
  LOG_PRINTLN(NTP_SERVER);
  LOG_PRINT(F("Timezone: UTC+"));
  LOG_PRINTLN(String(NTP_OFFSET / 3600));
  
  // Настраиваем NTP (смещение в секундах, летнее время = 0, до трёх серверов).
  // Не ждём синхронизации в setup(): SNTP в ядре ESP8266 работает в фоне и сам
  // повторяет запросы, а loop() отслеживает момент синхронизации и при необходимости
  // включает HTTP-фолбэк (см. «Контроль синхронизации времени» в loop()).
  // Это сокращает загрузку (и время OTA-деплоя) на несколько секунд.
  configTime(NTP_OFFSET, 0, NTP_SERVER, "pool.ntp.org", "time.cloudflare.com");
  
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
  
  LOG_PRINTF("\n🌐 Web server started at %lu ms\n", millis());
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
  
  // Контроль синхронизации времени (раз в секунду, неблокирующе):
  //  - один раз логируем момент, когда время стало валидным (обычно NTP за 1–3 с);
  //  - если через TIME_SYNC_FALLBACK_MS после старта NTP так и не ответил —
  //    пробуем HTTP-фолбэк, затем повторяем каждые TIME_SYNC_RETRY_MS.
  EVERY_N_SECONDS(1) {
    static bool timeSyncLogged = false;
    static unsigned long lastHttpSyncAttempt = 0;
    const unsigned long TIME_SYNC_FALLBACK_MS = 15000;
    const unsigned long TIME_SYNC_RETRY_MS = 300000;

    time_t now = time(nullptr);
    if (now >= 1000000000) {
      if (!timeSyncLogged) {
        timeSyncLogged = true;
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        LOG_PRINTF("✅ Time synchronized: %02d:%02d:%02d (uptime %lu ms)\n",
                   timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, millis());
      }
    } else if (millis() >= TIME_SYNC_FALLBACK_MS &&
               (lastHttpSyncAttempt == 0 || millis() - lastHttpSyncAttempt >= TIME_SYNC_RETRY_MS)) {
      lastHttpSyncAttempt = millis();
      LOG_PRINTLN(F("⏰ NTP not synced yet, attempting HTTP sync..."));
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

    // Виртуальные часы анимации: общая скорость своя у гирлянды и у подсветки
    advanceAnimationClock(ledState.operationMode == MODE_GARLAND
                            ? ledState.garlandSpeed : ledState.backlightSpeed);
    
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
      fillFrameSolid(CRGB::Green);
    }
    
    // Переносим кадр в выходной буфер со сглаживанием (плавное зажигание/угасание,
    // кроссфейд при смене режима). При выключенном питании показываем кадры,
    // пока лента плавно гаснет, затем перестаём тратить CPU на вывод.
    bool outputChanged = presentFrame();
    if (ledState.power) {
      powerOffFrameShown = false;
      FastLED.show();
      diag.recordLedFrame(millis() - frameStart);
    } else if (outputChanged || !powerOffFrameShown) {
      FastLED.show();
      powerOffFrameShown = !outputChanged;
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
