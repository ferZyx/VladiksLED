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

AsyncWebServer server(WEB_SERVER_PORT);
AsyncWebSocket ws(LOG_WEBSOCKET_PATH);

// Throttle защита
unsigned long lastRequestTime = 0;

bool checkThrottle() {
  unsigned long now = millis();
  if (now - lastRequestTime < MIN_REQUEST_INTERVAL) {
    return false;  // Слишком частые запросы
  }
  lastRequestTime = now;
  return true;
}

// WebSocket event handler
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    LOG_PRINTF("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    
    // Отправляем историю логов новому клиенту
    String history = logger.getBufferAsJson();
    client->text("{\"type\":\"history\",\"logs\":" + history + "}");
    
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
    request->send_P(200, "text/html", WEBPAGE);
  });
  
  // API endpoints - GET requests
  server.on("/api/state", HTTP_GET, handleGetState);
  server.on("/api/mode/settings/get", HTTP_GET, handleGetModeSettings);
  server.on("/api/schedules", HTTP_GET, handleGetSchedules);
  server.on("/api/time", HTTP_GET, handleGetTime);
  server.on("/api/debug", HTTP_GET, handleGetDebug);
  
  // API endpoints - POST requests with body
  // Note: The first lambda is called when request completes (after body), 
  // the body handler is the last parameter
  // IMPORTANT: Register more specific routes FIRST (e.g., /api/mode/settings before /api/mode)
  server.on("/api/power", HTTP_POST, 
    [](AsyncWebServerRequest *request){ LOG_PRINTLN("POST /api/power complete"); }, 
    NULL, handleSetPower);
  server.on("/api/brightness", HTTP_POST, 
    [](AsyncWebServerRequest *request){ LOG_PRINTLN("POST /api/brightness complete"); }, 
    NULL, handleSetBrightness);
  server.on("/api/leds", HTTP_POST, 
    [](AsyncWebServerRequest *request){ LOG_PRINTLN("POST /api/leds complete"); }, 
    NULL, handleSetLEDs);
  // Mode-related routes - more specific first!
  server.on("/api/mode/settings", HTTP_POST, 
    [](AsyncWebServerRequest *request){ LOG_PRINTLN("POST /api/mode/settings complete"); }, 
    NULL, handleSetModeSettings);
  server.on("/api/mode/reset", HTTP_POST, 
    [](AsyncWebServerRequest *request){ LOG_PRINTLN("POST /api/mode/reset complete"); }, 
    NULL, handleResetModeSettings);
  server.on("/api/mode/archive", HTTP_POST, 
    [](AsyncWebServerRequest *request){ LOG_PRINTLN("POST /api/mode/archive complete"); }, 
    NULL, handleToggleModeArchive);
  server.on("/api/mode", HTTP_POST, 
    [](AsyncWebServerRequest *request){ LOG_PRINTLN("POST /api/mode complete"); }, 
    NULL, handleSetMode);
  // Other routes
  server.on("/api/auto-switch", HTTP_POST, 
    [](AsyncWebServerRequest *request){ LOG_PRINTLN("POST /api/auto-switch complete"); }, 
    NULL, handleSetAutoSwitch);
  server.on("/api/schedules", HTTP_POST, 
    [](AsyncWebServerRequest *request){ LOG_PRINTLN("POST /api/schedules complete"); }, 
    NULL, handleSetSchedule);
  server.on("/api/time/set", HTTP_POST, 
    [](AsyncWebServerRequest *request){ LOG_PRINTLN("POST /api/time/set complete"); }, 
    NULL, handleSetTime);
  
  // DELETE request
  server.on("/api/schedules", HTTP_DELETE, handleDeleteSchedule);
  
  server.onNotFound([](AsyncWebServerRequest *request){
    LOG_PRINTF(">>> 404 NOT FOUND: %s %s\n", request->methodToString(), request->url().c_str());
    request->send(404, "text/plain", "Not Found");
  });
  
  server.begin();
  LOG_PRINTLN("HTTP server started");
}

void handleWebServer() {
  // AsyncWebServer обрабатывает запросы автоматически
  // Нужно только очищать WebSocket клиентов
  ws.cleanupClients();
}

void handleGetState(AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(8192);
  
  doc["power"] = ledState.power;
  doc["brightness"] = ledState.brightness;
  doc["numLeds"] = ledState.numLeds;
  doc["currentMode"] = ledState.currentMode;
  doc["autoSwitchDelay"] = ledState.autoSwitchDelay;
  doc["randomOrder"] = ledState.randomOrder;
  
  // Добавляем настройки всех режимов
  JsonArray modes = doc.createNestedArray("modeSettings");
  for (int i = 0; i < TOTAL_MODES; i++) {
    JsonObject mode = modes.createNestedObject();
    mode["speed"] = ledState.modeSettings[i].speed;
    mode["scale"] = ledState.modeSettings[i].scale;
    mode["brightness"] = ledState.modeSettings[i].brightness;
    mode["archived"] = ledState.modeSettings[i].archived;
  }
  
  String response;
  serializeJson(doc, response);
  
  request->send(200, "application/json", response);
}

void handleSetPower(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (!checkThrottle()) {
    request->send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
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
  if (!checkThrottle()) {
    request->send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
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
  if (!checkThrottle()) {
    request->send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
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
  if (!checkThrottle()) {
    request->send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
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
  // FIRST: Log that request arrived (before ANY checks)
  LOG_PRINTF(">>> handleSetModeSettings CALLED: len=%d, index=%d, total=%d\n", len, index, total);
  
  // Handle chunked body - only process when we have complete data
  if (index + len != total) {
    LOG_PRINTF("API: SetModeSettings - Chunked data, waiting for more (got %d/%d)\n", index + len, total);
    return;  // Wait for more data
  }
  
  if (!checkThrottle()) {
    LOG_PRINTLN("API: SetModeSettings - THROTTLED");
    request->send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  // Log raw request body for debugging
  char rawBody[128];
  size_t copyLen = len < 127 ? len : 127;
  memcpy(rawBody, data, copyLen);
  rawBody[copyLen] = '\0';
  LOG_PRINTF("API: SetModeSettings - Raw body: %s\n", rawBody);
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (error) {
    LOG_PRINTF("API: SetModeSettings - JSON parse error: %s\n", error.c_str());
    request->send(400, "application/json", "{\"error\":\"JSON parse error\"}");
    return;
  }
  
  if (!doc.containsKey("modeId")) {
    LOG_PRINTLN("API: SetModeSettings - Missing modeId field");
    request->send(400, "application/json", "{\"error\":\"Missing modeId\"}");
    return;
  }
  
  int modeId = doc["modeId"];
  LOG_PRINTF("API: SetModeSettings - Mode %d, Heap: %u\n", modeId, ESP.getFreeHeap());
  
  if (modeId < 0 || modeId >= TOTAL_MODES) {
    LOG_PRINTF("API: SetModeSettings - Invalid modeId: %d (max: %d)\n", modeId, TOTAL_MODES - 1);
    request->send(400, "application/json", "{\"error\":\"Invalid mode ID\"}");
    return;
  }
  
  // Log current values before update
  LOG_PRINTF("API: Mode %d BEFORE: speed=%d, scale=%d, brightness=%d\n", 
             modeId, 
             ledState.modeSettings[modeId].speed,
             ledState.modeSettings[modeId].scale,
             ledState.modeSettings[modeId].brightness);
  
  if (doc.containsKey("speed")) {
    ledState.modeSettings[modeId].speed = doc["speed"];
  }
  if (doc.containsKey("scale")) {
    ledState.modeSettings[modeId].scale = doc["scale"];
  }
  if (doc.containsKey("brightness")) {
    ledState.modeSettings[modeId].brightness = doc["brightness"];
  }
  
  // Log values after update
  LOG_PRINTF("API: Mode %d AFTER: speed=%d, scale=%d, brightness=%d\n", 
             modeId, 
             ledState.modeSettings[modeId].speed,
             ledState.modeSettings[modeId].scale,
             ledState.modeSettings[modeId].brightness);
  
  settingsChanged = true;
  LOG_PRINTF("API: SetModeSettings - Mode %d updated successfully, settingsChanged=true\n", modeId);
  request->send(200, "application/json", "{\"success\":true}");
}

void handleGetModeSettings(AsyncWebServerRequest *request) {
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
  LOG_PRINTF(">>> handleResetModeSettings CALLED: len=%d, index=%d, total=%d\n", len, index, total);
  
  if (index + len != total) {
    return;  // Wait for complete data
  }
  
  if (!checkThrottle()) {
    LOG_PRINTLN("API: ResetModeSettings - THROTTLED");
    request->send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
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
  LOG_PRINTF(">>> handleToggleModeArchive CALLED: len=%d, index=%d, total=%d\n", len, index, total);
  
  if (index + len != total) {
    return;  // Wait for complete data
  }
  
  if (!checkThrottle()) {
    LOG_PRINTLN("API: ToggleModeArchive - THROTTLED");
    request->send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  // Log raw body
  char rawBody[64];
  size_t copyLen = len < 63 ? len : 63;
  memcpy(rawBody, data, copyLen);
  rawBody[copyLen] = '\0';
  LOG_PRINTF("API: ToggleModeArchive - Raw body: %s\n", rawBody);
  
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
  if (!checkThrottle()) {
    request->send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
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
  if (!checkThrottle()) {
    request->send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
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
  if (!checkThrottle()) {
    request->send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
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
  if (!checkThrottle()) {
    request->send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, (const char*)data, len);
  
  if (!error && doc.containsKey("timestamp")) {
    time_t timestamp = doc["timestamp"];
    
    // Устанавливаем время на ESP8266
    timeval tv = { timestamp, 0 };
    settimeofday(&tv, nullptr);
    
    LOG_PRINT("⏰ Time set manually to: ");
    LOG_PRINTLN(String(timestamp));
    
    request->send(200, "application/json", "{\"success\":true}");
    return;
  }
  
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleGetDebug(AsyncWebServerRequest *request) {
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
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}
