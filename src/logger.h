#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

// Структура для хранения одного лог-сообщения
struct LogEntry {
  String timestamp;
  String message;
};

// Класс для управления логами
class Logger {
private:
  LogEntry buffer[LOG_BUFFER_SIZE];
  int writeIndex;
  int count;
  AsyncWebSocket* ws;
  
  String getTimestamp();
  
public:
  Logger();
  void setWebSocket(AsyncWebSocket* websocket);
  void log(const String& message);
  void log(const char* message);
  String getBufferAsJson();
  void broadcast(const String& message);
};

// Глобальный экземпляр логгера
extern Logger logger;

// Макросы для замены Serial.print
#define LOG_PRINT(x) do { Serial.print(x); logger.log(String(x)); } while(0)
#define LOG_PRINTLN(x) do { Serial.println(x); logger.log(String(x) + "\n"); } while(0)
#define LOG_PRINTF(fmt, ...) do { \
  char buf[256]; \
  snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
  Serial.print(buf); \
  logger.log(String(buf)); \
} while(0)

#endif
