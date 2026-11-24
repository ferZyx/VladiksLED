#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>

// Встроенный веб-интерфейс. Без внешних зависимостей (CDN), чтобы страница
// открывалась без интернета и не грузила телефон JIT-компиляцией CSS.
const char WEBPAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#14161c">
<title>VladiksLED</title>
<style>
:root{
  --bg:#14161c; --card:#1d2029; --card2:#262a35; --line:#2f3441;
  --text:#eef0f5; --muted:#9aa1b2; --accent:#34d399; --accent2:#10b981;
  --warn:#f59e0b; --danger:#ef4444; --radius:16px;
}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;background:var(--bg);color:var(--text);
  font:15px/1.4 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
body{padding:0 12px calc(24px + env(safe-area-inset-bottom))}
.wrap{max-width:620px;margin:0 auto}
button{font:inherit;color:inherit;border:0;cursor:pointer;background:none}
input,select{font:inherit;color:var(--text)}
.hidden{display:none!important}

/* ===== Шапка ===== */
.top{position:sticky;top:0;z-index:20;background:var(--bg);
  display:flex;align-items:center;gap:10px;padding:12px 0 10px}
.top h1{font-size:19px;margin:0;flex:1;display:flex;align-items:center;gap:8px}
.dot{width:8px;height:8px;border-radius:50%;background:var(--accent)}
.dot.off{background:var(--danger)}
.power{width:52px;height:52px;border-radius:50%;background:var(--card2);
  display:flex;align-items:center;justify-content:center;font-size:24px;
  transition:background .2s,box-shadow .2s}
.power.on{background:var(--accent2);box-shadow:0 0 18px rgba(52,211,153,.5)}
.banner{background:var(--danger);color:#fff;text-align:center;font-size:13px;
  padding:6px;border-radius:10px;margin-bottom:10px}

/* ===== Карточки и элементы ===== */
.card{background:var(--card);border-radius:var(--radius);padding:14px;margin-bottom:12px}
.card h2{font-size:15px;margin:0 0 10px;color:var(--muted);font-weight:600;
  display:flex;align-items:center;justify-content:space-between}
.row{display:flex;align-items:center;gap:10px}
.row+.row{margin-top:12px}
.grow{flex:1}
.muted{color:var(--muted);font-size:13px}
.seg{display:flex;background:var(--card);border-radius:14px;padding:4px;margin-bottom:12px}
.seg button{flex:1;padding:11px 8px;border-radius:11px;font-weight:600;color:var(--muted)}
.seg button.active{background:var(--card2);color:var(--text)}
.seg.small{background:var(--card2);margin:0}
.seg.small button{padding:8px}
.seg.small button.active{background:var(--accent2);color:#fff}
.btn{background:var(--card2);padding:10px 14px;border-radius:12px;font-weight:600}
.btn.primary{background:var(--accent2);color:#fff}
.btn.warn{background:rgba(245,158,11,.18);color:var(--warn)}
.btn.danger{background:rgba(239,68,68,.15);color:var(--danger)}
.btn.small{padding:6px 10px;font-size:13px;border-radius:10px}
.btn.block{width:100%}
.icon-btn{width:44px;height:44px;border-radius:12px;background:var(--card2);
  display:inline-flex;align-items:center;justify-content:center;font-size:18px}

/* Слайдер */
.slider-head{display:flex;justify-content:space-between;margin-bottom:6px}
.slider-head b{font-variant-numeric:tabular-nums}
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:32px;background:transparent;margin:0}
input[type=range]::-webkit-slider-runnable-track{height:8px;border-radius:4px;background:var(--line)}
input[type=range]::-moz-range-track{height:8px;border-radius:4px;background:var(--line)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:26px;height:26px;border-radius:50%;
  background:#fff;margin-top:-9px;box-shadow:0 1px 4px rgba(0,0,0,.4)}
input[type=range]::-moz-range-thumb{width:26px;height:26px;border:0;border-radius:50%;background:#fff}
.field{background:var(--card2);border:1px solid var(--line);border-radius:10px;padding:8px 10px;width:100%}
.field.num{width:90px;text-align:center}
.switch{position:relative;width:46px;height:28px;flex:none}
.switch input{opacity:0;width:0;height:0}
.switch span{position:absolute;inset:0;background:var(--line);border-radius:14px;transition:.2s}
.switch span:before{content:"";position:absolute;width:22px;height:22px;left:3px;top:3px;
  background:#fff;border-radius:50%;transition:.2s}
.switch input:checked+span{background:var(--accent2)}
.switch input:checked+span:before{transform:translateX(18px)}

/* ===== Текущий режим ===== */
.now{display:flex;align-items:center;gap:8px}
.now .name{flex:1;min-width:0;text-align:center}
.now .name b{display:block;font-size:18px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}

/* ===== Сетка режимов ===== */
.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:8px}
@media(min-width:520px){.grid{grid-template-columns:repeat(3,1fr)}}
.mode{position:relative;background:var(--card2);border-radius:12px;padding:12px 40px 12px 12px;
  text-align:left;min-height:52px;border:2px solid transparent;transition:border-color .15s,background .15s}
.mode.active{border-color:var(--accent);background:rgba(52,211,153,.12)}
.mode .gear{position:absolute;right:4px;top:50%;transform:translateY(-50%);
  width:34px;height:34px;border-radius:9px;color:var(--muted);font-size:16px}
details.archive{margin-top:12px}
details.archive summary{color:var(--muted);font-size:14px;cursor:pointer;padding:6px 0;list-style:none}
details.archive summary::-webkit-details-marker{display:none}
details.archive .grid{margin-top:8px;opacity:.75}

/* ===== Палитра ===== */
.palette{display:flex;flex-wrap:wrap;gap:10px}
.sw{position:relative;width:48px;height:48px;border-radius:14px;border:3px solid transparent;
  box-shadow:inset 0 0 0 1px rgba(255,255,255,.12)}
.sw.active{border-color:#fff;box-shadow:0 0 0 2px var(--accent)}
.sw.add{background:var(--card2);color:var(--muted);font-size:24px;box-shadow:none}
.sw .x{position:absolute;top:-7px;right:-7px;width:22px;height:22px;border-radius:50%;
  background:var(--danger);color:#fff;font-size:14px;line-height:22px;text-align:center}
.palette:not(.editing) .sw .x{display:none}
.palette.editing .sw:not(.add){animation:wiggle .3s ease-in-out infinite alternate}
@keyframes wiggle{from{transform:rotate(-2deg)}to{transform:rotate(2deg)}}

/* ===== Нижние «шторки» (модальные окна) ===== */
.sheet-overlay{position:fixed;inset:0;z-index:50;background:rgba(0,0,0,.55);
  display:flex;align-items:flex-end;justify-content:center;opacity:0;pointer-events:none;transition:opacity .2s}
.sheet-overlay.open{opacity:1;pointer-events:auto}
.sheet{background:var(--card);width:100%;max-width:620px;max-height:90vh;overflow-y:auto;
  border-radius:20px 20px 0 0;padding:8px 16px calc(18px + env(safe-area-inset-bottom));
  transform:translateY(30px);transition:transform .2s}
.sheet-overlay.open .sheet{transform:none}
@media(min-width:700px){.sheet-overlay{align-items:center}.sheet{border-radius:20px}}
.grabber{width:40px;height:5px;border-radius:3px;background:var(--line);margin:4px auto 12px}
.sheet h3{margin:0 0 14px;font-size:18px}
.sheet .actions{display:flex;gap:8px;margin-top:16px}
.sheet .actions .btn{flex:1}
input[type=color]{width:100%;height:120px;border:0;border-radius:14px;background:none;padding:0}
.days{display:grid;grid-template-columns:repeat(7,1fr);gap:4px}
.days button{padding:9px 0;border-radius:9px;background:var(--card2);font-size:13px}
.days button.on{background:var(--accent2);color:#fff}
.sched{display:flex;align-items:center;gap:10px;background:var(--card2);border-radius:12px;padding:10px 12px}
.sched+.sched{margin-top:8px}
.sched b{font-size:18px;font-variant-numeric:tabular-nums}
.clock{text-align:center;background:var(--card2);border-radius:14px;padding:10px;margin-bottom:14px}
.clock b{font-size:28px;font-variant-numeric:tabular-nums}

/* ===== Логи ===== */
details.more summary{cursor:pointer;color:var(--muted);font-weight:600;list-style:none}
details.more summary::-webkit-details-marker{display:none}
#logsConsole{background:#0b0c10;border-radius:10px;padding:10px;height:260px;overflow-y:auto;
  font:11px/1.45 ui-monospace,Menlo,monospace;color:#7ee2a8;margin-top:10px;word-break:break-word}
#logsConsole .ts{color:#5b6172}
.toast{position:fixed;left:50%;bottom:calc(24px + env(safe-area-inset-bottom));transform:translateX(-50%);
  background:#000;color:#fff;padding:10px 16px;border-radius:12px;font-size:14px;z-index:60;
  opacity:0;transition:opacity .2s;pointer-events:none}
.toast.show{opacity:.92}
</style>
</head>
<body>
<div class="wrap">

  <!-- Шапка: название, статус связи, питание -->
  <div class="top">
    <h1><span id="statusDot" class="dot"></span>VladiksLED</h1>
    <button id="powerBtn" class="power" onclick="togglePower()" aria-label="Питание">⏻</button>
  </div>
  <div id="connectionBanner" class="banner hidden">Нет связи с гирляндой — повторяю…</div>

  <!-- Режим работы -->
  <div class="seg">
    <button id="opModeGarland" onclick="setOperationMode(0)">🎄 Гирлянда</button>
    <button id="opModeBacklight" onclick="setOperationMode(1)">💡 Подсветка</button>
  </div>

  <!-- Общая яркость -->
  <div class="card">
    <div class="slider-head"><span>☀️ Яркость</span><b id="brightnessValue">50%</b></div>
    <input type="range" id="brightnessSlider" min="1" max="255" value="128" oninput="updateBrightness(this.value)">
  </div>

  <!-- ===== Гирлянда ===== -->
  <div id="garlandPanel">
    <div class="card">
      <div class="now">
        <button class="icon-btn" onclick="stepMode(-1)" aria-label="Предыдущий">◀</button>
        <div class="name">
          <b id="currentModeName">—</b>
          <span class="muted" id="currentModeIndex">(-/-)</span>
        </div>
        <button class="icon-btn" onclick="stepMode(1)" aria-label="Следующий">▶</button>
        <button class="icon-btn" onclick="openModeSettings(currentModeId)" aria-label="Настройки режима">⚙️</button>
      </div>
    </div>

    <div class="card">
      <h2>Режимы</h2>
      <div id="modesGrid" class="grid"></div>
      <details class="archive" id="archiveDetails">
        <summary>📦 Архив (<span id="archiveCount">0</span>) ▾</summary>
        <div id="archivedGrid" class="grid"></div>
      </details>
    </div>

    <div class="card">
      <h2>Авто-переключение</h2>
      <div class="row">
        <span class="grow">Каждые, сек <span class="muted">(0 — выкл)</span></span>
        <input type="number" id="autoSwitchDelay" class="field num" min="0" max="3600" value="0" onchange="saveSettings()">
      </div>
      <div class="row">
        <span class="grow">Случайный порядок</span>
        <label class="switch"><input type="checkbox" id="randomOrder" onchange="saveSettings()"><span></span></label>
      </div>
    </div>
  </div>

  <!-- ===== Подсветка ===== -->
  <div id="backlightPanel" class="hidden">
    <div class="card">
      <h2>Цвета <button id="paletteEditBtn" class="btn small" onclick="togglePaletteEdit()">✎ Изменить</button></h2>
      <div id="colorPalette" class="palette"></div>
      <div class="muted" id="paletteHint" style="margin-top:10px">Нажмите на цвет, чтобы включить его</div>
    </div>

    <div class="card">
      <h2>Анимация</h2>
      <div id="backlightModesGrid" class="grid"></div>
    </div>

    <div class="card">
      <h2>Авто-переключение <span class="muted">сек, 0 — выкл</span></h2>
      <div class="row">
        <span class="grow">Смена анимации</span>
        <input type="number" id="blModeDelay" class="field num" min="0" max="3600" value="0" onchange="updateBacklightAutoSwitch()">
      </div>
      <div class="row">
        <span class="grow">Смена цвета</span>
        <input type="number" id="blColorDelay" class="field num" min="0" max="3600" value="0" onchange="updateBacklightAutoSwitch()">
      </div>
    </div>
  </div>

  <!-- ===== Устройство ===== -->
  <div class="card">
    <h2>Устройство</h2>
    <div class="row">
      <span class="grow">💡 Количество диодов</span>
      <input type="number" id="ledCount" class="field num" min="1" max="300" value="50" onchange="updateLEDCount(this.value)">
    </div>
    <div class="row">
      <button class="btn block" onclick="openSchedule()">📅 Расписание</button>
    </div>
  </div>

  <!-- ===== Логи (WebSocket подключается только когда раздел открыт) ===== -->
  <div class="card">
    <details class="more" id="logsDetails" ontoggle="onLogsToggle()">
      <summary>📋 Логи <span class="muted" id="wsStatusText">выкл</span></summary>
      <div class="row" style="margin-top:10px">
        <button id="pauseLogsBtn" class="btn small" onclick="toggleLogsPause()">⏸ Пауза</button>
        <button class="btn small" onclick="clearLogs()">Очистить</button>
        <button class="btn small" onclick="downloadLogs()">Скачать</button>
      </div>
      <div id="logsConsole"></div>
    </details>
  </div>
</div>

<!-- Шторка: настройки режима гирлянды -->
<div id="modeSettingsModal" class="sheet-overlay" onclick="onOverlayClick(event)">
  <div class="sheet">
    <div class="grabber"></div>
    <h3 id="modeSettingsTitle">Настройки режима</h3>
    <div class="slider-head"><span>Скорость</span><b id="modeSpeedVal">128</b></div>
    <input type="range" id="modeSpeed" min="0" max="255" value="128" oninput="updateModeSettings()">
    <div class="slider-head" style="margin-top:12px"><span>Масштаб</span><b id="modeScaleVal">128</b></div>
    <input type="range" id="modeScale" min="0" max="255" value="128" oninput="updateModeSettings()">
    <div class="slider-head" style="margin-top:12px"><span>Яркость режима</span><b id="modeBrightnessVal">255</b></div>
    <input type="range" id="modeBrightness" min="0" max="255" value="255" oninput="updateModeSettings()">
    <div class="actions">
      <button id="modePlayBtn" class="btn primary" onclick="playEditingMode()">▶ Включить</button>
    </div>
    <div class="actions">
      <button id="archiveButton" class="btn warn" onclick="toggleArchiveFromSettings()">📦 В архив</button>
      <button class="btn" onclick="resetModeSettings()">↺ По умолчанию</button>
    </div>
    <div class="actions"><button class="btn" onclick="closeModeSettings()">Готово</button></div>
  </div>
</div>

<!-- Шторка: настройки анимации подсветки -->
<div id="blModeSettingsModal" class="sheet-overlay" onclick="onOverlayClick(event)">
  <div class="sheet">
    <div class="grabber"></div>
    <h3 id="blModeSettingsTitle">Настройки анимации</h3>
    <div class="slider-head"><span>Скорость</span><b id="blModeSpeedVal">128</b></div>
    <input type="range" id="blModeSpeed" min="0" max="255" value="128" oninput="updateBacklightModeSettings()">
    <div class="slider-head" style="margin-top:12px"><span>Интенсивность</span><b id="blModeIntensityVal">128</b></div>
    <input type="range" id="blModeIntensity" min="0" max="255" value="128" oninput="updateBacklightModeSettings()">
    <div class="slider-head" style="margin-top:12px"><span>Яркость</span><b id="blModeBrightnessVal">255</b></div>
    <input type="range" id="blModeBrightness" min="0" max="255" value="255" oninput="updateBacklightModeSettings()">
    <div class="actions"><button class="btn" onclick="closeBacklightModeSettings()">Готово</button></div>
  </div>
</div>

<!-- Шторка: добавление / редактирование цвета -->
<div id="colorPickerModal" class="sheet-overlay" onclick="onOverlayClick(event)">
  <div class="sheet">
    <div class="grabber"></div>
    <h3 id="colorPickerTitle">Новый цвет</h3>
    <input type="color" id="newColorPicker" value="#ff9966" oninput="previewColorChange()">
    <div class="actions">
      <button id="colorDeleteBtn" class="btn danger" onclick="deleteEditingColor()">Удалить</button>
      <button id="colorSaveBtn" class="btn primary" onclick="saveColor()">Добавить</button>
    </div>
    <div class="actions"><button class="btn" onclick="closeColorPicker()">Отмена</button></div>
  </div>
</div>

<!-- Шторка: расписание -->
<div id="scheduleModal" class="sheet-overlay" onclick="onOverlayClick(event)">
  <div class="sheet">
    <div class="grabber"></div>
    <h3>📅 Расписание</h3>
    <div class="clock">
      <div class="muted">Время на гирлянде</div>
      <b id="currentTime">--:--</b>
      <div class="muted" id="currentDay">—</div>
    </div>

    <div id="scheduleList"></div>

    <h2 style="font-size:15px;color:var(--muted);margin:18px 0 10px">Новое правило</h2>
    <div class="row">
      <input type="time" id="scheduleTime" class="field" value="08:00" style="width:130px">
      <div class="seg small grow">
        <button id="actOn" class="active" onclick="setScheduleAction(true)">Включить</button>
        <button id="actOff" onclick="setScheduleAction(false)">Выключить</button>
      </div>
    </div>
    <div class="days" style="margin-top:12px">
      <button id="day0" onclick="toggleDay(0)">Пн</button><button id="day1" onclick="toggleDay(1)">Вт</button>
      <button id="day2" onclick="toggleDay(2)">Ср</button><button id="day3" onclick="toggleDay(3)">Чт</button>
      <button id="day4" onclick="toggleDay(4)">Пт</button><button id="day5" onclick="toggleDay(5)">Сб</button>
      <button id="day6" onclick="toggleDay(6)">Вс</button>
    </div>
    <div class="actions">
      <button class="btn primary" onclick="addSchedule()">＋ Добавить</button>
      <button class="btn" onclick="closeSchedule()">Закрыть</button>
    </div>
  </div>
</div>

<div id="toast" class="toast"></div>

<script>
// Названия режимов гирлянды (порядок = индексы прошивки, см. MODE_NAMES в main.cpp)
const modeNames = [
    "Смешанные волны",    // Blendwave
    "Радужная пульсация", // Rainbow Beat
    "Две синусоиды",      // Two Sin
    "Конфетти",           // Confetti
    "Огонь",              // Fire
    "Радужный марш",      // Rainbow March
    "Плазма",             // Plasma
    "Шум",                // Noise
    "Жонглирование",      // Juggle
    "Один цвет",          // Solid Color
    "Снегопад",           // Snowfall
    "Северное сияние",    // Aurora
    "Светлячки"           // Fireflies
];

// Названия режимов подсветки
const backlightModeNames = [
    "Волны",              // Waves
    "Пульсация"           // Pulse
];

const $ = (id) => document.getElementById(id);

// ===== Состояние интерфейса =====
let operationMode = 0;            // 0 = гирлянда, 1 = подсветка
let currentModeId = 0;            // активный режим гирлянды на устройстве
let editingModeId = null;         // режим, открытый в шторке настроек
let modeSettingsCache = [];       // настройки режимов гирлянды с устройства
let powerOn = true;
let brightnessDebounceTimer = null;
let modeSettingsDebounceTimer = null;
let blModeSettingsTimer = null;
let colorPreviewTimer = null;
let editingColorIndex = null;     // null — добавление нового цвета
let paletteEditMode = false;
let selectedDays = 0x7F;          // битовая маска дней (бит 0 = Пн)
let scheduleAction = true;
let schedulesCache = [];
let clockTimer = null;
let clockBase = null;             // {hour, minute, second, dayOfWeek, at}
let lastArchivedSignature = '';

let backlightState = {
    currentMode: 0,
    activeColorIndex: 0,
    modeSettings: [],
    colors: [],
    autoSwitch: { modeDelay: 0, modeRandom: false, colorDelay: 0, colorRandom: false }
};
let editingBacklightModeId = null;

// ===== Сетевой слой =====
// ESP8266 тянет лишь несколько одновременных соединений и мало памяти,
// поэтому все запросы идут строго по одному через очередь.
// POST-запросы с одинаковым ключом (endpoint + modeId/index/id) склеиваются:
// если предыдущий ещё не ушёл — отправится только последнее значение.
const REQUEST_TIMEOUT_MS = 6000;
const requestQueue = [];
let requestInFlight = false;
let lastUserActionAt = 0;
let connectionOk = true;

/**
 * Отмечает действие пользователя: на несколько секунд опрос состояния
 * не будет перетирать значения в интерфейсе.
 */
function markUserAction() {
    lastUserActionAt = Date.now();
}

/**
 * Показывает/скрывает индикатор потери связи.
 * @param {boolean} ok - есть ли связь с устройством
 */
function setConnectionStatus(ok) {
    if (ok === connectionOk) return;
    connectionOk = ok;
    $('connectionBanner').classList.toggle('hidden', ok);
    $('statusDot').classList.toggle('off', !ok);
}

/**
 * Ключ склейки POST-запросов: настройки одного объекта заменяют друг друга.
 * @param {string} endpoint
 * @param {object} data
 * @returns {string}
 */
function coalesceKey(endpoint, data) {
    const id = data.modeId ?? data.index ?? data.id ?? '';
    return endpoint + '#' + id;
}

/**
 * Выполняет один HTTP-запрос с таймаутом.
 * @param {{method:string, endpoint:string, data:object|null}} req
 * @returns {Promise<object|null>} JSON ответа или null при ошибке
 */
async function performRequest(req) {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);
    try {
        const options = { method: req.method, signal: controller.signal };
        if (req.data) {
            options.headers = { 'Content-Type': 'application/json' };
            options.body = JSON.stringify(req.data);
        }
        const response = await fetch(req.endpoint, options);
        const json = await response.json().catch(() => null);
        setConnectionStatus(true);
        if (!response.ok) {
            console.warn('API', req.endpoint, response.status, json);
            // 503 = устройству не хватает памяти; это временно, а не ошибка данных
            return response.status === 503 ? null : json;
        }
        return json;
    } catch (error) {
        console.error('API Error:', req.endpoint, error);
        setConnectionStatus(false);
        return null;
    } finally {
        clearTimeout(timer);
    }
}

/**
 * Разбирает очередь: строго один запрос в полёте.
 */
async function drainQueue() {
    if (requestInFlight) return;
    requestInFlight = true;
    while (requestQueue.length > 0) {
        const req = requestQueue.shift();
        const result = await performRequest(req);
        req.resolvers.forEach(resolve => resolve(result));
    }
    requestInFlight = false;
}

/**
 * Ставит запрос в очередь.
 * @param {string} method - GET/POST/DELETE
 * @param {string} endpoint
 * @param {object|null} data - тело для POST
 * @returns {Promise<object|null>}
 */
function apiRequest(method, endpoint, data = null) {
    return new Promise(resolve => {
        if (method === 'POST' && data) {
            const key = coalesceKey(endpoint, data);
            const pending = requestQueue.find(r => r.key === key);
            if (pending) {
                // Ещё не отправлен — просто подменяем данные на свежие
                pending.data = data;
                pending.resolvers.push(resolve);
                return;
            }
            requestQueue.push({ method, endpoint, data, key, resolvers: [resolve] });
        } else {
            requestQueue.push({ method, endpoint, data, key: null, resolvers: [resolve] });
        }
        drainQueue();
    });
}

/**
 * Обёртка: GET без данных, POST с данными.
 * @param {string} endpoint
 * @param {object|null} data
 * @returns {Promise<object|null>}
 */
function apiCall(endpoint, data = null) {
    if (data) markUserAction();
    return apiRequest(data ? 'POST' : 'GET', endpoint, data);
}

// ===== Мелкие помощники UI =====

/**
 * Короткое всплывающее сообщение внизу экрана.
 * @param {string} text
 */
function showToast(text) {
    const t = $('toast');
    t.textContent = text;
    t.classList.add('show');
    clearTimeout(showToast.timer);
    showToast.timer = setTimeout(() => t.classList.remove('show'), 2200);
}

/**
 * Записывает значение в поле, только если пользователь его сейчас не редактирует.
 * @param {HTMLInputElement} el
 * @param {*} value
 */
function setIfIdle(el, value) {
    if (el && document.activeElement !== el) el.value = value;
}

/**
 * Открывает шторку.
 * @param {string} id - id оверлея
 */
function openSheet(id) {
    $(id).classList.add('open');
}

/**
 * Закрывает шторку.
 * @param {string} id - id оверлея
 */
function closeSheet(id) {
    $(id).classList.remove('open');
}

/**
 * Клик по затемнению вокруг шторки закрывает её (через штатную функцию закрытия).
 * @param {MouseEvent} e
 */
function onOverlayClick(e) {
    if (e.target !== e.currentTarget) return;
    const closers = {
        modeSettingsModal: closeModeSettings,
        blModeSettingsModal: closeBacklightModeSettings,
        colorPickerModal: closeColorPicker,
        scheduleModal: closeSchedule
    };
    (closers[e.currentTarget.id] || (() => closeSheet(e.currentTarget.id)))();
}

/**
 * Открыта ли сейчас какая-либо шторка.
 * @returns {boolean}
 */
function isAnyModalOpen() {
    return document.querySelector('.sheet-overlay.open') !== null;
}

// ===== Питание, яркость, режим работы =====

/**
 * Обновляет вид кнопки питания.
 * @param {boolean} isOn
 */
function updatePowerToggleUI(isOn) {
    powerOn = isOn;
    $('powerBtn').classList.toggle('on', isOn);
}

/**
 * Переключает питание (оптимистично, с откатом при ошибке).
 */
async function togglePower() {
    const newState = !powerOn;
    updatePowerToggleUI(newState);
    const result = await apiCall('/api/power', { on: newState });
    if (!result || !result.success) await loadState(false);
}

/**
 * Обновляет только отображение яркости (без запроса).
 * @param {number|string} value - 0..255
 */
function updateBrightnessUI(value) {
    $('brightnessValue').textContent = Math.round((value / 255) * 100) + '%';
    setIfIdle($('brightnessSlider'), value);
}

/**
 * Изменение яркости слайдером: UI сразу, запрос — с задержкой.
 * @param {number|string} value
 */
function updateBrightness(value) {
    markUserAction();
    $('brightnessValue').textContent = Math.round((value / 255) * 100) + '%';
    clearTimeout(brightnessDebounceTimer);
    brightnessDebounceTimer = setTimeout(() => {
        apiCall('/api/brightness', { value: parseInt(value) });
    }, 150);
}

/**
 * Переключает режим работы (гирлянда/подсветка).
 * @param {number} mode - 0 или 1
 */
async function setOperationMode(mode) {
    operationMode = mode;
    updateOperationModeUI();
    const result = await apiCall('/api/operation-mode', { mode: mode });
    if (!result || !result.success) {
        await loadState(false);
    } else if (mode === 1 && backlightState.colors.length === 0) {
        await loadBacklightState();
    }
}

/**
 * Показывает панель текущего режима работы.
 */
function updateOperationModeUI() {
    const garland = operationMode === 0;
    $('opModeGarland').classList.toggle('active', garland);
    $('opModeBacklight').classList.toggle('active', !garland);
    $('garlandPanel').classList.toggle('hidden', !garland);
    $('backlightPanel').classList.toggle('hidden', garland);
}

/**
 * Количество светодиодов (с ограничением диапазона).
 * @param {number|string} value
 */
function updateLEDCount(value) {
    const count = Math.max(1, Math.min(300, parseInt(value) || 1));
    $('ledCount').value = count;
    apiCall('/api/leds', { count: count });
}

// ===== Режимы гирлянды =====

/**
 * Индексы режимов, которые не в архиве.
 * @returns {number[]}
 */
function activeModeIds() {
    return modeNames.map((_, i) => i).filter(i => !(modeSettingsCache[i]?.archived));
}

/**
 * Выбор режима гирлянды.
 * @param {number} modeId
 */
async function selectMode(modeId) {
    currentModeId = modeId;
    updateModeCards();
    const result = await apiCall('/api/mode', { mode: modeId });
    if (!result || !result.success) await loadState(false);
}

/**
 * Следующий/предыдущий режим среди активных.
 * @param {number} dir - +1 или -1
 */
function stepMode(dir) {
    const list = activeModeIds();
    if (list.length === 0) return;
    const pos = list.indexOf(currentModeId);
    const next = pos < 0 ? list[0] : list[(pos + dir + list.length) % list.length];
    selectMode(next);
}

/**
 * Создаёт карточку режима для сетки.
 * @param {string} name
 * @param {number} index
 * @returns {HTMLButtonElement}
 */
function createModeCard(name, index) {
    const card = document.createElement('button');
    card.className = 'mode';
    card.dataset.modeId = index;
    card.innerHTML = `<span>${name}</span><span class="gear" data-gear="1">⚙</span>`;
    return card;
}

/**
 * Обработчик кликов по сетке режимов гирлянды (делегирование событий).
 * @param {MouseEvent} e
 */
function onModesGridClick(e) {
    const card = e.target.closest('[data-mode-id]');
    if (!card) return;
    const id = parseInt(card.dataset.modeId);
    if (e.target.closest('[data-gear]')) openModeSettings(id);
    else selectMode(id);
}

/**
 * Перерисовывает сетки активных и архивных режимов.
 */
function renderModes() {
    const grid = $('modesGrid');
    const archived = $('archivedGrid');
    grid.innerHTML = '';
    archived.innerHTML = '';
    let archivedCount = 0;
    modeNames.forEach((name, index) => {
        const isArchived = modeSettingsCache[index]?.archived || false;
        if (isArchived) archivedCount++;
        (isArchived ? archived : grid).appendChild(createModeCard(name, index));
    });
    $('archiveCount').textContent = archivedCount;
    $('archiveDetails').classList.toggle('hidden', archivedCount === 0);
    updateModeCards();
}

/**
 * Подсвечивает активный режим и обновляет блок «сейчас играет».
 */
function updateModeCards() {
    document.querySelectorAll('#modesGrid .mode, #archivedGrid .mode').forEach(card => {
        card.classList.toggle('active', parseInt(card.dataset.modeId) === currentModeId);
    });
    const isArchived = modeSettingsCache[currentModeId]?.archived || false;
    const group = modeNames.map((_, i) => i)
        .filter(i => (modeSettingsCache[i]?.archived || false) === isArchived);
    $('currentModeName').textContent = modeNames[currentModeId] || '—';
    $('currentModeIndex').textContent = `(${group.indexOf(currentModeId) + 1}/${group.length})` +
        (isArchived ? ' · в архиве' : '');
}

/**
 * Заполняет слайдеры шторки настроек режима и подписи значений.
 * @param {{speed:number, scale:number, brightness:number}} s
 */
function fillModeSliders(s) {
    $('modeSpeed').value = s.speed;
    $('modeScale').value = s.scale;
    $('modeBrightness').value = s.brightness;
    $('modeSpeedVal').textContent = s.speed;
    $('modeScaleVal').textContent = s.scale;
    $('modeBrightnessVal').textContent = s.brightness;
}

/**
 * Обновляет кнопки шторки в зависимости от состояния режима.
 */
function updateModeSheetButtons() {
    const archived = modeSettingsCache[editingModeId]?.archived || false;
    $('archiveButton').textContent = archived ? '✅ Вернуть из архива' : '📦 В архив';
    const playing = editingModeId === currentModeId;
    $('modePlayBtn').textContent = playing ? '✓ Сейчас включён' : '▶ Включить';
    $('modePlayBtn').disabled = playing;
    $('modePlayBtn').style.opacity = playing ? .6 : 1;
}

/**
 * Открывает шторку настроек режима гирлянды.
 * @param {number} modeId
 */
function openModeSettings(modeId) {
    editingModeId = modeId;
    $('modeSettingsTitle').textContent = modeNames[modeId];
    const s = modeSettingsCache[modeId] || { speed: 128, scale: 128, brightness: 255 };
    fillModeSliders(s);
    updateModeSheetButtons();
    openSheet('modeSettingsModal');
}

/**
 * Закрывает шторку настроек режима.
 */
function closeModeSettings() {
    editingModeId = null;
    closeSheet('modeSettingsModal');
}

/**
 * Включает режим, открытый в шторке.
 */
function playEditingMode() {
    if (editingModeId === null) return;
    selectMode(editingModeId);
    updateModeSheetButtons();
}

/**
 * Изменение слайдеров режима: подписи сразу, запрос — с задержкой.
 */
function updateModeSettings() {
    markUserAction();
    const speed = parseInt($('modeSpeed').value);
    const scale = parseInt($('modeScale').value);
    const brightness = parseInt($('modeBrightness').value);
    $('modeSpeedVal').textContent = speed;
    $('modeScaleVal').textContent = scale;
    $('modeBrightnessVal').textContent = brightness;

    // Фиксируем id сразу: к моменту отправки шторку могут закрыть
    const modeId = editingModeId;
    if (modeId === null) return;
    if (modeSettingsCache[modeId]) {
        Object.assign(modeSettingsCache[modeId], { speed, scale, brightness });
    }
    clearTimeout(modeSettingsDebounceTimer);
    modeSettingsDebounceTimer = setTimeout(() => {
        apiCall('/api/mode/settings', { modeId, speed, scale, brightness });
    }, 150);
}

/**
 * Отправляет режим в архив или возвращает из него.
 */
async function toggleArchiveFromSettings() {
    if (editingModeId === null) return;
    const modeId = editingModeId;
    const archived = !(modeSettingsCache[modeId]?.archived || false);
    const result = await apiCall('/api/mode/archive', { modeId, archived });
    if (result && result.success) {
        if (modeSettingsCache[modeId]) modeSettingsCache[modeId].archived = archived;
        lastArchivedSignature = '';
        renderModes();
        updateModeSheetButtons();
        showToast(archived ? 'Режим убран в архив' : 'Режим возвращён');
    } else {
        showToast('Не получилось, попробуйте ещё раз');
    }
}

/**
 * Сбрасывает настройки режима к значениям по умолчанию.
 */
async function resetModeSettings() {
    if (editingModeId === null) return;
    const modeId = editingModeId;
    const result = await apiCall('/api/mode/reset', { modeId });
    if (result && result.success) {
        const defaults = { speed: 128, scale: 128, brightness: 255 };
        if (modeSettingsCache[modeId]) Object.assign(modeSettingsCache[modeId], defaults);
        if (editingModeId === modeId) fillModeSliders(defaults);
        showToast('Настройки сброшены');
    }
}

/**
 * Сохраняет авто-переключение режимов гирлянды.
 */
async function saveSettings() {
    const delay = Math.max(0, parseInt($('autoSwitchDelay').value) || 0);
    $('autoSwitchDelay').value = delay;
    await apiCall('/api/auto-switch', { delay: delay, random: $('randomOrder').checked });
}

// ===== Загрузка состояния и опрос =====

/**
 * Загружает состояние устройства и обновляет UI.
 * @param {boolean} includeBacklight - заодно загрузить палитру/анимации подсветки
 */
async function loadState(includeBacklight = true) {
    const state = await apiCall('/api/state');
    if (state) {
        updatePowerToggleUI(state.power);
        setIfIdle($('ledCount'), state.numLeds);
        setIfIdle($('autoSwitchDelay'), state.autoSwitchDelay);
        $('randomOrder').checked = state.randomOrder;
        currentModeId = state.currentMode;
        if (state.modeSettings) modeSettingsCache = state.modeSettings;
        if (state.operationMode !== undefined) {
            operationMode = state.operationMode;
            updateOperationModeUI();
        }
        updateBrightnessUI(state.brightness);

        // Полная перерисовка сетки — только если поменялся состав архива
        const signature = (state.modeSettings || []).map(m => m.archived ? 1 : 0).join('');
        if (signature !== lastArchivedSignature) {
            lastArchivedSignature = signature;
            renderModes();
        } else {
            updateModeCards();
        }
    }
    if (includeBacklight) await loadBacklightState();
}

const POLL_INTERVAL_MS = 10000;
const USER_ACTION_QUIET_MS = 5000;

/**
 * Периодический опрос (видно авто-переключение и изменения с других устройств).
 * Пропускается, если вкладка скрыта, пользователь только что что-то менял,
 * открыта шторка или очередь запросов не пуста.
 */
async function pollState() {
    const busy = requestInFlight || requestQueue.length > 0;
    const recentlyTouched = Date.now() - lastUserActionAt < USER_ACTION_QUIET_MS;
    if (!document.hidden && !busy && !recentlyTouched && !isAnyModalOpen()) {
        await loadState(operationMode === 1);
    }
    setTimeout(pollState, POLL_INTERVAL_MS);
}

// ===== Подсветка: палитра =====

/**
 * Загружает режимы и палитру подсветки.
 */
async function loadBacklightState() {
    const state = await apiCall('/api/backlight/state');
    if (state) {
        backlightState.currentMode = state.currentMode;
        backlightState.activeColorIndex = state.activeColorIndex;
        backlightState.modeSettings = state.modeSettings || [];
        backlightState.autoSwitch = {
            modeDelay: state.modeAutoSwitchDelay || 0,
            modeRandom: state.modeRandomOrder || false,
            colorDelay: state.colorAutoSwitchDelay || 0,
            colorRandom: state.colorRandomOrder || false
        };
        setIfIdle($('blModeDelay'), backlightState.autoSwitch.modeDelay);
        setIfIdle($('blColorDelay'), backlightState.autoSwitch.colorDelay);
        renderBacklightModes();
    }
    const colors = await apiCall('/api/backlight/colors');
    if (colors && colors.colors) {
        backlightState.colors = colors.colors;
        backlightState.activeColorIndex = colors.activeIndex;
        renderColorPalette();
    }
}

/**
 * Перерисовывает палитру цветов подсветки.
 */
function renderColorPalette() {
    const palette = $('colorPalette');
    palette.innerHTML = '';
    palette.classList.toggle('editing', paletteEditMode);
    backlightState.colors.forEach((c, index) => {
        if (!c.enabled) return;
        const sw = document.createElement('button');
        sw.className = 'sw' + (index === backlightState.activeColorIndex ? ' active' : '');
        sw.style.background = c.hex;
        sw.dataset.index = index;
        sw.innerHTML = '<span class="x" data-del="1">×</span>';
        palette.appendChild(sw);
    });
    // Кнопка «добавить», если есть свободные слоты
    if (backlightState.colors.some(c => !c.enabled)) {
        const add = document.createElement('button');
        add.className = 'sw add';
        add.textContent = '+';
        add.dataset.add = '1';
        palette.appendChild(add);
    }
    $('paletteHint').textContent = paletteEditMode
        ? 'Нажмите на цвет, чтобы изменить его, × — удалить'
        : 'Нажмите на цвет, чтобы включить его';
}

/**
 * Клик по палитре: выбор, редактирование, удаление или добавление цвета.
 * @param {MouseEvent} e
 */
function onPaletteClick(e) {
    const sw = e.target.closest('.sw');
    if (!sw) return;
    if (sw.dataset.add) { openColorPicker(); return; }
    const index = parseInt(sw.dataset.index);
    if (paletteEditMode && e.target.closest('[data-del]')) { deleteBacklightColor(index); return; }
    if (paletteEditMode) openColorEditor(index, backlightState.colors[index].hex);
    else selectBacklightColor(index);
}

/**
 * Включает/выключает режим редактирования палитры.
 */
function togglePaletteEdit() {
    paletteEditMode = !paletteEditMode;
    $('paletteEditBtn').textContent = paletteEditMode ? 'Готово' : '✎ Изменить';
    renderColorPalette();
}

/**
 * Делает цвет активным.
 * @param {number} index
 */
async function selectBacklightColor(index) {
    backlightState.activeColorIndex = index;
    renderColorPalette();
    await apiCall('/api/backlight/active-color', { index: index });
}

/**
 * Удаляет цвет из палитры.
 * @param {number} index
 */
async function deleteBacklightColor(index) {
    if (!confirm('Удалить этот цвет?')) return;
    markUserAction();
    const result = await apiRequest('DELETE', `/api/backlight/colors?index=${index}`);
    if (result && result.success) {
        closeColorPicker();
        await loadBacklightState();
    } else if (result && result.error) {
        showToast(result.error === 'Cannot delete last color' ? 'Нельзя удалить последний цвет' : result.error);
    }
}

/**
 * Удаляет цвет, открытый в редакторе.
 */
function deleteEditingColor() {
    if (editingColorIndex !== null) deleteBacklightColor(editingColorIndex);
}

/**
 * Открывает шторку добавления цвета.
 */
function openColorPicker() {
    editingColorIndex = null;
    $('colorPickerTitle').textContent = 'Новый цвет';
    $('colorSaveBtn').textContent = 'Добавить';
    $('colorDeleteBtn').classList.add('hidden');
    $('newColorPicker').value = '#ff9966';
    openSheet('colorPickerModal');
}

/**
 * Открывает шторку редактирования цвета; цвет сразу становится активным,
 * чтобы изменения были видны на ленте.
 * @param {number} index
 * @param {string} currentHex
 */
function openColorEditor(index, currentHex) {
    editingColorIndex = index;
    $('colorPickerTitle').textContent = 'Изменить цвет';
    $('colorSaveBtn').textContent = 'Готово';
    $('colorDeleteBtn').classList.remove('hidden');
    $('newColorPicker').value = currentHex.toLowerCase();
    openSheet('colorPickerModal');
    if (backlightState.activeColorIndex !== index) selectBacklightColor(index);
}

/**
 * Закрывает шторку цвета.
 */
function closeColorPicker() {
    editingColorIndex = null;
    clearTimeout(colorPreviewTimer);
    closeSheet('colorPickerModal');
}

/**
 * HEX → {r,g,b}.
 * @param {string} hex
 * @returns {{r:number,g:number,b:number}|null}
 */
function hexToRgb(hex) {
    const m = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return m ? { r: parseInt(m[1], 16), g: parseInt(m[2], 16), b: parseInt(m[3], 16) } : null;
}

/**
 * Живой предпросмотр при редактировании существующего цвета.
 */
function previewColorChange() {
    if (editingColorIndex === null) return;
    markUserAction();
    const index = editingColorIndex;
    clearTimeout(colorPreviewTimer);
    colorPreviewTimer = setTimeout(async () => {
        const hex = $('newColorPicker').value;
        const rgb = hexToRgb(hex);
        if (!rgb) return;
        await apiCall('/api/backlight/colors', { index, r: rgb.r, g: rgb.g, b: rgb.b });
        if (backlightState.colors[index]) backlightState.colors[index].hex = hex;
        renderColorPalette();
    }, 250);
}

/**
 * Добавляет новый цвет или закрывает редактор (изменения уже применены).
 */
async function saveColor() {
    const rgb = hexToRgb($('newColorPicker').value);
    if (!rgb) return;
    if (editingColorIndex !== null) {
        closeColorPicker();
        return;
    }
    const result = await apiCall('/api/backlight/colors', { r: rgb.r, g: rgb.g, b: rgb.b });
    if (result && result.success) {
        closeColorPicker();
        await loadBacklightState();
    } else if (result && result.error) {
        showToast(result.error === 'No free color slots' ? 'Палитра заполнена' : result.error);
    }
}

/**
 * Сохраняет авто-переключение подсветки (флаги «случайно» сохраняются как есть).
 */
async function updateBacklightAutoSwitch() {
    const modeDelay = Math.max(0, parseInt($('blModeDelay').value) || 0);
    const colorDelay = Math.max(0, parseInt($('blColorDelay').value) || 0);
    const a = backlightState.autoSwitch;
    await apiCall('/api/backlight/auto-switch', {
        modeDelay, modeRandom: a.modeRandom, colorDelay, colorRandom: a.colorRandom
    });
    a.modeDelay = modeDelay;
    a.colorDelay = colorDelay;
}

// ===== Подсветка: анимации =====

/**
 * Перерисовывает сетку анимаций подсветки.
 */
function renderBacklightModes() {
    const grid = $('backlightModesGrid');
    grid.innerHTML = '';
    backlightModeNames.forEach((name, index) => {
        const card = createModeCard(name, index);
        card.classList.toggle('active', index === backlightState.currentMode);
        grid.appendChild(card);
    });
}

/**
 * Клик по сетке анимаций подсветки.
 * @param {MouseEvent} e
 */
function onBacklightGridClick(e) {
    const card = e.target.closest('[data-mode-id]');
    if (!card) return;
    const id = parseInt(card.dataset.modeId);
    if (e.target.closest('[data-gear]')) openBacklightModeSettings(id);
    else selectBacklightMode(id);
}

/**
 * Выбор анимации подсветки.
 * @param {number} modeId
 */
async function selectBacklightMode(modeId) {
    backlightState.currentMode = modeId;
    renderBacklightModes();
    await apiCall('/api/backlight/mode', { mode: modeId });
}

/**
 * Открывает шторку настроек анимации подсветки.
 * @param {number} modeId
 */
function openBacklightModeSettings(modeId) {
    editingBacklightModeId = modeId;
    $('blModeSettingsTitle').textContent = backlightModeNames[modeId];
    const s = backlightState.modeSettings[modeId] || { speed: 128, intensity: 128, brightness: 255 };
    $('blModeSpeed').value = s.speed;
    $('blModeIntensity').value = s.intensity;
    $('blModeBrightness').value = s.brightness;
    $('blModeSpeedVal').textContent = s.speed;
    $('blModeIntensityVal').textContent = s.intensity;
    $('blModeBrightnessVal').textContent = s.brightness;
    openSheet('blModeSettingsModal');
}

/**
 * Закрывает шторку настроек анимации подсветки.
 */
function closeBacklightModeSettings() {
    editingBacklightModeId = null;
    closeSheet('blModeSettingsModal');
}

/**
 * Изменение слайдеров анимации подсветки.
 */
function updateBacklightModeSettings() {
    markUserAction();
    const speed = parseInt($('blModeSpeed').value);
    const intensity = parseInt($('blModeIntensity').value);
    const brightness = parseInt($('blModeBrightness').value);
    $('blModeSpeedVal').textContent = speed;
    $('blModeIntensityVal').textContent = intensity;
    $('blModeBrightnessVal').textContent = brightness;

    const modeId = editingBacklightModeId;
    if (modeId === null) return;
    if (backlightState.modeSettings[modeId]) {
        Object.assign(backlightState.modeSettings[modeId], { speed, intensity, brightness });
    }
    clearTimeout(blModeSettingsTimer);
    blModeSettingsTimer = setTimeout(() => {
        apiCall('/api/backlight/mode/settings', { modeId, speed, intensity, brightness });
    }, 150);
}

// ===== Расписание =====

const DAY_NAMES_FULL = ['Воскресенье', 'Понедельник', 'Вторник', 'Среда', 'Четверг', 'Пятница', 'Суббота'];
const DAY_SHORT = ['Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб', 'Вс'];

/**
 * Открывает шторку расписания.
 */
function openSchedule() {
    selectedDays = 0x7F;
    updateDayButtons();
    setScheduleAction(true);
    openSheet('scheduleModal');
    loadSchedules();
    fetchDeviceClock();
    // Время с устройства берём раз в 30 с, между запросами тикаем локально
    clockTimer = setInterval(() => {
        if (clockBase && Date.now() - clockBase.at > 30000) fetchDeviceClock();
        renderClock();
    }, 1000);
}

/**
 * Закрывает шторку расписания.
 */
function closeSchedule() {
    closeSheet('scheduleModal');
    clearInterval(clockTimer);
    clockTimer = null;
}

/**
 * Запрашивает время устройства.
 */
async function fetchDeviceClock() {
    const t = await apiCall('/api/time');
    if (t) {
        clockBase = { ...t, at: Date.now() };
        renderClock();
    }
}

/**
 * Показывает время устройства с учётом прошедших с запроса секунд.
 */
function renderClock() {
    if (!clockBase) return;
    const elapsed = Math.floor((Date.now() - clockBase.at) / 1000);
    let total = clockBase.hour * 3600 + clockBase.minute * 60 + clockBase.second + elapsed;
    const dayShift = Math.floor(total / 86400);
    total %= 86400;
    const h = String(Math.floor(total / 3600)).padStart(2, '0');
    const m = String(Math.floor(total / 60) % 60).padStart(2, '0');
    $('currentTime').textContent = `${h}:${m}`;
    $('currentDay').textContent = DAY_NAMES_FULL[(clockBase.dayOfWeek + dayShift) % 7];
}

/**
 * Переключает день недели в форме нового правила.
 * @param {number} dayIndex - 0 = Пн
 */
function toggleDay(dayIndex) {
    selectedDays ^= (1 << dayIndex);
    updateDayButtons();
}

/**
 * Обновляет кнопки дней недели.
 */
function updateDayButtons() {
    for (let i = 0; i < 7; i++) $(`day${i}`).classList.toggle('on', !!(selectedDays & (1 << i)));
}

/**
 * Выбор действия правила.
 * @param {boolean} on - включить/выключить
 */
function setScheduleAction(on) {
    scheduleAction = on;
    $('actOn').classList.toggle('active', on);
    $('actOff').classList.toggle('active', !on);
}

/**
 * Загружает расписания.
 */
async function loadSchedules() {
    const data = await apiCall('/api/schedules');
    if (data && data.schedules) {
        schedulesCache = data.schedules;
        renderSchedules();
    }
}

/**
 * Перерисовывает список правил (отсортирован по времени).
 */
function renderSchedules() {
    const list = $('scheduleList');
    list.innerHTML = '';
    const active = schedulesCache.filter(s => s.enabled)
        .sort((a, b) => (a.hour * 60 + a.minute) - (b.hour * 60 + b.minute));
    if (active.length === 0) {
        list.innerHTML = '<div class="muted" style="text-align:center">Правил пока нет</div>';
        return;
    }
    active.forEach(s => {
        const days = DAY_SHORT.filter((_, i) => s.daysOfWeek & (1 << i));
        const daysStr = days.length === 7 ? 'Каждый день'
            : (s.daysOfWeek === 0x1F ? 'По будням' : (s.daysOfWeek === 0x60 ? 'По выходным' : days.join(', ')));
        const row = document.createElement('div');
        row.className = 'sched';
        row.innerHTML = `
            <b>${String(s.hour).padStart(2, '0')}:${String(s.minute).padStart(2, '0')}</b>
            <div class="grow">${s.action ? '🟢 Включить' : '🔴 Выключить'}<div class="muted">${daysStr}</div></div>
            <button class="btn small danger" onclick="deleteSchedule(${s.id})">Удалить</button>`;
        list.appendChild(row);
    });
}

/**
 * Добавляет правило в первый свободный слот.
 */
async function addSchedule() {
    const [hour, minute] = ($('scheduleTime').value || '').split(':').map(Number);
    if (isNaN(hour) || isNaN(minute)) { showToast('Укажите время'); return; }
    if (selectedDays === 0) { showToast('Выберите хотя бы один день'); return; }
    const slot = schedulesCache.find(s => !s.enabled);
    if (!slot) { showToast('Максимум 10 правил'); return; }
    const result = await apiCall('/api/schedules', {
        id: slot.id, enabled: true, hour, minute, action: scheduleAction, daysOfWeek: selectedDays
    });
    if (result && result.success) {
        await loadSchedules();
        showToast('Правило добавлено');
    }
}

/**
 * Удаляет правило.
 * @param {number} id
 */
async function deleteSchedule(id) {
    if (!confirm('Удалить это правило?')) return;
    markUserAction();
    const result = await apiRequest('DELETE', `/api/schedules?id=${id}`);
    if (result && result.success) await loadSchedules();
}

/**
 * Передаёт время браузера на устройство, если там время не задано или уехало > 60 с
 * (без нужды NTP не перебиваем).
 */
async function syncTimeFromBrowser() {
    const timestamp = Math.floor(Date.now() / 1000);
    const deviceTime = await apiCall('/api/time');
    if (deviceTime && deviceTime.timestamp > 1000000000 &&
        Math.abs(deviceTime.timestamp - timestamp) < 60) {
        return;
    }
    await apiCall('/api/time/set', { timestamp });
}

// ===== Логи (WebSocket) =====
// Подключаемся только когда раздел логов открыт: каждый клиент
// занимает память на устройстве.
let ws = null;
let logsBuffer = [];
let logsPaused = false;
const MAX_LOG_LINES = 200;

/**
 * Реакция на открытие/закрытие раздела логов.
 */
function onLogsToggle() {
    if ($('logsDetails').open) connectWebSocket();
    else disconnectWebSocket();
}

/**
 * Подключает WebSocket логов (с переподключением, пока раздел открыт).
 */
function connectWebSocket() {
    if (ws) return;
    $('logsConsole').innerHTML = '';
    $('wsStatusText').textContent = 'подключение…';
    const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
    ws = new WebSocket(`${protocol}//${location.host}/ws/logs`);
    ws.onopen = () => { $('wsStatusText').textContent = 'онлайн'; };
    ws.onclose = () => {
        ws = null;
        if ($('logsDetails').open) {
            $('wsStatusText').textContent = 'переподключение…';
            setTimeout(() => { if ($('logsDetails').open) connectWebSocket(); }, 3000);
        } else {
            $('wsStatusText').textContent = 'выкл';
        }
    };
    ws.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            if (data.type === 'history') data.logs.forEach(l => addLogToConsole(l.timestamp, l.message));
            else if (data.timestamp && data.message) addLogToConsole(data.timestamp, data.message);
        } catch (e) {
            console.error('WS parse error', e);
        }
    };
}

/**
 * Отключает WebSocket логов.
 */
function disconnectWebSocket() {
    if (ws) { ws.onclose = null; ws.close(); ws = null; }
    $('wsStatusText').textContent = 'выкл';
}

/**
 * Добавляет строку в консоль логов.
 * @param {string} timestamp
 * @param {string} message
 */
function addLogToConsole(timestamp, message) {
    if (logsPaused) return;
    logsBuffer.push({ timestamp, message });
    if (logsBuffer.length > MAX_LOG_LINES) logsBuffer.shift();
    const box = $('logsConsole');
    const line = document.createElement('div');
    const ts = document.createElement('span');
    ts.className = 'ts';
    ts.textContent = `[${timestamp}] `;
    line.appendChild(ts);
    line.appendChild(document.createTextNode(message.replace(/\n+$/, '')));
    box.appendChild(line);
    while (box.children.length > MAX_LOG_LINES) box.removeChild(box.firstChild);
    box.scrollTop = box.scrollHeight;
}

/**
 * Очищает консоль логов.
 */
function clearLogs() {
    $('logsConsole').innerHTML = '';
    logsBuffer = [];
}

/**
 * Пауза/продолжение вывода логов.
 */
function toggleLogsPause() {
    logsPaused = !logsPaused;
    $('pauseLogsBtn').textContent = logsPaused ? '▶ Продолжить' : '⏸ Пауза';
}

/**
 * Скачивает накопленные логи текстовым файлом.
 */
function downloadLogs() {
    const text = logsBuffer.map(l => `[${l.timestamp}] ${l.message}`).join('');
    const url = URL.createObjectURL(new Blob([text], { type: 'text/plain' }));
    const a = document.createElement('a');
    a.href = url;
    a.download = `vladiksled-logs-${new Date().toISOString().replace(/:/g, '-')}.txt`;
    a.click();
    URL.revokeObjectURL(url);
}

// ===== Старт =====
window.addEventListener('DOMContentLoaded', () => {
    $('modesGrid').addEventListener('click', onModesGridClick);
    $('archivedGrid').addEventListener('click', onModesGridClick);
    $('backlightModesGrid').addEventListener('click', onBacklightGridClick);
    $('colorPalette').addEventListener('click', onPaletteClick);
    document.addEventListener('keydown', (e) => {
        if (e.key !== 'Escape') return;
        const open = document.querySelector('.sheet-overlay.open');
        if (open) onOverlayClick({ target: open, currentTarget: open });
    });
    // Вернулись на вкладку — сразу обновляем состояние
    document.addEventListener('visibilitychange', () => {
        if (!document.hidden && !isAnyModalOpen()) loadState(operationMode === 1);
    });

    updateOperationModeUI();
    renderModes();
    renderBacklightModes();
    syncTimeFromBrowser();
    loadState(true).then(() => setTimeout(pollState, POLL_INTERVAL_MS));
});
</script>
</body>
</html>
)rawliteral";

#endif
