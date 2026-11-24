# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Базовые правила проекта (команды, конвенции, чек-лист режимов) лежат в AGENTS.md и подключаются отсюда — не дублируйте их здесь:

@AGENTS.md

## ОБЯЗАТЕЛЬНО перед завершением любой сессии

Если в сессии менялись файлы, агент обязан до финального ответа:

1. **Обновить память Serena** (`.serena/memories/`) — отразить изменения в коде, архитектуре, командах и конвенциях; удалить/исправить устаревшее.
2. **Закоммитить** изменения кода вместе с обновлённой памятью. Перед коммитом проверить `git status`/`git diff`: не включать правки `src/config.h`, которые меняют только WiFi-креды/сетевые настройки, и временные файлы из `temp/`.
3. **Запушить** коммит в текущую ветку (`git push`).

Если коммит или пуш не удался (конфликт, нет доступа к remote и т.п.) — не форсить, а явно сообщить пользователю, что осталось незакоммиченным/незапушенным.

Ниже — то, чего нет в AGENTS.md и что требует чтения нескольких файлов.

## Сборка и прошивка

- Тестов, линтера и тайпчека нет. Единственная автоматическая проверка — `pio run -e d1_mini`.
- В `platformio.ini` стоит `upload_protocol = espota` и `upload_port = 192.168.100.222`, поэтому `pio run -e d1_mini -t upload` шьёт **по воздуху (OTA)**, а не по USB. Для первой прошивки по USB нужно временно переопределить протокол/порт.
- IP в `.github/copilot-instructions.md` (`192.168.1.222`) устарел — актуальные значения в `platformio.ini` и `src/config.h`.

## Архитектура

- **Одно глобальное состояние.** Всё настраиваемое живёт в `LEDState ledState` (`src/led_state.h`): питание, яркость, гирлянда (`modeSettings[TOTAL_MODES]`), расписания, `operationMode` (`MODE_GARLAND` / `MODE_BACKLIGHT`) и вложенный `BacklightSettings`. HTTP-хендлеры меняют `ledState` и выставляют `settingsChanged`, `loop()` сохраняет.
- **Главный цикл (`src/main.cpp` `loop()`):** OTA → `handleWebServer()` → раз в секунду `checkSchedules()` → каждые 20 мс кадр: `runMode()` или `runBacklightMode()` в зависимости от `operationMode`, затем единственный `FastLED.show()` → авто-переключение режимов гирлянды / режимов и цветов подсветки (по `millis()`, без записи в EEPROM) → отложенный `saveLEDState()`. Каждая стадия обёрнута в `diag.taskStart/taskEnd` (`src/diagnostics.*`), предупреждения о медленных стадиях пишутся в лог. `GET /api/debug` отдаёт состояние времени/NTP и WiFi, а не тайминги.
- **Два независимых семейства эффектов** в `src/led_modes.cpp`: режимы гирлянды (используют `ModeSettings`: speed/scale/color1/color2) и режимы подсветки (используют `BacklightModeSettings` + палитру `backlight.colors[]` с плавными переходами между цветами, длительность зависит от RGB-расстояния — `BACKLIGHT_TRANSITION_*`).
- **Веб-сервер** — `ESPAsyncWebServer` (асинхронный, хендлеры выполняются вне `loop()`): REST под `/api/*` + WebSocket `/ws/logs`, в который `LOG_PRINT/LOG_PRINTLN` (`src/logger.*`) транслируют кольцевой буфер логов. UI — одна PROGMEM-строка в `src/webpage.h` (Tailwind с CDN, клиенту нужен интернет).
- **Время:** NTP (`NTP_SERVER`, `NTP_OFFSET`) → HTTP-фолбэк `syncTimeViaHTTP()` → установка из браузера через `POST /api/time/set`. От времени зависят расписания и (при `WAIT_FOR_TIME_SYNC true`) старт эффектов.

## EEPROM: важный нюанс

`saveLEDState()`/`loadLEDState()` пишут/читают `LEDState` **целиком сырой структурой** (`EEPROM.put/get`) после `EEPROMHeader` (magic `"LEDV"` + `EEPROM_VERSION`), область 2048 байт.

- Любое изменение раскладки `LEDState` — включая изменение `TOTAL_MODES`, `TOTAL_BACKLIGHT_MODES`, `MAX_BACKLIGHT_COLORS` или добавление полей — сдвигает смещения всех следующих полей. Без повышения `EEPROM_VERSION` и ветки миграции в `loadLEDState()` после прошивки на устройстве окажутся мусорные расписания/настройки подсветки (и новые режимы могут оказаться `archived`).
- Неизвестная версия заголовка → сброс к дефолтам (`initLEDState()`), то есть потеря пользовательских настроек.
- Следите, чтобы `sizeof(EEPROMHeader) + sizeof(LEDState)` укладывался в 2048.

## Прочее

- `src/schedule_handlers_temp.txt` — старый черновик под синхронный `ESP8266WebServer` (`server.send`), не компилируется; ориентироваться на `src/webserver.cpp`.
- `src/config.h` содержит реальные WiFi-креды — не копировать их значения в ответы, коммиты и документацию.
