#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESP8266WebServer.h>
#include "led_state.h"

extern ESP8266WebServer server;

void setupWebServer();
void handleWebServer();

// API handlers
void handleRoot();
void handleGetState();
void handleSetPower();
void handleSetBrightness();
void handleSetLEDs();
void handleSetMode();
void handleGetModeSettings();
void handleSetModeSettings();
void handleResetModeSettings();
void handleToggleModeArchive();
void handleSetAutoSwitch();
void handleGetSchedules();
void handleSetSchedule();
void handleDeleteSchedule();
void handleGetTime();
void handleSetTime();
void handleGetDebug();
void handleNotFound();

#endif
