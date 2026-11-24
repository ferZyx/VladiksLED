# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> `AGENTS.md` — симлинк на этот файл (для Codex и других агентов). Редактировать только `CLAUDE.md`.

## ОБЯЗАТЕЛЬНО перед завершением любой сессии

Если в сессии менялись файлы, агент обязан до финального ответа:

1. **Обновить память Serena** (`.serena/memories/`) — отразить изменения в коде, архитектуре, командах и конвенциях; удалить/исправить устаревшее.
2. **Закоммитить** изменения кода вместе с обновлённой памятью. Перед коммитом проверить `git status`/`git diff`: не включать правки `src/config.h`, которые меняют только WiFi-креды/сетевые настройки, и временные файлы из `temp/`.
3. **Запушить** коммит в текущую ветку (`git push`).

Если коммит или пуш не удался (конфликт, нет доступа к remote и т.п.) — не форсить, а явно сообщить пользователю, что осталось незакоммиченным/незапушенным.

## Проект

- Одна PlatformIO-прошивка для ESP8266 (Wemos D1 Mini, env `d1_mini`), Arduino framework, FastLED (WS2812B), ESPAsyncWebServer, ArduinoJson 6.
- Точка входа — `src/main.cpp` (`setup()` + `loop()`).
- HTTP/WebSocket API — `src/webserver.cpp`; веб-UI — одна PROGMEM-строка в `src/webpage.h`.
- Состояние и EEPROM-миграции — `src/led_state.h` + `src/led_state.cpp`.
- Эффекты — `src/led_modes.cpp`, выбор через `runMode()` / `runBacklightMode()`.

## Команды

- Сборка: `pio run -e d1_mini`
- Чистая пересборка: `pio run -e d1_mini -t clean && pio run -e d1_mini`
- Прошивка: `pio run -e d1_mini -t upload` (полный деплой с проверкой — скилл `.claude/skills/deploy/`, скрипт `.claude/skills/deploy/deploy.sh`)
- Serial-монитор: `pio device monitor -b 115200`
- Тестов, линтера и тайпчека нет. Единственная автоматическая проверка — `pio run -e d1_mini`.
- В `platformio.ini` стоит `upload_protocol = espota` и `upload_port = 192.168.100.222`, поэтому `-t upload` шьёт **по воздуху (OTA)**, а не по USB. Для первой прошивки по USB нужно временно переопределить протокол/порт.

## Конвенции

- `src/config.h` — локальный конфиг машины/сети (WiFi-креды, статический IP, OTA). Не коммитить правки, меняющие только креды/сеть, и не копировать значения кредов в ответы, коммиты и документацию.
- Не вызывать `saveLEDState()` в хендлерах запросов и горячих путях — выставлять `settingsChanged = true`, сохранение делает `loop()` в `src/main.cpp`.
- Переходы авто-переключения режимов намеренно не сохраняются в EEPROM (иначе watchdog/крэши).
- В `setupWebServer()` специфичные роуты регистрировать раньше общих (например, `/api/mode/settings` до `/api/mode`).
- В POST body-хендлерах с чанками сохранять guard полного тела (`if (index + len != total) return;`) перед парсингом JSON.
- Хендлеры защищены от спама через `checkThrottle()` (`MIN_REQUEST_INTERVAL`, 100 мс).
- В циклах и хендлерах предпочитать `snprintf` в буфер вместо конкатенации `String` (фрагментация кучи).

## Добавление/удаление режимов

Полная пошаговая процедура — `rules/create-mode.rule.md`. Кратко:

- Режимы гирлянды синхронизировать в четырёх местах: `src/config.h` (`TOTAL_MODES`), `src/led_modes.h` + `src/led_modes.cpp` (`runMode()` switch), `src/main.cpp` (`MODE_NAMES[]`), `src/webpage.h` (`modeNames`). Индексы и порядок должны совпадать.
- Режимы подсветки: `src/config.h` (`TOTAL_BACKLIGHT_MODES`), `src/led_modes.cpp` (`runBacklightMode()`), `src/main.cpp` (`BACKLIGHT_MODE_NAMES[]`), UI/подписи в `src/webpage.h`.
- Индекс режима в UI динамический (`id="currentModeIndex"` стартует с `(-/-)`); не хардкодить количество в HTML/JS.
- Функции эффектов рисуют в буфер `frame[]` (static в `src/led_modes.cpp`), время берут из `animMillis()` / `animTimebase()` / `animSteps()` (общая скорость), а не из `millis()`. `presentFrame()` сглаживает `frame[]` → `leds[]`; `FastLED.show()` вызывается централизованно в `src/main.cpp`.
- Изменение `TOTAL_MODES` меняет раскладку EEPROM — см. раздел ниже.

## Архитектура

- **Одно глобальное состояние.** Всё настраиваемое живёт в `LEDState ledState` (`src/led_state.h`): питание, яркость, гирлянда (`modeSettings[TOTAL_MODES]`), расписания, `operationMode` (`MODE_GARLAND` / `MODE_BACKLIGHT`) и вложенный `BacklightSettings`. HTTP-хендлеры меняют `ledState` и выставляют `settingsChanged`, `loop()` сохраняет.
- **Главный цикл (`src/main.cpp` `loop()`):** OTA → `handleWebServer()` → раз в секунду `checkSchedules()` → каждые 20 мс кадр: `syncLedCount()` → `advanceAnimationClock()` → `runMode()` или `runBacklightMode()` в зависимости от `operationMode` → `presentFrame()` (сглаживание/кроссфейд) → единственный `FastLED.show()` → авто-переключение режимов гирлянды / режимов и цветов подсветки (по `millis()`, без записи в EEPROM) → отложенный `saveLEDState()`. Каждая стадия обёрнута в `diag.taskStart/taskEnd` (`src/diagnostics.*`), предупреждения о медленных стадиях пишутся в лог. `GET /api/debug` отдаёт время/NTP, WiFi и метрики здоровья (куча, фрагментация, тайминги цикла, FPS, сохранения EEPROM).
- **Два независимых семейства эффектов** в `src/led_modes.cpp`: режимы гирлянды (используют `ModeSettings`: speed/scale/color1/color2/brightness/archived; архивные пропускаются автосвитчем) и режимы подсветки (используют `BacklightModeSettings` + палитру `backlight.colors[]` с плавными переходами между цветами, длительность зависит от RGB-расстояния — `BACKLIGHT_TRANSITION_*`).
- **Веб-сервер** — `ESPAsyncWebServer` (асинхронный, хендлеры выполняются вне `loop()`): REST под `/api/*` + WebSocket `/ws/logs`, в который `LOG_PRINT/LOG_PRINTLN` (`src/logger.*`) транслируют кольцевой буфер логов. UI использует Tailwind с CDN (клиенту нужен интернет).
- **Время:** NTP (`NTP_SERVER` + pool.ntp.org + time.cloudflare.com, `NTP_OFFSET`; SNTP сам ретраит в фоне) → HTTP-фолбэк `syncTimeViaHTTP()` (заголовок `Date` ответа `connectivitycheck.gstatic.com/generate_204`, раз в 5 мин пока время невалидно) → установка из браузера через `POST /api/time/set`. От времени зависят расписания и (при `WAIT_FOR_TIME_SYNC true`) старт эффектов. При статическом IP `WiFi.config()` обязательно вызывать с DNS (роутер + 8.8.8.8), иначе имена NTP/HTTP не резолвятся и время не синхронизируется. `setup()` не ждёт NTP: синхронизация идёт в фоне, `loop()` включает HTTP-фолбэк через 15 с после старта.

## EEPROM: важный нюанс

`saveLEDState()`/`loadLEDState()` пишут/читают `LEDState` **целиком сырой структурой** (`EEPROM.put/get`) после `EEPROMHeader` (magic `"LEDV"` + `EEPROM_VERSION`), область 2048 байт.

- Любое изменение раскладки `LEDState` — включая изменение `TOTAL_MODES`, `TOTAL_BACKLIGHT_MODES`, `MAX_BACKLIGHT_COLORS` или добавление полей — сдвигает смещения всех следующих полей. Без повышения `EEPROM_VERSION` и ветки миграции в `loadLEDState()` после прошивки на устройстве окажутся мусорные расписания/настройки подсветки (и новые режимы могут оказаться `archived`).
- Неизвестная версия заголовка → сброс к дефолтам (`initLEDState()`), то есть потеря пользовательских настроек.
- Следите, чтобы `sizeof(EEPROMHeader) + sizeof(LEDState)` укладывался в 2048.

## Прочее

- `src/schedule_handlers_temp.txt` — старый черновик под синхронный `ESP8266WebServer` (`server.send`), не компилируется; ориентироваться на `src/webserver.cpp`.
