#include "logger.h"
#include <time.h>

// Глобальный экземпляр
Logger logger;

/**
 * Создаёт пустой логгер без подключённого WebSocket.
 */
Logger::Logger() : writeIndex(0), count(0), ws(nullptr) {
  memset(buffer, 0, sizeof(buffer));
}

/**
 * Подключает WebSocket, в который будут транслироваться новые сообщения.
 * @param websocket экземпляр AsyncWebSocket (/ws/logs)
 */
void Logger::setWebSocket(AsyncWebSocket* websocket) {
  ws = websocket;
}

/**
 * Пишет текущее локальное время в формате HH:MM:SS.
 * @param out  буфер назначения
 * @param size размер буфера
 */
void Logger::formatTimestamp(char* out, size_t size) {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  strftime(out, size, "%H:%M:%S", &timeinfo);
}

/**
 * Дописывает в строку JSON-экранированный текст.
 * @param dst строка-приёмник
 * @param src исходный текст
 */
static void appendJsonEscaped(String& dst, const char* src) {
  for (const char* p = src; *p; p++) {
    char c = *p;
    switch (c) {
      case '\\': dst += "\\\\"; break;
      case '"':  dst += "\\\""; break;
      case '\n': dst += "\\n"; break;
      case '\r': dst += "\\r"; break;
      case '\t': dst += "\\t"; break;
      default:
        // Прочие управляющие символы отбрасываем, чтобы не сломать JSON
        if ((uint8_t)c >= 0x20) dst += c;
    }
  }
}

/**
 * Обрезает строку по границе UTF-8 символа, чтобы не оставить «половинку» кириллицы/эмодзи.
 * @param s строка с завершающим нулём, обрезается на месте
 */
static void trimUtf8Tail(char* s) {
  size_t len = strlen(s);
  if (len == 0) return;
  // Идём назад по байтам-продолжениям (10xxxxxx) до начала последнего символа
  size_t i = len;
  while (i > 0 && ((uint8_t)s[i - 1] & 0xC0) == 0x80) i--;
  if (i == 0) { s[0] = '\0'; return; }
  uint8_t lead = (uint8_t)s[i - 1];
  size_t need = (lead >= 0xF0) ? 4 : (lead >= 0xE0) ? 3 : (lead >= 0xC0) ? 2 : 1;
  if (len - (i - 1) < need) s[i - 1] = '\0';
}

/**
 * Сохраняет сообщение в кольцевой буфер и отправляет его WebSocket-клиентам.
 * @param message текст сообщения
 */
void Logger::log(const char* message) {
  LogEntry& entry = buffer[writeIndex];
  formatTimestamp(entry.timestamp, sizeof(entry.timestamp));
  strncpy(entry.message, message, sizeof(entry.message) - 1);
  entry.message[sizeof(entry.message) - 1] = '\0';
  trimUtf8Tail(entry.message);

  writeIndex = (writeIndex + 1) % LOG_RING_SIZE;
  if (count < LOG_RING_SIZE) count++;

  // Трансляция в WebSocket. Если очередь клиента переполнена (медленный браузер),
  // сообщение пропускаем — иначе AsyncWebSocket копит буферы и съедает кучу.
  if (ws != nullptr && ws->count() > 0 && ws->availableForWriteAll()) {
    String json;
    json.reserve(strlen(entry.message) + 48);
    json += "{\"timestamp\":\"";
    json += entry.timestamp;
    json += "\",\"message\":\"";
    appendJsonEscaped(json, entry.message);
    json += "\"}";
    ws->textAll(json);
  }
}

/**
 * Перегрузка для String.
 * @param message текст сообщения
 */
void Logger::log(const String& message) {
  log(message.c_str());
}

/**
 * Собирает JSON-сообщение с последними LOG_HISTORY_SEND_COUNT записями
 * для нового WebSocket-клиента: {"type":"history","logs":[...]}.
 * @return готовая JSON-строка
 */
String Logger::getHistoryJson() {
  int sendCount = count < LOG_HISTORY_SEND_COUNT ? count : LOG_HISTORY_SEND_COUNT;
  // Индекс самой старой из отправляемых записей
  int start = (writeIndex - sendCount + LOG_RING_SIZE) % LOG_RING_SIZE;

  String json;
  json.reserve(sendCount * 64 + 32);
  json += "{\"type\":\"history\",\"logs\":[";
  for (int i = 0; i < sendCount; i++) {
    const LogEntry& e = buffer[(start + i) % LOG_RING_SIZE];
    if (i > 0) json += ",";
    json += "{\"timestamp\":\"";
    json += e.timestamp;
    json += "\",\"message\":\"";
    appendJsonEscaped(json, e.message);
    json += "\"}";
  }
  json += "]}";
  return json;
}
