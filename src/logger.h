#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

// Максимальная длина одного сообщения в кольцевом буфере (длиннее — обрезается).
// Фиксированный размер вместо String: буфер не фрагментирует кучу ESP8266.
#define LOG_MESSAGE_MAX_LEN 80

// Размер кольцевого буфера: не больше 30 записей, даже если LOG_BUFFER_SIZE больше.
// Буфер статический (~2.7 КБ), каждая лишняя запись — минус ~90 байт кучи.
#if LOG_BUFFER_SIZE > 30
#define LOG_RING_SIZE 30
#else
#define LOG_RING_SIZE LOG_BUFFER_SIZE
#endif

// Сколько последних записей отправлять новому WebSocket-клиенту.
// Ограничено, чтобы JSON истории не требовал крупного блока памяти.
#define LOG_HISTORY_SEND_COUNT 25

/**
 * Одна запись лога в кольцевом буфере.
 */
struct LogEntry {
  char timestamp[9];                    // "HH:MM:SS"
  char message[LOG_MESSAGE_MAX_LEN];    // текст сообщения (обрезанный)
};

/**
 * Логгер: пишет в Serial, хранит последние записи в кольцевом буфере
 * и транслирует новые сообщения в WebSocket /ws/logs.
 */
class Logger {
private:
  LogEntry buffer[LOG_RING_SIZE];
  int writeIndex;
  int count;
  AsyncWebSocket* ws;

  void formatTimestamp(char* out, size_t size);

public:
  Logger();
  void setWebSocket(AsyncWebSocket* websocket);
  void log(const char* message);
  void log(const String& message);
  String getHistoryJson();
};

// Глобальный экземпляр логгера
extern Logger logger;

// Макросы для замены Serial.print.
// Строковые литералы передавать как F("..."): на ESP8266 обычный литерал
// копируется в .rodata (DRAM) и отнимает память у кучи, а F() оставляет его во Flash.
#define LOG_PRINT(x) do { String _logStr = String(x); Serial.print(_logStr); logger.log(_logStr); } while(0)
#define LOG_PRINTLN(x) do { String _logStr = String(x); Serial.println(_logStr); _logStr += "\n"; logger.log(_logStr); } while(0)
// fmt обязан быть строковым литералом: PSTR() кладёт его во Flash, snprintf_P читает оттуда.
#define LOG_PRINTF(fmt, ...) do { \
  char _logBuf[160]; \
  snprintf_P(_logBuf, sizeof(_logBuf), PSTR(fmt), ##__VA_ARGS__); \
  Serial.print(_logBuf); \
  logger.log(_logBuf); \
} while(0)

#endif
