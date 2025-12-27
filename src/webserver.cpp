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
  server.on("/api/mode/settings", HTTP_POST, handleSetModeSettings);
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
  StaticJsonDocument<2048> doc;
  
  doc["power"] = ledState.power;
  doc["brightness"] = ledState.brightness;
  doc["numLeds"] = ledState.numLeds;
  doc["currentMode"] = ledState.currentMode;
  doc["autoSwitchDelay"] = ledState.autoSwitchDelay;
  doc["randomOrder"] = ledState.randomOrder;
  
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
      if (doc.containsKey("color1")) {
        JsonObject color1 = doc["color1"];
        ledState.modeSettings[modeId].color1 = CRGB(color1["r"], color1["g"], color1["b"]);
      }
      if (doc.containsKey("color2")) {
        JsonObject color2 = doc["color2"];
        ledState.modeSettings[modeId].color2 = CRGB(color2["r"], color2["g"], color2["b"]);
      }
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
