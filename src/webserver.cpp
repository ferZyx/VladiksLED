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
  server.on("/api/mode/archive", HTTP_POST, handleToggleModeArchive);
  server.on("/api/auto-switch", HTTP_POST, handleSetAutoSwitch);
  
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

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}
