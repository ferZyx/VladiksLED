#ifndef CONFIG_H
#define CONFIG_H

// WiFi настройки
#define WIFI_SSID "TP-LINK_8E7C38"        // Замените на имя вашей WiFi сети
#define WIFI_PASSWORD "877746046333" // Замените на пароль от WiFi

// Статический IP (закомментируйте эти строки для использования DHCP)
#define USE_STATIC_IP true
#define STATIC_IP 192, 168, 1, 222
#define GATEWAY_IP 192, 168, 1, 1
#define SUBNET_MASK 255, 255, 255, 0

// OTA настройки
#define OTA_HOSTNAME "VladiksLED"

// LED настройки
#define LED_PIN 2           // Пин подключения ленты (D4 на Wemos D1 Mini = GPIO2)
#define MAX_LEDS 300        // Максимальное количество LED
#define DEFAULT_LEDS 50     // Количество LED по умолчанию
#define DEFAULT_BRIGHTNESS 128  // Яркость по умолчанию (0-255)

// Режимы работы
#define TOTAL_MODES 10      // Общее количество режимов

// Web Server
#define WEB_SERVER_PORT 80

// Debounce/Throttle настройки
#define MIN_REQUEST_INTERVAL 100  // Минимальный интервал между запросами (мс)

// NTP настройки
#define NTP_SERVER "time.google.com"  // Более надежный NTP сервер
#define NTP_OFFSET 18000          // UTC+5 (Казахстан/Екатеринбург) в секундах
#define NTP_UPDATE_INTERVAL 3600000  // Обновление каждый час (мс)

// Расписание
#define MAX_SCHEDULES 10          // Максимальное количество расписаний

// Логирование
#define LOG_BUFFER_SIZE 50        // Размер кольцевого буфера логов
#define LOG_ENABLE_TIMESTAMPS true // Включить временные метки
#define LOG_WEBSOCKET_PATH "/ws/logs" // WebSocket endpoint для логов

#endif
