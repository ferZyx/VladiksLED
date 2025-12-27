#include "webserver.h"
#include "webpage.h"
#include "config.h"
#include <ArduinoJson.h>

ESP8266WebServer server(WEB_SERVER_PORT);

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

void setupWebServer() {
  // Главная страница
  server.on("/", HTTP_GET, handleRoot);
  
  // API endpoints
  server.on("/api/state", HTTP_GET, handleGetState);
  server.on("/api/power", HTTP_POST, handleSetPower);
  server.on("/api/brightness", HTTP_POST, handleSetBrightness);
  server.on("/api/leds", HTTP_POST, handleSetLEDs);
  server.on("/api/mode", HTTP_POST, handleSetMode);
  server.on("/api/mode/settings/get", HTTP_GET, handleGetModeSettings);
  server.on("/api/mode/settings", HTTP_POST, handleSetModeSettings);
  server.on("/api/mode/reset", HTTP_POST, handleResetModeSettings);
  server.on("/api/mode/archive", HTTP_POST, handleToggleModeArchive);
  server.on("/api/auto-switch", HTTP_POST, handleSetAutoSwitch);
  server.on("/api/schedules", HTTP_GET, handleGetSchedules);
  server.on("/api/schedules", HTTP_POST, handleSetSchedule);
  server.on("/api/schedules", HTTP_DELETE, handleDeleteSchedule);
  server.on("/api/time", HTTP_GET, handleGetTime);
  server.on("/api/time/set", HTTP_POST, handleSetTime);
  server.on("/api/debug", HTTP_GET, handleGetDebug);
  
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("HTTP server started");
}

void handleWebServer() {
  server.handleClient();
}

void handleRoot() {
  server.send_P(200, "text/html", WEBPAGE);
}

void handleGetState() {
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
  
  server.send(200, "application/json", response);
}

void handleSetPower() {
  if (!checkThrottle()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (!error) {
      ledState.power = doc["on"];
      saveLEDState();
      server.send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleSetBrightness() {
  if (!checkThrottle()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (!error) {
      ledState.brightness = doc["value"];
      saveLEDState();
      server.send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleSetLEDs() {
  if (!checkThrottle()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (!error) {
      uint16_t count = doc["count"];
      if (count > 0 && count <= MAX_LEDS) {
        ledState.numLeds = count;
        saveLEDState();
        server.send(200, "application/json", "{\"success\":true}");
        return;
      }
    }
  }
  
  server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleSetMode() {
  if (!checkThrottle()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (!error) {
      uint8_t mode = doc["mode"];
      if (mode < TOTAL_MODES) {
        ledState.currentMode = mode;
        saveLEDState();
        server.send(200, "application/json", "{\"success\":true}");
        return;
      }
    }
  }
  
  server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleSetModeSettings() {
  if (!checkThrottle()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  if (server.hasArg("plain")) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (!error && doc.containsKey("modeId")) {
      int modeId = doc["modeId"];
      
      if (modeId < 0 || modeId >= TOTAL_MODES) {
        server.send(400, "application/json", "{\"error\":\"Invalid mode ID\"}");
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
      saveLEDState();
      server.send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleGetModeSettings() {
  if (server.hasArg("modeId")) {
    int modeId = server.arg("modeId").toInt();
    
    if (modeId < 0 || modeId >= TOTAL_MODES) {
      server.send(400, "application/json", "{\"error\":\"Invalid mode ID\"}");
      return;
    }
    
    StaticJsonDocument<256> doc;
    doc["speed"] = ledState.modeSettings[modeId].speed;
    doc["scale"] = ledState.modeSettings[modeId].scale;
    doc["brightness"] = ledState.modeSettings[modeId].brightness;
    doc["archived"] = ledState.modeSettings[modeId].archived;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
    return;
  }
  
  server.send(400, "application/json", "{\"error\":\"Missing modeId parameter\"}");
}

void handleResetModeSettings() {
  if (!checkThrottle()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (!error && doc.containsKey("modeId")) {
      int modeId = doc["modeId"];
      
      if (modeId < 0 || modeId >= TOTAL_MODES) {
        server.send(400, "application/json", "{\"error\":\"Invalid mode ID\"}");
        return;
      }
      
      // Reset to default values
      ledState.modeSettings[modeId].speed = 128;
      ledState.modeSettings[modeId].scale = 128;
      ledState.modeSettings[modeId].brightness = 255;
      // Don't reset archived status or colors
      
      saveLEDState();
      server.send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleToggleModeArchive() {
  if (!checkThrottle()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (!error && doc.containsKey("modeId")) {
      int modeId = doc["modeId"];
      
      if (modeId < 0 || modeId >= TOTAL_MODES) {
        server.send(400, "application/json", "{\"error\":\"Invalid mode ID\"}");
        return;
      }
      
      bool archived = doc["archived"];
      ledState.modeSettings[modeId].archived = archived;
      saveLEDState();
      server.send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleSetAutoSwitch() {
  if (!checkThrottle()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (!error) {
      ledState.autoSwitchDelay = doc["delay"];
      ledState.randomOrder = doc["random"];
      saveLEDState();
      server.send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleGetSchedules() {
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
  server.send(200, "application/json", response);
}

void handleSetSchedule() {
  if (!checkThrottle()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  if (server.hasArg("plain")) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (!error && doc.containsKey("id")) {
      int id = doc["id"];
      
      if (id < 0 || id >= MAX_SCHEDULES) {
        server.send(400, "application/json", "{\"error\":\"Invalid schedule ID\"}");
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
      
      saveLEDState();
      server.send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleDeleteSchedule() {
  if (!checkThrottle()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  if (server.hasArg("id")) {
    int id = server.arg("id").toInt();
    
    if (id < 0 || id >= MAX_SCHEDULES) {
      server.send(400, "application/json", "{\"error\":\"Invalid schedule ID\"}");
      return;
    }
    
    // Отключаем расписание
    ledState.schedules[id].enabled = false;
    saveLEDState();
    
    server.send(200, "application/json", "{\"success\":true}");
    return;
  }
  
  server.send(400, "application/json", "{\"error\":\"Missing id parameter\"}");
}

void handleGetTime() {
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
  server.send(200, "application/json", response);
}

void handleSetTime() {
  if (!checkThrottle()) {
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return;
  }
  
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (!error && doc.containsKey("timestamp")) {
      time_t timestamp = doc["timestamp"];
      
      // Устанавливаем время на ESP8266
      timeval tv = { timestamp, 0 };
      settimeofday(&tv, nullptr);
      
      Serial.print("⏰ Time set manually to: ");
      Serial.println(timestamp);
      
      server.send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  
  server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleGetDebug() {
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
  server.send(200, "application/json", response);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}
