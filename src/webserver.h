#ifndef WEBSERVER_H
#define WEBSERVER_H

// Define HTTP method constants before including ESPAsyncWebServer
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

#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include "led_state.h"

extern AsyncWebServer server;
extern AsyncWebSocket ws;

void setupWebServer();
void handleWebServer();

// API handlers
void handleRoot();
void handleGetState(AsyncWebServerRequest *request);
void handleSetPower(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleSetBrightness(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleSetLEDs(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleSetMode(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleGetModeSettings(AsyncWebServerRequest *request);
void handleSetModeSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleResetModeSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleToggleModeArchive(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleSetAutoSwitch(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleGetSchedules(AsyncWebServerRequest *request);
void handleSetSchedule(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleDeleteSchedule(AsyncWebServerRequest *request);
void handleGetTime(AsyncWebServerRequest *request);
void handleSetTime(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleGetDebug(AsyncWebServerRequest *request);
void handleNotFound();

#endif
