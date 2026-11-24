#include "webserver.h"
#include "webpage.h"
#include "config.h"
#include "logger.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

// Define HTTP method constants if not defined
#ifndef HTTP_ANY
#define HTTP_ANY 0xFF
#endif
#ifndef HTTP_GET
#define HTTP_GET 0x01
#endif
#ifndef HTTP_POST
#define HTTP_POST 0x02
#endif
#ifndef HTTP_DELETE
#define HTTP_DELETE 0x04
#endif

#include "diagnostics.h"

AsyncWebServer server(WEB_SERVER_PORT);
AsyncWebSocket ws(LOG_WEBSOCKET_PATH);

// Минимальный непрерывный блок кучи, при котором ещё берёмся отвечать на GET.
// Ниже — отдаём 503 сразу: лучше быстрый отказ (UI повторит), чем OOM-ребут.
#define MIN_FREE_BLOCK_FOR_RESPONSE 4096

// Максимум одновременных WebSocket-клиентов логов: каждый держит свою очередь сообщений
#define MAX_WS_CLIENTS 2

/**
 * Заглушка onRequest для POST-маршрутов: ответ уже отправлен из body-хендлера.
 * @param request входящий запрос
 */
static void onBodyRequestDone(AsyncWebServerRequest *request) {}

/**
 * Проверяет запас памяти перед формированием ответа.
 * @param request входящий запрос
 * @return true, если памяти мало и запросу уже ответили 503
 */
static bool rejectIfLowMemory(AsyncWebServerRequest *request) {
  if (ESP.getMaxFreeBlockSize() >= MIN_FREE_BLOCK_FOR_RESPONSE) return false;
  AsyncWebServerResponse *response = request->beginResponse(503, "application/json", "{\"error\":\"Low memory\"}");
  response->addHeader("Retry-After", "1");
  request->send(response);
  return true;
}

// WebSocket event handler
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    // Ограничиваем число клиентов: старые вкладки иначе копят очереди и съедают кучу
    if (server->count() > MAX_WS_CLIENTS) {
      client->close();
      return;
    }
    LOG_PRINTF("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    
    // Отправляем историю логов новому клиенту
    client->text(logger.getHistoryJson());
    
  } else if (type == WS_EVT_DISCONNECT) {
    LOG_PRINTF("WebSocket client #%u disconnected\n", client->id());
    
  } else if (type == WS_EVT_ERROR) {
    LOG_PRINTF("WebSocket client #%u error(%u): %s\n", client->id(), *((uint16_t*)arg), (char*)data);
    
  } else if (type == WS_EVT_DATA) {
    // Обработка входящих сообщений от клиента (если нужно)
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = 0;
      LOG_PRINTF("WebSocket message from client #%u: %s\n", client->id(), (char*)data);
    }
  }
}

void setupWebServer() {
  // Настройка WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  logger.setWebSocket(&ws);
  
  // Главная страница
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (rejectIfLowMemory(request)) return;
    request->send_P(200, "text/html", WEBPAGE);
  });
  
  // API endpoints - GET requests
  server.on("/api/state", HTTP_GET, handleGetState);
  server.on("/api/mode/settings/get", HTTP_GET, handleGetModeSettings);
  server.on("/api/schedules", HTTP_GET, handleGetSchedules);
  server.on("/api/time", HTTP_GET, handleGetTime);
  server.on("/api/debug", HTTP_GET, handleGetDebug);
  server.on("/api/backlight/state", HTTP_GET, handleGetBacklightState);
  server.on("/api/backlight/colors", HTTP_GET, handleGetBacklightColors);
  
  // API endpoints - POST requests with body
  // Note: The first lambda is called when request completes (after body), 
  // the body handler is the last parameter
  // IMPORTANT: Register more specific routes FIRST (e.g., /api/mode/settings before /api/mode)
  server.on("/api/power", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleSetPower);
  server.on("/api/brightness", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleSetBrightness);
  server.on("/api/leds", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleSetLEDs);
  // Mode-related routes - more specific first!
  server.on("/api/mode/settings", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleSetModeSettings);
  server.on("/api/mode/reset", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleResetModeSettings);
  server.on("/api/mode/archive", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleToggleModeArchive);
  server.on("/api/mode", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleSetMode);
  // Other routes
  server.on("/api/auto-switch", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleSetAutoSwitch);
  server.on("/api/schedules", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleSetSchedule);
  server.on("/api/time/set", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleSetTime);
  
  // Backlight API endpoints (more specific routes FIRST)
  server.on("/api/operation-mode", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleSetOperationMode);
  server.on("/api/backlight/mode/settings", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleSetBacklightModeSettings);
  server.on("/api/backlight/mode", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleSetBacklightMode);
  server.on("/api/backlight/active-color", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleSetBacklightActiveColor);
  server.on("/api/backlight/colors", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleSetBacklightColor);
  server.on("/api/backlight/auto-switch", HTTP_POST, 
    onBodyRequestDone, 
    NULL, handleSetBacklightAutoSwitch);
  
  // DELETE request
  server.on("/api/schedules", HTTP_DELETE, handleDeleteSchedule);
  server.on("/api/backlight/colors", HTTP_DELETE, handleDeleteBacklightColor);
  
  server.onNotFound([](AsyncWebServerRequest *request){
    LOG_PRINTF("404: %s %s\n", request->methodToString(), request->url().c_str());
    request->send(404, "text/plain", "Not Found");
  });
  
  server.begin();
  LOG_PRINTLN(F("HTTP server started"));
}

void handleWebServer() {
  // AsyncWebServer обрабатывает запросы автоматически
  // Нужно только очищать WebSocket клиентов
  ws.cleanupClients();
}

void handleGetState(AsyncWebServerRequest *request) {
  if (rejectIfLowMemory(request)) return;
  
  // Используем chunked response чтобы не держать весь JSON в памяти
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  
  response->print("{");
  response->printf("\"power\":%s,", ledState.power ? "true" : "false");
  response->printf("\"brightness\":%d,", ledState.brightness);
  response->printf("\"numLeds\":%d,", ledState.numLeds);
  response->printf("\"currentMode\":%d,", ledState.currentMode);
  response->printf("\"autoSwitchDelay\":%u,", (unsigned)ledState.autoSwitchDelay);
  response->printf("\"randomOrder\":%s,", ledState.randomOrder ? "true" : "false");
  response->printf("\"operationMode\":%d,", (uint8_t)ledState.operationMode);
  
  // Настройки режимов гирлянды
  response->print("\"modeSettings\":[");
  for (int i = 0; i < TOTAL_MODES; i++) {
    if (i > 0) response->print(",");
    response->printf("{\"speed\":%d,\"scale\":%d,\"brightness\":%d,\"archived\":%s}",
      ledState.modeSettings[i].speed,
      ledState.modeSettings[i].scale,
      ledState.modeSettings[i].brightness,
      ledState.modeSettings[i].archived ? "true" : "false");
  }
  response->print("]}");
  
  request->send(response);
  
}

void handleSetPower(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Тело может прийти несколькими чанками — обрабатываем только полностью полученное
  if (index + len != total) return;

  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (!error) {
    LOG_PRINTF("API: Power set to %s\n", doc["on"] ? "ON" : "OFF");
    ledState.power = doc["on"];
    settingsChanged = true;
    request->send(200, "application/json", "{\"success\":true}");
    return;
  }
  
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleSetBrightness(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Тело может прийти несколькими чанками — обрабатываем только полностью полученное
  if (index + len != total) return;

  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (!error) {
    LOG_PRINTF("API: Brightness set to %d\n", (int)doc["value"]);
    ledState.brightness = doc["value"];
    settingsChanged = true;
    request->send(200, "application/json", "{\"success\":true}");
    return;
  }
  
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleSetLEDs(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Тело может прийти несколькими чанками — обрабатываем только полностью полученное
  if (index + len != total) return;

  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (!error) {
    uint16_t count = doc["count"];
    if (count > 0 && count <= MAX_LEDS) {
      ledState.numLeds = count;
      settingsChanged = true;
      request->send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleSetMode(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Тело может прийти несколькими чанками — обрабатываем только полностью полученное
  if (index + len != total) return;

  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (!error) {
    uint8_t mode = doc["mode"];
    LOG_PRINTF("API: Set Mode request: %d\n", mode);
    if (mode < TOTAL_MODES) {
      ledState.currentMode = mode;
      settingsChanged = true;
      request->send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleSetModeSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  
  // Handle chunked body - only process when we have complete data
  if (index + len != total) {
    return;  // Wait for more data
  }
  
  
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (error) {
    LOG_PRINTF("API: SetModeSettings - JSON parse error: %s\n", error.c_str());
    request->send(400, "application/json", "{\"error\":\"JSON parse error\"}");
    return;
  }
  
  if (!doc.containsKey("modeId")) {
    LOG_PRINTLN(F("API: SetModeSettings - Missing modeId field"));
    request->send(400, "application/json", "{\"error\":\"Missing modeId\"}");
    return;
  }
  
  int modeId = doc["modeId"];
  
  if (modeId < 0 || modeId >= TOTAL_MODES) {
    LOG_PRINTF("API: SetModeSettings - Invalid modeId: %d (max: %d)\n", modeId, TOTAL_MODES - 1);
    request->send(400, "application/json", "{\"error\":\"Invalid mode ID\"}");
    return;
  }
  
  
  if (doc.containsKey("speed")) {
    ledState.modeSettings[modeId].speed = doc["speed"];
  }
  if (doc.containsKey("scale")) {
    ledState.modeSettings[modeId].scale = doc["scale"];
  }
  if (doc.containsKey("brightness")) {
    ledState.modeSettings[modeId].brightness = doc["brightness"];
  }
  
  
  settingsChanged = true;
  request->send(200, "application/json", "{\"success\":true}");
}

void handleGetModeSettings(AsyncWebServerRequest *request) {
  if (rejectIfLowMemory(request)) return;
  if (request->hasParam("modeId")) {
    int modeId = request->getParam("modeId")->value().toInt();
    
    if (modeId < 0 || modeId >= TOTAL_MODES) {
      request->send(400, "application/json", "{\"error\":\"Invalid mode ID\"}");
      return;
    }
    
    StaticJsonDocument<256> doc;
    doc["speed"] = ledState.modeSettings[modeId].speed;
    doc["scale"] = ledState.modeSettings[modeId].scale;
    doc["brightness"] = ledState.modeSettings[modeId].brightness;
    doc["archived"] = ledState.modeSettings[modeId].archived;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
    return;
  }
  
  request->send(400, "application/json", "{\"error\":\"Missing modeId parameter\"}");
}

void handleResetModeSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  
  if (index + len != total) {
    return;  // Wait for complete data
  }
  
  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (!error && doc.containsKey("modeId")) {
    int modeId = doc["modeId"];
    
    if (modeId < 0 || modeId >= TOTAL_MODES) {
      LOG_PRINTF("API: Reset Settings - Invalid Mode %d\n", modeId);
      request->send(400, "application/json", "{\"error\":\"Invalid mode ID\"}");
      return;
    }

    LOG_PRINTF("API: Reset Settings for Mode %d\n", modeId);
    
    // Reset to default values
    ledState.modeSettings[modeId].speed = 128;
    ledState.modeSettings[modeId].scale = 128;
    ledState.modeSettings[modeId].brightness = 255;
    // Don't reset archived status or colors
    
    settingsChanged = true;
    request->send(200, "application/json", "{\"success\":true}");
    return;
  }
  
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleToggleModeArchive(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  
  if (index + len != total) {
    return;  // Wait for complete data
  }
  
  
  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (!error && doc.containsKey("modeId")) {
    int modeId = doc["modeId"];
    
    if (modeId < 0 || modeId >= TOTAL_MODES) {
      LOG_PRINTF("API: Toggle Archive - Invalid Mode %d\n", modeId);
      request->send(400, "application/json", "{\"error\":\"Invalid mode ID\"}");
      return;;
    }
    
    bool archived = doc["archived"];
    LOG_PRINTF("API: Set Archive Mode %d to %s\n", modeId, archived ? "TRUE" : "FALSE");
    ledState.modeSettings[modeId].archived = archived;
    settingsChanged = true;
    request->send(200, "application/json", "{\"success\":true}");
    return;
  }
  
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleSetAutoSwitch(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Тело может прийти несколькими чанками — обрабатываем только полностью полученное
  if (index + len != total) return;

  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (!error) {
    ledState.autoSwitchDelay = doc["delay"];
    ledState.randomOrder = doc["random"];
    LOG_PRINTF("API: AutoSwitch Delay=%d, Random=%d\n", ledState.autoSwitchDelay, ledState.randomOrder);
    settingsChanged = true;
    request->send(200, "application/json", "{\"success\":true}");
    return;
  }
  
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleGetSchedules(AsyncWebServerRequest *request) {
  if (rejectIfLowMemory(request)) return;
  DynamicJsonDocument doc(2048);
  
  JsonArray schedules = doc.createNestedArray("schedules");
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    JsonObject schedule = schedules.createNestedObject();
    schedule["id"] = i;
    schedule["enabled"] = ledState.schedules[i].enabled;
    schedule["hour"] = ledState.schedules[i].hour;
    schedule["minute"] = ledState.schedules[i].minute;
    schedule["action"] = ledState.schedules[i].action;
    schedule["daysOfWeek"] = ledState.schedules[i].daysOfWeek;
  }
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void handleSetSchedule(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Тело может прийти несколькими чанками — обрабатываем только полностью полученное
  if (index + len != total) return;

  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (!error && doc.containsKey("id")) {
    int id = doc["id"];
    
    if (id < 0 || id >= MAX_SCHEDULES) {
      request->send(400, "application/json", "{\"error\":\"Invalid schedule ID\"}");
      return;
    }
    
    if (doc.containsKey("enabled")) {
      ledState.schedules[id].enabled = doc["enabled"];
    }
    if (doc.containsKey("hour")) {
      ledState.schedules[id].hour = doc["hour"];
    }
    if (doc.containsKey("minute")) {
      ledState.schedules[id].minute = doc["minute"];
    }
    if (doc.containsKey("action")) {
      ledState.schedules[id].action = doc["action"];
    }
    if (doc.containsKey("daysOfWeek")) {
      ledState.schedules[id].daysOfWeek = doc["daysOfWeek"];
    }
    
    LOG_PRINTF("API: Set Schedule %d. Act=%d, Time=%d:%d\n", id, ledState.schedules[id].action, ledState.schedules[id].hour, ledState.schedules[id].minute);
    
    settingsChanged = true;
    request->send(200, "application/json", "{\"success\":true}");
    return;
  }
  
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleDeleteSchedule(AsyncWebServerRequest *request) {
  
  if (request->hasParam("id")) {
    int id = request->getParam("id")->value().toInt();
    
    if (id < 0 || id >= MAX_SCHEDULES) {
      request->send(400, "application/json", "{\"error\":\"Invalid schedule ID\"}");
      return;
    }
    
    // Отключаем расписание
    ledState.schedules[id].enabled = false;
    settingsChanged = true;
    
    request->send(200, "application/json", "{\"success\":true}");
    return;
  }
  
  request->send(400, "application/json", "{\"error\":\"Missing id parameter\"}");
}

void handleGetTime(AsyncWebServerRequest *request) {
  if (rejectIfLowMemory(request)) return;
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  StaticJsonDocument<256> doc;
  doc["hour"] = timeinfo.tm_hour;
  doc["minute"] = timeinfo.tm_min;
  doc["second"] = timeinfo.tm_sec;
  doc["dayOfWeek"] = timeinfo.tm_wday;  // 0 = Sunday, 1 = Monday, ..., 6 = Saturday
  doc["timestamp"] = now;
  
  // Format time as HH:MM:SS
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  doc["formatted"] = timeStr;
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void handleSetTime(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Тело может прийти несколькими чанками — обрабатываем только полностью полученное
  if (index + len != total) return;

  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (!error && doc.containsKey("timestamp")) {
    time_t timestamp = doc["timestamp"];
    
    // Устанавливаем время на ESP8266
    timeval tv = { timestamp, 0 };
    settimeofday(&tv, nullptr);
    
    LOG_PRINT(F("⏰ Time set manually to: "));
    LOG_PRINTLN(String(timestamp));
    
    request->send(200, "application/json", "{\"success\":true}");
    return;
  }
  
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleGetDebug(AsyncWebServerRequest *request) {
  if (rejectIfLowMemory(request)) return;
  DynamicJsonDocument doc(1024);
  
  // NTP info
  doc["ntpServer"] = NTP_SERVER;
  doc["ntpOffset"] = NTP_OFFSET;
  doc["ntpOffsetHours"] = NTP_OFFSET / 3600;
  
  // Current time info
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  doc["epochTime"] = now;
  
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  doc["formattedTime"] = timeStr;
  
  doc["hours"] = timeinfo.tm_hour;
  doc["minutes"] = timeinfo.tm_min;
  doc["dayOfWeek"] = timeinfo.tm_wday;
  
  // Check if time is valid (epoch should be > 1000000000 for dates after 2001)
  doc["timeValid"] = now > 1000000000;
  
  // WiFi info
  doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
  doc["wifiRSSI"] = WiFi.RSSI();
  doc["ipAddress"] = WiFi.localIP().toString();
  
  // Uptime
  doc["uptimeMs"] = millis();

  // ===== Метрики здоровья устройства =====
  // freeHeap — свободно всего; maxFreeBlock — самый большой непрерывный кусок
  // (именно он решает, хватит ли памяти на ответ); fragmentation — % фрагментации
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["maxFreeBlock"] = ESP.getMaxFreeBlockSize();
  doc["heapFragmentation"] = ESP.getHeapFragmentation();
  doc["resetReason"] = ESP.getResetReason();
  doc["wsClients"] = ws.count();

  const DiagSnapshot& d = diag.snapshot();
  doc["minFreeHeap"] = d.minFreeHeap;
  doc["minMaxFreeBlock"] = d.minMaxFreeBlock;
  doc["loopsPerSec"] = d.loopsPerSec;
  doc["maxLoopMs"] = d.maxLoopMsWindow;
  doc["maxLoopMsBoot"] = d.maxLoopMsBoot;
  doc["ledFps"] = d.ledFps;
  doc["maxLedFrameMs"] = d.maxLedFrameMs;
  doc["slowestTask"] = d.slowestTask;
  doc["slowestTaskMs"] = d.slowestTaskMs;
  doc["eepromSaves"] = d.eepromSaves;
  doc["lastEepromSaveMs"] = d.lastSaveMs;

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

// =====================================================
// ===== BACKLIGHT API HANDLERS =====
// =====================================================

void handleSetOperationMode(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Тело может прийти несколькими чанками — обрабатываем только полностью полученное
  if (index + len != total) return;

  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (!error && doc.containsKey("mode")) {
    uint8_t mode = doc["mode"];
    if (mode <= 1) {
      ledState.operationMode = (OperationMode)mode;
      LOG_PRINTF("API: Operation mode set to %s\n", mode == 0 ? "GARLAND" : "BACKLIGHT");
      settingsChanged = true;
      request->send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleGetBacklightState(AsyncWebServerRequest *request) {
  if (rejectIfLowMemory(request)) return;
  
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  
  response->print("{");
  response->printf("\"currentMode\":%d,", ledState.backlight.currentMode);
  response->printf("\"modeAutoSwitchDelay\":%u,", (unsigned)ledState.backlight.modeAutoSwitchDelay);
  response->printf("\"modeRandomOrder\":%s,", ledState.backlight.modeRandomOrder ? "true" : "false");
  response->printf("\"activeColorIndex\":%d,", ledState.backlight.activeColorIndex);
  response->printf("\"colorAutoSwitchDelay\":%u,", (unsigned)ledState.backlight.colorAutoSwitchDelay);
  response->printf("\"colorRandomOrder\":%s,", ledState.backlight.colorRandomOrder ? "true" : "false");
  
  // Настройки режимов подсветки
  response->print("\"modeSettings\":[");
  for (int i = 0; i < TOTAL_BACKLIGHT_MODES; i++) {
    if (i > 0) response->print(",");
    response->printf("{\"speed\":%d,\"intensity\":%d,\"brightness\":%d,\"archived\":%s}",
      ledState.backlight.modeSettings[i].speed,
      ledState.backlight.modeSettings[i].intensity,
      ledState.backlight.modeSettings[i].brightness,
      ledState.backlight.modeSettings[i].archived ? "true" : "false");
  }
  response->print("]}");
  
  request->send(response);
  
}

void handleGetBacklightColors(AsyncWebServerRequest *request) {
  if (rejectIfLowMemory(request)) return;
  
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  
  response->print("{\"colors\":[");
  for (int i = 0; i < MAX_BACKLIGHT_COLORS; i++) {
    if (i > 0) response->print(",");
    response->printf("{\"index\":%d,\"enabled\":%s,\"r\":%d,\"g\":%d,\"b\":%d,\"hex\":\"#%02X%02X%02X\"}",
      i,
      ledState.backlight.colors[i].enabled ? "true" : "false",
      ledState.backlight.colors[i].color.r,
      ledState.backlight.colors[i].color.g,
      ledState.backlight.colors[i].color.b,
      ledState.backlight.colors[i].color.r,
      ledState.backlight.colors[i].color.g,
      ledState.backlight.colors[i].color.b);
  }
  response->printf("],\"activeIndex\":%d}", ledState.backlight.activeColorIndex);
  
  request->send(response);
  
}

void handleSetBacklightMode(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Тело может прийти несколькими чанками — обрабатываем только полностью полученное
  if (index + len != total) return;

  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (!error && doc.containsKey("mode")) {
    uint8_t mode = doc["mode"];
    if (mode < TOTAL_BACKLIGHT_MODES) {
      ledState.backlight.currentMode = mode;
      LOG_PRINTF("API: Backlight mode set to %d\n", mode);
      settingsChanged = true;
      request->send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleSetBacklightModeSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Wait for complete data
  if (index + len != total) return;
  
  
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (error) {
    LOG_PRINTF("API: JSON parse error: %s\n", error.c_str());
    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  if (!doc.containsKey("modeId")) {
    LOG_PRINTLN(F("API: Missing modeId"));
    request->send(400, "application/json", "{\"error\":\"Missing modeId\"}");
    return;
  }
  
  int modeId = doc["modeId"];
  
  if (modeId < 0 || modeId >= TOTAL_BACKLIGHT_MODES) {
    request->send(400, "application/json", "{\"error\":\"Invalid mode ID\"}");
    return;
  }
  
  if (doc.containsKey("speed")) {
    ledState.backlight.modeSettings[modeId].speed = doc["speed"];
  }
  if (doc.containsKey("intensity")) {
    ledState.backlight.modeSettings[modeId].intensity = doc["intensity"];
  }
  if (doc.containsKey("brightness")) {
    ledState.backlight.modeSettings[modeId].brightness = doc["brightness"];
  }
  if (doc.containsKey("archived")) {
    ledState.backlight.modeSettings[modeId].archived = doc["archived"];
  }
  
  LOG_PRINTF("API: Backlight mode %d settings updated\n", modeId);
  settingsChanged = true;
  request->send(200, "application/json", "{\"success\":true}");
}

void handleSetBacklightActiveColor(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Тело может прийти несколькими чанками — обрабатываем только полностью полученное
  if (index + len != total) return;

  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (!error && doc.containsKey("index")) {
    uint8_t idx = doc["index"];
    if (idx < MAX_BACKLIGHT_COLORS && ledState.backlight.colors[idx].enabled) {
      ledState.backlight.activeColorIndex = idx;
      LOG_PRINTF("API: Backlight active color set to %d\n", idx);
      settingsChanged = true;
      request->send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleSetBacklightColor(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Тело может прийти несколькими чанками — обрабатываем только полностью полученное
  if (index + len != total) return;

  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (error) {
    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  // Добавление нового цвета (найти первый свободный слот)
  if (doc.containsKey("r") && doc.containsKey("g") && doc.containsKey("b")) {
    uint8_t r = doc["r"];
    uint8_t g = doc["g"];
    uint8_t b = doc["b"];
    
    // Если указан index - обновляем существующий
    if (doc.containsKey("index")) {
      int idx = doc["index"];
      if (idx >= 0 && idx < MAX_BACKLIGHT_COLORS) {
        ledState.backlight.colors[idx].color = CRGB(r, g, b);
        ledState.backlight.colors[idx].enabled = true;
        LOG_PRINTF("API: Backlight color %d updated to #%02X%02X%02X\n", idx, r, g, b);
        settingsChanged = true;
        request->send(200, "application/json", "{\"success\":true}");
        return;
      }
    } else {
      // Найти первый свободный слот
      for (int i = 0; i < MAX_BACKLIGHT_COLORS; i++) {
        if (!ledState.backlight.colors[i].enabled) {
          ledState.backlight.colors[i].color = CRGB(r, g, b);
          ledState.backlight.colors[i].enabled = true;
          LOG_PRINTF("API: Backlight color added at %d: #%02X%02X%02X\n", i, r, g, b);
          settingsChanged = true;
          
          StaticJsonDocument<64> resp;
          resp["success"] = true;
          resp["index"] = i;
          String response;
          serializeJson(resp, response);
          request->send(200, "application/json", response);
          return;
        }
      }
      request->send(400, "application/json", "{\"error\":\"No free color slots\"}");
      return;
    }
  }
  
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleDeleteBacklightColor(AsyncWebServerRequest *request) {
  
  if (request->hasParam("index")) {
    int idx = request->getParam("index")->value().toInt();
    
    if (idx >= 0 && idx < MAX_BACKLIGHT_COLORS) {
      // Не удаляем последний активный цвет
      int activeCount = 0;
      for (int i = 0; i < MAX_BACKLIGHT_COLORS; i++) {
        if (ledState.backlight.colors[i].enabled) activeCount++;
      }
      
      if (activeCount <= 1) {
        request->send(400, "application/json", "{\"error\":\"Cannot delete last color\"}");
        return;
      }
      
      ledState.backlight.colors[idx].enabled = false;
      
      // Если удалили активный цвет - переключаемся на первый доступный
      if (ledState.backlight.activeColorIndex == idx) {
        for (int i = 0; i < MAX_BACKLIGHT_COLORS; i++) {
          if (ledState.backlight.colors[i].enabled) {
            ledState.backlight.activeColorIndex = i;
            break;
          }
        }
      }
      
      LOG_PRINTF("API: Backlight color %d deleted\n", idx);
      settingsChanged = true;
      request->send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  request->send(400, "application/json", "{\"error\":\"Missing index parameter\"}");
}

void handleSetBacklightAutoSwitch(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Тело может прийти несколькими чанками — обрабатываем только полностью полученное
  if (index + len != total) return;

  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (error) {
    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  // Настройки авто-переключения режимов
  if (doc.containsKey("modeDelay")) {
    ledState.backlight.modeAutoSwitchDelay = doc["modeDelay"];
  }
  if (doc.containsKey("modeRandom")) {
    ledState.backlight.modeRandomOrder = doc["modeRandom"];
  }
  
  // Настройки авто-переключения цветов
  if (doc.containsKey("colorDelay")) {
    ledState.backlight.colorAutoSwitchDelay = doc["colorDelay"];
  }
  if (doc.containsKey("colorRandom")) {
    ledState.backlight.colorRandomOrder = doc["colorRandom"];
  }
  
  LOG_PRINTF("API: Backlight auto-switch updated: modeDelay=%d, colorDelay=%d\n", 
             ledState.backlight.modeAutoSwitchDelay, 
             ledState.backlight.colorAutoSwitchDelay);
  settingsChanged = true;
  request->send(200, "application/json", "{\"success\":true}");
}
