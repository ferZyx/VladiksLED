#include "logger.h"
#include <time.h>

// Глобальный экземпляр
Logger logger;

Logger::Logger() : writeIndex(0), count(0), ws(nullptr) {
  // Инициализация буфера
  for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
    buffer[i].timestamp = "";
    buffer[i].message = "";
  }
}

void Logger::setWebSocket(AsyncWebSocket* websocket) {
  ws = websocket;
}

String Logger::getTimestamp() {
  #if LOG_ENABLE_TIMESTAMPS
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &timeinfo);
    return String(timestamp);
  #else
    return String(millis());
  #endif
}

void Logger::log(const String& message) {
  // Сохраняем в буфер
  buffer[writeIndex].timestamp = getTimestamp();
  buffer[writeIndex].message = message;
  
  writeIndex = (writeIndex + 1) % LOG_BUFFER_SIZE;
  if (count < LOG_BUFFER_SIZE) {
    count++;
  }
  
  // Отправляем через WebSocket
  broadcast(message);
}

void Logger::log(const char* message) {
  log(String(message));
}

void Logger::broadcast(const String& message) {
  if (ws != nullptr && ws->count() > 0) {
    // Экранируем специальные символы в сообщении для JSON
    String escapedMsg = message;
    escapedMsg.replace("\\", "\\\\");
    escapedMsg.replace("\"", "\\\"");
    escapedMsg.replace("\n", "\\n");
    escapedMsg.replace("\r", "\\r");
    escapedMsg.replace("\t", "\\t");
    
    // Создаем JSON объект для отправки
    String timestamp = getTimestamp();
    String json = "{\"timestamp\":\"" + timestamp + "\",\"message\":\"" + escapedMsg + "\"}";
    ws->textAll(json);
  }
}

String Logger::getBufferAsJson() {
  String json = "[";
  
  // Определяем начальный индекс для чтения
  int startIndex = (count < LOG_BUFFER_SIZE) ? 0 : writeIndex;
  
  for (int i = 0; i < count; i++) {
    int index = (startIndex + i) % LOG_BUFFER_SIZE;
    
    if (i > 0) json += ",";
    
    // Экранируем специальные символы в сообщении
    String escapedMsg = buffer[index].message;
    escapedMsg.replace("\\", "\\\\");
    escapedMsg.replace("\"", "\\\"");
    escapedMsg.replace("\n", "\\n");
    escapedMsg.replace("\r", "\\r");
    
    json += "{\"timestamp\":\"" + buffer[index].timestamp + "\",\"message\":\"" + escapedMsg + "\"}";
  }
  
  json += "]";
  return json;
}
