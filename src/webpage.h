#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>

const char WEBPAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>🎄 WiFi LED Controller</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <style>
        body {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
        }
        .glass {
            background: rgba(255, 255, 255, 0.1);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.2);
        }
        .toggle-checkbox:checked {
            background-color: #10b981;
            border-color: #10b981;
        }
        .toggle-checkbox:checked ~ .toggle-label {
            transform: translateX(100%);
        }
        .toggle-bg {
            transition: background-color 0.3s ease;
        }
        .toggle-label {
            transition: transform 0.3s ease;
        }
        .mode-card {
            transition: all 0.3s ease;
        }
        .mode-card:hover {
            transform: translateY(-4px);
            box-shadow: 0 10px 25px rgba(0,0,0,0.3);
        }
        .mode-card.active {
            border: 2px solid #10b981;
            box-shadow: 0 0 20px rgba(16, 185, 129, 0.6);
            background: rgba(16, 185, 129, 0.15);
            transform: scale(1.02);
        }
        .color-swatch {
            width: 40px;
            height: 40px;
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.2s ease;
            border: 2px solid transparent;
        }
        .color-swatch:hover {
            transform: scale(1.1);
        }
        .color-swatch.active {
            border: 3px solid #10b981;
            box-shadow: 0 0 10px rgba(16, 185, 129, 0.8);
        }
        .op-mode-btn {
            transition: all 0.3s ease;
        }
        .op-mode-btn.active {
            background: linear-gradient(135deg, #10b981 0%, #059669 100%);
            box-shadow: 0 4px 15px rgba(16, 185, 129, 0.4);
        }
    </style>
</head>
<body class="p-2">
    <div class="max-w-6xl mx-auto">
        <!-- Header -->
        <div class="glass rounded-2xl shadow-2xl p-3 mb-3">
            <!-- Title and Power Toggle on same line -->
            <div class="flex items-center justify-between mb-3">
                <h1 class="text-2xl md:text-3xl font-bold text-white">
                    🎄 WiFi LED Controller
                </h1>
                <div class="flex items-center gap-2">
                    <span class="text-white text-sm md:text-base font-semibold">⚡</span>
                    <div class="relative">
                        <input type="checkbox" id="powerToggle" class="toggle-checkbox sr-only" checked>
                        <div class="toggle-bg block bg-gray-600 w-14 h-8 rounded-full cursor-pointer" onclick="togglePower()"></div>
                        <div class="toggle-label absolute left-1 top-1 bg-white w-6 h-6 rounded-full cursor-pointer" onclick="togglePower()"></div>
                    </div>
                </div>
            </div>

            <!-- Operation Mode Toggle -->
            <div class="mb-3 p-2 bg-white bg-opacity-10 rounded-xl">
                <div class="text-white text-sm font-semibold mb-2 text-center">Режим работы</div>
                <div class="flex gap-2">
                    <button id="opModeGarland" onclick="setOperationMode(0)" 
                            class="op-mode-btn flex-1 py-2 px-3 rounded-lg font-semibold text-white bg-white bg-opacity-20">
                        🎄 Гирлянда
                    </button>
                    <button id="opModeBacklight" onclick="setOperationMode(1)" 
                            class="op-mode-btn flex-1 py-2 px-3 rounded-lg font-semibold text-white bg-white bg-opacity-20">
                        💡 Подсветка
                    </button>
                </div>
            </div>

            <!-- Brightness Slider -->
            <div class="mb-3 p-2 bg-white bg-opacity-10 rounded-xl">
                <div class="flex items-center justify-between mb-2">
                    <span class="text-white text-base font-semibold">☀️ Яркость</span>
                    <span class="text-white text-sm" id="brightnessValue">50%</span>
                </div>
                <input type="range" id="brightnessSlider" min="0" max="255" value="128" 
                       class="w-full h-2 bg-gray-300 rounded-lg appearance-none cursor-pointer"
                       oninput="updateBrightness(this.value)">
            </div>

            <!-- LED Count -->
            <div class="mb-3 p-2 bg-white bg-opacity-10 rounded-xl">
                <div class="flex items-center justify-between">
                    <span class="text-white text-base font-semibold">💡 Количество диодов</span>
                    <input type="number" id="ledCount" min="1" max="300" value="50"
                           class="bg-white bg-opacity-20 text-white px-3 py-1 rounded-lg w-20 text-center text-sm"
                           onchange="updateLEDCount(this.value)">
                </div>
            </div>

            <!-- Settings and Schedule Buttons -->
            <div class="flex gap-2">
                <button onclick="openSettings()" 
                        class="flex-1 bg-blue-500 hover:bg-blue-600 text-white font-bold py-2 px-4 rounded-xl transition duration-300">
                    ⚙️ Настройки
                </button>
                <button onclick="openSchedule()" 
                        class="flex-1 bg-purple-500 hover:bg-purple-600 text-white font-bold py-2 px-4 rounded-xl transition duration-300">
                    📅 Расписание
                </button>
            </div>
        </div>

        <!-- Garland Modes Grid (показывается когда operationMode = 0) -->
        <div id="garlandPanel" class="glass rounded-2xl shadow-2xl p-3 mb-3">
            <div class="flex items-center justify-between mb-3">
                <h2 class="text-xl font-bold text-white">🎨 Режимы гирлянды</h2>
            </div>
            <div class="mb-3 p-2 bg-white bg-opacity-10 rounded-xl">
                <span class="text-white text-sm font-semibold">Активный режим: </span>
                <span class="text-white text-sm" id="currentModeName">-</span>
                <span class="text-white text-xs opacity-75 ml-1" id="currentModeIndex">(-/-)</span>
            </div>
            
            <!-- Tabs for Active/Archived -->
            <div class="flex gap-2 mb-3">
                <button id="activeTab" onclick="switchTab('active')" 
                        class="flex-1 py-2 px-4 rounded-lg font-semibold transition duration-300 bg-green-500 text-white">
                    ✅ Активные
                </button>
                <button id="archivedTab" onclick="switchTab('archived')" 
                        class="flex-1 py-2 px-4 rounded-lg font-semibold transition duration-300 bg-white bg-opacity-20 text-white">
                    📦 Архивные
                </button>
            </div>
            
            <div id="modesGrid" class="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-2">
                <!-- Modes will be loaded here -->
            </div>
        </div>

        <!-- Backlight Panel (показывается когда operationMode = 1) -->
        <div id="backlightPanel" class="glass rounded-2xl shadow-2xl p-3 mb-3 hidden">
            <div class="flex items-center justify-between mb-3">
                <h2 class="text-xl font-bold text-white">💡 Настройки подсветки</h2>
            </div>
            
            <!-- Color Palette -->
            <div class="mb-4 p-3 bg-white bg-opacity-10 rounded-xl">
                <div class="flex items-center justify-between mb-2">
                    <span class="text-white text-sm font-semibold">🎨 Цвета</span>
                    <button onclick="openColorPicker()" 
                            class="bg-green-500 hover:bg-green-600 text-white text-xs py-1 px-3 rounded-lg">
                        ➕ Добавить
                    </button>
                </div>
                <div id="colorPalette" class="flex flex-wrap gap-2">
                    <!-- Colors will be loaded here -->
                </div>
            </div>
            
            <!-- Backlight Modes -->
            <div class="mb-3 p-3 bg-white bg-opacity-10 rounded-xl">
                <div class="text-white text-sm font-semibold mb-2">🌊 Режимы анимации</div>
                <div id="backlightModesGrid" class="grid grid-cols-2 gap-2">
                    <!-- Backlight modes will be loaded here -->
                </div>
                
                <!-- Auto-switch Settings -->
                <div class="mt-3 pt-3 border-t border-white border-opacity-20">
                    <div class="text-white text-xs font-semibold mb-2">⏱️ Авто-переключение</div>
                    <div class="grid grid-cols-2 gap-2">
                        <div>
                            <label class="text-white text-xs block mb-1">Режимы (сек)</label>
                            <input type="number" id="blModeDelay" min="0" max="3600" value="0"
                                   class="w-full bg-white bg-opacity-20 text-white px-2 py-1 rounded text-xs"
                                   onchange="updateBacklightAutoSwitch()">
                        </div>
                        <div>
                            <label class="text-white text-xs block mb-1">Цвета (сек)</label>
                            <input type="number" id="blColorDelay" min="0" max="3600" value="0"
                                   class="w-full bg-white bg-opacity-20 text-white px-2 py-1 rounded text-xs"
                                   onchange="updateBacklightAutoSwitch()">
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- Logs Console -->
        <div class="glass rounded-2xl shadow-2xl p-3 mt-3">
            <div class="flex items-center justify-between mb-3">
                <h2 class="text-xl font-bold text-white">📋 Логи</h2>
                <div class="flex gap-2">
                    <button id="pauseLogsBtn" onclick="toggleLogsPause()" 
                            class="bg-yellow-500 hover:bg-yellow-600 text-white text-xs py-1 px-3 rounded-lg">
                        ⏸️ Пауза
                    </button>
                    <button onclick="clearLogs()" 
                            class="bg-red-500 hover:bg-red-600 text-white text-xs py-1 px-3 rounded-lg">
                        🗑️ Очистить
                    </button>
                    <button onclick="downloadLogs()" 
                            class="bg-blue-500 hover:bg-blue-600 text-white text-xs py-1 px-3 rounded-lg">
                        💾 Скачать
                    </button>
                </div>
            </div>
            <div id="logsConsole" class="bg-black bg-opacity-80 rounded-lg p-3 h-64 overflow-y-auto font-mono text-xs text-green-400">
                <div class="text-gray-400">Подключение к WebSocket...</div>
            </div>
            <div class="mt-2 text-white text-xs opacity-75">
                <span id="wsStatus" class="inline-block w-2 h-2 rounded-full bg-yellow-500 mr-1"></span>
                <span id="wsStatusText">Подключение...</span>
            </div>
        </div>
    </div>

    <!-- Settings Modal (Garland) -->
    <div id="settingsModal" class="fixed inset-0 bg-black bg-opacity-50 hidden items-center justify-center z-50">
        <div class="glass rounded-2xl p-6 m-4 max-w-md w-full">
            <h2 class="text-2xl font-bold text-white mb-4">⚙️ Настройки гирлянды</h2>
            
            <div class="mb-4">
                <label class="text-white block mb-2">Авто-переключение (сек, 0 = выкл)</label>
                <input type="number" id="autoSwitchDelay" min="0" max="3600" value="0"
                       class="w-full bg-white bg-opacity-20 text-white px-4 py-2 rounded-lg">
            </div>

            <div class="mb-6">
                <label class="flex items-center text-white cursor-pointer">
                    <input type="checkbox" id="randomOrder" class="mr-2">
                    <span>Случайный порядок режимов</span>
                </label>
            </div>

            <div class="flex gap-2">
                <button onclick="saveSettings()" 
                        class="flex-1 bg-green-500 hover:bg-green-600 text-white font-bold py-2 px-4 rounded-lg">
                    Сохранить
                </button>
                <button onclick="closeSettings()" 
                        class="flex-1 bg-gray-500 hover:bg-gray-600 text-white font-bold py-2 px-4 rounded-lg">
                    Закрыть
                </button>
            </div>
        </div>
    </div>

    <!-- Mode Settings Modal (Garland) -->
    <div id="modeSettingsModal" class="fixed inset-0 bg-black bg-opacity-50 hidden items-center justify-center z-50">
        <div class="glass rounded-2xl p-4 m-4 max-w-md w-full">
            <h2 class="text-xl font-bold text-white mb-3" id="modeSettingsTitle">Настройки режима</h2>
            
            <div class="mb-3">
                <label class="text-white block mb-1 text-sm">Скорость</label>
                <input type="range" id="modeSpeed" min="0" max="255" value="128"
                       class="w-full h-2 bg-gray-300 rounded-lg appearance-none cursor-pointer"
                       oninput="updateModeSettings()">
            </div>

            <div class="mb-3">
                <label class="text-white block mb-1 text-sm">Масштаб</label>
                <input type="range" id="modeScale" min="0" max="255" value="128"
                       class="w-full h-2 bg-gray-300 rounded-lg appearance-none cursor-pointer"
                       oninput="updateModeSettings()">
            </div>

            <div class="mb-3">
                <label class="text-white block mb-1 text-sm">Яркость режима</label>
                <input type="range" id="modeBrightness" min="0" max="255" value="255"
                       class="w-full h-2 bg-gray-300 rounded-lg appearance-none cursor-pointer"
                       oninput="updateModeSettings()">
            </div>

            <div class="mb-3">
                <button id="archiveButton" onclick="toggleArchiveFromSettings()" 
                        class="w-full bg-orange-500 hover:bg-orange-600 text-white font-bold py-2 px-3 rounded-lg text-sm">
                    📦 Архивировать
                </button>
            </div>

            <div class="flex gap-2">
                <button onclick="resetModeSettings()" 
                        class="flex-1 bg-yellow-500 hover:bg-yellow-600 text-white font-bold py-2 px-3 rounded-lg text-sm">
                    🔄 По умолчанию
                </button>
                <button onclick="closeModeSettings()" 
                        class="flex-1 bg-gray-500 hover:bg-gray-600 text-white font-bold py-2 px-3 rounded-lg text-sm">
                    Закрыть
                </button>
            </div>
        </div>
    </div>

    <!-- Backlight Mode Settings Modal -->
    <div id="blModeSettingsModal" class="fixed inset-0 bg-black bg-opacity-50 hidden items-center justify-center z-50">
        <div class="glass rounded-2xl p-4 m-4 max-w-md w-full">
            <h2 class="text-xl font-bold text-white mb-3" id="blModeSettingsTitle">Настройки режима</h2>
            
            <div class="mb-3">
                <label class="text-white block mb-1 text-sm">Скорость</label>
                <input type="range" id="blModeSpeed" min="0" max="255" value="128"
                       class="w-full h-2 bg-gray-300 rounded-lg appearance-none cursor-pointer"
                       oninput="updateBacklightModeSettings()">
            </div>

            <div class="mb-3">
                <label class="text-white block mb-1 text-sm">Интенсивность</label>
                <input type="range" id="blModeIntensity" min="0" max="255" value="128"
                       class="w-full h-2 bg-gray-300 rounded-lg appearance-none cursor-pointer"
                       oninput="updateBacklightModeSettings()">
            </div>

            <div class="mb-3">
                <label class="text-white block mb-1 text-sm">Яркость</label>
                <input type="range" id="blModeBrightness" min="0" max="255" value="255"
                       class="w-full h-2 bg-gray-300 rounded-lg appearance-none cursor-pointer"
                       oninput="updateBacklightModeSettings()">
            </div>

            <div class="flex gap-2">
                <button onclick="closeBacklightModeSettings()" 
                        class="flex-1 bg-gray-500 hover:bg-gray-600 text-white font-bold py-2 px-3 rounded-lg text-sm">
                    Закрыть
                </button>
            </div>
        </div>
    </div>

    <!-- Color Picker Modal (Add/Edit) -->
    <div id="colorPickerModal" class="fixed inset-0 bg-black bg-opacity-50 hidden items-center justify-center z-50">
        <div class="glass rounded-2xl p-4 m-4 max-w-md w-full">
            <h2 class="text-xl font-bold text-white mb-3" id="colorPickerTitle">🎨 Добавить цвет</h2>
            
            <div class="mb-4">
                <input type="color" id="newColorPicker" value="#ff9966" 
                       class="w-full h-16 rounded-lg cursor-pointer"
                       oninput="previewColorChange()">
            </div>
            
            <div class="flex gap-2">
                <button onclick="saveColor()" id="colorSaveBtn"
                        class="flex-1 bg-green-500 hover:bg-green-600 text-white font-bold py-2 px-4 rounded-lg">
                    ➕ Добавить
                </button>
                <button onclick="closeColorPicker()" 
                        class="flex-1 bg-gray-500 hover:bg-gray-600 text-white font-bold py-2 px-4 rounded-lg">
                    Отмена
                </button>
            </div>
        </div>
    </div>

    <!-- Schedule Modal -->
    <div id="scheduleModal" class="fixed inset-0 bg-black bg-opacity-50 hidden items-center justify-center z-50">
        <div class="glass rounded-2xl p-4 m-4 max-w-md w-full max-h-[90vh] overflow-y-auto">
            <h2 class="text-xl font-bold text-white mb-3">📅 Расписание</h2>
            
            <!-- Current Time Display -->
            <div class="mb-4 p-3 bg-white bg-opacity-20 rounded-xl">
                <div class="text-white text-center">
                    <div class="text-sm opacity-75">Текущее время ESP8266:</div>
                    <div class="text-2xl font-bold" id="currentTime">--:--</div>
                    <div class="text-xs opacity-75" id="currentDay">-</div>
                </div>
            </div>
            
            <!-- Add Schedule Form -->
            <div class="mb-4 p-3 bg-white bg-opacity-10 rounded-xl">
                <h3 class="text-white font-semibold mb-2 text-sm">Добавить расписание</h3>
                
                <div class="mb-2">
                    <label class="text-white block mb-1 text-xs">Время</label>
                    <div class="flex gap-2">
                        <input type="number" id="scheduleHour" min="0" max="23" value="8" placeholder="ЧЧ"
                               class="flex-1 bg-white bg-opacity-20 text-white px-2 py-1 rounded text-center text-sm">
                        <span class="text-white self-center">:</span>
                        <input type="number" id="scheduleMinute" min="0" max="59" value="0" placeholder="ММ"
                               class="flex-1 bg-white bg-opacity-20 text-white px-2 py-1 rounded text-center text-sm">
                    </div>
                </div>
                
                <div class="mb-2">
                    <label class="text-white block mb-1 text-xs">Действие</label>
                    <select id="scheduleAction" class="w-full bg-white bg-opacity-20 text-white px-2 py-1 rounded text-sm">
                        <option value="true">Включить</option>
                        <option value="false">Выключить</option>
                    </select>
                </div>
                
                <div class="mb-3">
                    <label class="text-white block mb-1 text-xs">Дни недели</label>
                    <div class="grid grid-cols-7 gap-1">
                        <button onclick="toggleDay(0)" id="day0" class="day-btn bg-white bg-opacity-20 text-white text-xs py-1 rounded">Пн</button>
                        <button onclick="toggleDay(1)" id="day1" class="day-btn bg-white bg-opacity-20 text-white text-xs py-1 rounded">Вт</button>
                        <button onclick="toggleDay(2)" id="day2" class="day-btn bg-white bg-opacity-20 text-white text-xs py-1 rounded">Ср</button>
                        <button onclick="toggleDay(3)" id="day3" class="day-btn bg-white bg-opacity-20 text-white text-xs py-1 rounded">Чт</button>
                        <button onclick="toggleDay(4)" id="day4" class="day-btn bg-white bg-opacity-20 text-white text-xs py-1 rounded">Пт</button>
                        <button onclick="toggleDay(5)" id="day5" class="day-btn bg-white bg-opacity-20 text-white text-xs py-1 rounded">Сб</button>
                        <button onclick="toggleDay(6)" id="day6" class="day-btn bg-white bg-opacity-20 text-white text-xs py-1 rounded">Вс</button>
                    </div>
                </div>
                
                <button onclick="addSchedule()" 
                        class="w-full bg-green-500 hover:bg-green-600 text-white font-bold py-2 px-3 rounded-lg text-sm">
                    ➕ Добавить
                </button>
            </div>
            
            <!-- Schedule List -->
            <div class="mb-3">
                <h3 class="text-white font-semibold mb-2 text-sm">Активные расписания</h3>
                <div id="scheduleList" class="space-y-2">
                    <!-- Schedules will be loaded here -->
                </div>
            </div>
            
            <button onclick="closeSchedule()" 
                    class="w-full bg-gray-500 hover:bg-gray-600 text-white font-bold py-2 px-3 rounded-lg text-sm">
                Закрыть
            </button>
        </div>
    </div>

    <script>
        // Названия режимов гирлянды
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

        let operationMode = 0;  // 0 = garland, 1 = backlight
        let currentModeId = 0;  // Currently active mode on device (garland)
        let editingModeId = null;  // Mode currently being edited (null if settings modal closed)
        let brightnessDebounceTimer = null;
        let ledCountDebounceTimer = null;
        let modeSettingsDebounceTimer = null;
        let modeSettingsCache = [];  // Cache for garland mode settings
        let currentTab = 'active';  // Current tab (active or archived)
        let selectedDays = 0x7F;  // Битовая маска выбранных дней (по умолчанию все дни)
        let schedulesCache = [];  // Cache for schedules
        let currentTimeInterval = null;  // Interval for updating current time
        
        // Backlight state
        let backlightState = {
            currentMode: 0,
            activeColorIndex: 0,
            modeSettings: [],
            colors: [],
            autoSwitch: {
                modeDelay: 0,
                modeRandom: false,
                colorDelay: 0,
                colorRandom: false
            }
        };
        let editingBacklightModeId = null;

        // Debug logging helper
        function debugLog(msg) {
            console.log('[DEBUG] ' + msg);
        }

        // API calls
        async function apiCall(endpoint, data = null) {
            try {
                const options = data ? {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                } : { method: 'GET' };
                
                const response = await fetch(endpoint, options);
                return await response.json();
            } catch (error) {
                console.error('API Error:', error);
                return null;
            }
        }

        // Update power toggle visual state
        function updatePowerToggleUI(isOn) {
            const checkbox = document.getElementById('powerToggle');
            const toggleBg = document.querySelector('.toggle-bg');
            checkbox.checked = isOn;
            if (isOn) {
                toggleBg.classList.remove('bg-gray-600');
                toggleBg.classList.add('bg-green-500');
            } else {
                toggleBg.classList.remove('bg-green-500');
                toggleBg.classList.add('bg-gray-600');
            }
        }

        // Toggle power
        async function togglePower() {
            const checkbox = document.getElementById('powerToggle');
            const newState = !checkbox.checked;
            updatePowerToggleUI(newState);
            await apiCall('/api/power', { on: newState });
        }

        // Update brightness UI only (no API call)
        function updateBrightnessUI(value) {
            const percent = Math.round((value / 255) * 100);
            document.getElementById('brightnessValue').textContent = percent + '%';
            document.getElementById('brightnessSlider').value = value;
        }

        // Update brightness with debounce (sends API call)
        function updateBrightness(value) {
            updateBrightnessUI(value);
            
            clearTimeout(brightnessDebounceTimer);
            brightnessDebounceTimer = setTimeout(async () => {
                debugLog('updateBrightness: sending API call with value=' + value);
                await apiCall('/api/brightness', { value: parseInt(value) });
            }, 300);
        }
        
        // ===== OPERATION MODE =====
        async function setOperationMode(mode) {
            operationMode = mode;
            updateOperationModeUI();
            await apiCall('/api/operation-mode', { mode: mode });
        }
        
        function updateOperationModeUI() {
            const garlandBtn = document.getElementById('opModeGarland');
            const backlightBtn = document.getElementById('opModeBacklight');
            const garlandPanel = document.getElementById('garlandPanel');
            const backlightPanel = document.getElementById('backlightPanel');
            
            if (operationMode === 0) {
                garlandBtn.classList.add('active');
                backlightBtn.classList.remove('active');
                garlandPanel.classList.remove('hidden');
                backlightPanel.classList.add('hidden');
            } else {
                garlandBtn.classList.remove('active');
                backlightBtn.classList.add('active');
                garlandPanel.classList.add('hidden');
                backlightPanel.classList.remove('hidden');
            }
        }

        // Update LED count with debounce
        function updateLEDCount(value) {
            clearTimeout(ledCountDebounceTimer);
            ledCountDebounceTimer = setTimeout(async () => {
                await apiCall('/api/leds', { count: parseInt(value) });
            }, 500);
        }

        // Select mode
        async function selectMode(modeId) {
            debugLog('selectMode: selecting mode ' + modeId + ' (was ' + currentModeId + ')');
            currentModeId = modeId;
            const result = await apiCall('/api/mode', { mode: modeId });
            debugLog('selectMode: API result: ' + JSON.stringify(result));
            updateModeCards();
        }

        // Update mode cards visual state
        function updateModeCards() {
            // Highlight active mode card
            document.querySelectorAll('.mode-card').forEach((card) => {
                const cardModeId = parseInt(card.getAttribute('data-mode-id'));
                if (cardModeId === currentModeId) {
                    card.classList.add('active');
                } else {
                    card.classList.remove('active');
                }
            });
            
            // Update current mode display
            const isCurrentModeArchived = modeSettingsCache[currentModeId]?.archived || false;
            
            // Count active or archived modes depending on current mode status
            let visibleModes = [];
            let currentPosition = 0;
            
            for (let i = 0; i < modeNames.length; i++) {
                const isArchived = modeSettingsCache[i]?.archived || false;
                if (isArchived === isCurrentModeArchived) {
                    visibleModes.push(i);
                    if (i === currentModeId) {
                        currentPosition = visibleModes.length;
                    }
                }
            }
            
            document.getElementById('currentModeName').textContent = modeNames[currentModeId];
            document.getElementById('currentModeIndex').textContent = `(${currentPosition}/${visibleModes.length})`;
        }

        // Helper function to convert RGB to hex
        function rgbToHex(r, g, b) {
            return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
        }

        // Helper function to convert hex to RGB
        function hexToRgb(hex) {
            const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
            return result ? {
                r: parseInt(result[1], 16),
                g: parseInt(result[2], 16),
                b: parseInt(result[3], 16)
            } : null;
        }

        // Open mode settings
        async function openModeSettings(modeId) {
            editingModeId = modeId;  // Set editing mode (NOT currentModeId!)
            debugLog('openModeSettings: editingModeId=' + editingModeId + ', currentModeId=' + currentModeId);
            document.getElementById('modeSettingsTitle').textContent = `${modeNames[modeId]} - Настройки`;
            
            // Load current settings from cache or server
            if (modeSettingsCache[modeId]) {
                const settings = modeSettingsCache[modeId];
                debugLog('Loading cached settings for mode ' + modeId + ': speed=' + settings.speed + ', scale=' + settings.scale + ', brightness=' + settings.brightness);
                document.getElementById('modeSpeed').value = settings.speed;
                document.getElementById('modeScale').value = settings.scale;
                document.getElementById('modeBrightness').value = settings.brightness;
                
                // Update archive button text
                const archiveBtn = document.getElementById('archiveButton');
                if (settings.archived) {
                    archiveBtn.textContent = '✅ Активировать';
                    archiveBtn.classList.remove('bg-orange-500', 'hover:bg-orange-600');
                    archiveBtn.classList.add('bg-green-500', 'hover:bg-green-600');
                } else {
                    archiveBtn.textContent = '📦 Архивировать';
                    archiveBtn.classList.remove('bg-green-500', 'hover:bg-green-600');
                    archiveBtn.classList.add('bg-orange-500', 'hover:bg-orange-600');
                }
            } else {
                debugLog('No cached settings for mode ' + modeId);
            }
            
            document.getElementById('modeSettingsModal').classList.remove('hidden');
            document.getElementById('modeSettingsModal').classList.add('flex');
        }

        function closeModeSettings() {
            debugLog('closeModeSettings: was editing mode ' + editingModeId);
            editingModeId = null;  // Clear editing mode
            document.getElementById('modeSettingsModal').classList.add('hidden');
            document.getElementById('modeSettingsModal').classList.remove('flex');
        }

        // Update mode settings with debounce (auto-apply)
        function updateModeSettings() {
            clearTimeout(modeSettingsDebounceTimer);
            modeSettingsDebounceTimer = setTimeout(async () => {
                // CRITICAL: Use editingModeId, NOT currentModeId!
                if (editingModeId === null) {
                    debugLog('updateModeSettings: editingModeId is null, aborting!');
                    return;
                }
                
                const speed = document.getElementById('modeSpeed').value;
                const scale = document.getElementById('modeScale').value;
                const brightness = document.getElementById('modeBrightness').value;
                
                debugLog('updateModeSettings: Sending settings for mode ' + editingModeId + ': speed=' + speed + ', scale=' + scale + ', brightness=' + brightness);
                
                const result = await apiCall('/api/mode/settings', {
                    modeId: editingModeId,
                    speed: parseInt(speed),
                    scale: parseInt(scale),
                    brightness: parseInt(brightness)
                });
                
                debugLog('updateModeSettings: API result: ' + JSON.stringify(result));
                
                // Update local cache
                if (result && result.success && modeSettingsCache[editingModeId]) {
                    modeSettingsCache[editingModeId].speed = parseInt(speed);
                    modeSettingsCache[editingModeId].scale = parseInt(scale);
                    modeSettingsCache[editingModeId].brightness = parseInt(brightness);
                    debugLog('updateModeSettings: Local cache updated for mode ' + editingModeId);
                }
            }, 300);
        }

        // Settings modal
        function openSettings() {
            document.getElementById('settingsModal').classList.remove('hidden');
            document.getElementById('settingsModal').classList.add('flex');
        }

        function closeSettings() {
            document.getElementById('settingsModal').classList.add('hidden');
            document.getElementById('settingsModal').classList.remove('flex');
        }

        async function saveSettings() {
            const delay = document.getElementById('autoSwitchDelay').value;
            const random = document.getElementById('randomOrder').checked;
            await apiCall('/api/auto-switch', {
                delay: parseInt(delay),
                random: random
            });
            closeSettings();
        }

        // Load state from ESP
        async function loadState() {
            debugLog('loadState: fetching state... (editingModeId=' + editingModeId + ')');
            const state = await apiCall('/api/state');
            if (state) {
                updatePowerToggleUI(state.power);
                document.getElementById('ledCount').value = state.numLeds;
                document.getElementById('autoSwitchDelay').value = state.autoSwitchDelay;
                document.getElementById('randomOrder').checked = state.randomOrder;
                
                const prevModeId = currentModeId;
                currentModeId = state.currentMode;
                if (prevModeId !== currentModeId) {
                    debugLog('loadState: currentModeId changed from ' + prevModeId + ' to ' + currentModeId);
                }
                
                // Cache mode settings
                if (state.modeSettings) {
                    modeSettingsCache = state.modeSettings;
                    debugLog('loadState: modeSettingsCache updated, ' + modeSettingsCache.length + ' modes');
                }
                
                // Update operation mode
                if (state.operationMode !== undefined) {
                    operationMode = state.operationMode;
                    updateOperationModeUI();
                }
                
                updateBrightnessUI(state.brightness);  // UI only, no API call!
                renderModes();
                updateModeCards();
            } else {
                debugLog('loadState: failed to fetch state');
            }
            
            // Also load backlight state
            await loadBacklightState();
        }
        
        // Load backlight state
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
            }
            
            // Load colors
            const colors = await apiCall('/api/backlight/colors');
            if (colors && colors.colors) {
                backlightState.colors = colors.colors;
                backlightState.activeColorIndex = colors.activeIndex;
                renderColorPalette();
            }
            
            // Update auto-switch input fields
            const modeDelayInput = document.getElementById('blModeDelay');
            const colorDelayInput = document.getElementById('blColorDelay');
            console.log('loadBacklightState: modeDelay=' + backlightState.autoSwitch.modeDelay + ', colorDelay=' + backlightState.autoSwitch.colorDelay);
            console.log('loadBacklightState: modeDelayInput=' + modeDelayInput + ', colorDelayInput=' + colorDelayInput);
            if (modeDelayInput) modeDelayInput.value = backlightState.autoSwitch.modeDelay;
            if (colorDelayInput) colorDelayInput.value = backlightState.autoSwitch.colorDelay;
            
            renderBacklightModes();
        }

        // Switch between active and archived tabs
        function switchTab(tab) {
            currentTab = tab;
            const activeBtn = document.getElementById('activeTab');
            const archivedBtn = document.getElementById('archivedTab');
            
            if (tab === 'active') {
                activeBtn.classList.remove('bg-white', 'bg-opacity-20');
                activeBtn.classList.add('bg-green-500');
                archivedBtn.classList.remove('bg-green-500');
                archivedBtn.classList.add('bg-white', 'bg-opacity-20');
            } else {
                archivedBtn.classList.remove('bg-white', 'bg-opacity-20');
                archivedBtn.classList.add('bg-green-500');
                activeBtn.classList.remove('bg-green-500');
                activeBtn.classList.add('bg-white', 'bg-opacity-20');
            }
            
            renderModes();
        }
        
        // Toggle archive status from settings modal
        async function toggleArchiveFromSettings() {
            if (editingModeId === null) {
                debugLog('toggleArchiveFromSettings: editingModeId is null, aborting!');
                return;
            }
            const modeToArchive = editingModeId;  // Save before async call
            const isArchived = modeSettingsCache[modeToArchive]?.archived || false;
            debugLog('toggleArchiveFromSettings: mode=' + modeToArchive + ', isArchived=' + isArchived + ' -> ' + !isArchived);
            
            await apiCall('/api/mode/archive', {
                modeId: modeToArchive,
                archived: !isArchived
            });
            // Reload state to update cache and UI
            await loadState();
            // Close modal if mode was archived and we're on active tab
            if (!isArchived && currentTab === 'active') {
                closeModeSettings();
            } else if (isArchived && currentTab === 'archived') {
                closeModeSettings();
            } else {
                // Reopen to update button text
                openModeSettings(modeToArchive);
            }
        }
        
        // Reset mode settings to defaults
        async function resetModeSettings() {
            if (editingModeId === null) {
                debugLog('resetModeSettings: editingModeId is null, aborting!');
                return;
            }
            const modeToReset = editingModeId;  // Save before async call
            debugLog('resetModeSettings: resetting mode ' + modeToReset);
            
            await apiCall('/api/mode/reset', {
                modeId: modeToReset
            });
            // Reload state to update cache
            await loadState();
            // Reopen modal to show updated values
            openModeSettings(modeToReset);
        }
        
        // Render modes based on current tab
        function renderModes() {
            const grid = document.getElementById('modesGrid');
            grid.innerHTML = '';
            
            modeNames.forEach((name, index) => {
                const isArchived = modeSettingsCache[index]?.archived || false;
                
                // Filter based on current tab
                if ((currentTab === 'active' && isArchived) || (currentTab === 'archived' && !isArchived)) {
                    return;
                }
                
                const card = document.createElement('div');
                card.className = 'mode-card glass rounded-xl p-2 cursor-pointer';
                card.setAttribute('data-mode-id', index); // Store actual mode ID
                
                card.innerHTML = `
                    <div class="text-white font-bold mb-2 text-sm">${name}</div>
                    <div class="flex gap-1">
                        <button onclick="selectMode(${index})" 
                                class="flex-1 bg-green-500 hover:bg-green-600 text-white text-xs py-1 px-1 rounded">
                            Выбрать
                        </button>
                        <button onclick="openModeSettings(${index}); event.stopPropagation();" 
                                class="bg-blue-500 hover:bg-blue-600 text-white text-xs py-1 px-2 rounded">
                            ⚙️
                        </button>
                    </div>
                `;
                grid.appendChild(card);
            });
            
            // Update the current mode display after rendering cards
            updateModeCards();
        }
        
        // ===== BACKLIGHT FUNCTIONS =====
        
        // Color editing state
        let editingColorIndex = null;  // null = adding new, number = editing existing
        let colorPreviewTimer = null;
        
        // Render color palette
        function renderColorPalette() {
            const palette = document.getElementById('colorPalette');
            palette.innerHTML = '';
            
            backlightState.colors.forEach((colorObj, index) => {
                if (!colorObj.enabled) return;
                
                // Container for swatch + buttons
                const container = document.createElement('div');
                container.className = 'relative group';
                
                const swatch = document.createElement('div');
                swatch.className = 'color-swatch';
                swatch.style.backgroundColor = colorObj.hex;
                if (index === backlightState.activeColorIndex) {
                    swatch.classList.add('active');
                }
                
                // Click to select color
                swatch.onclick = () => selectBacklightColor(index);
                
                // Double-click to edit color
                swatch.ondblclick = (e) => {
                    e.stopPropagation();
                    openColorEditor(index, colorObj.hex);
                };
                
                // Delete button
                const deleteBtn = document.createElement('button');
                deleteBtn.className = 'absolute -top-1 -right-1 w-4 h-4 bg-red-500 text-white rounded-full text-xs leading-none flex items-center justify-center opacity-70 hover:opacity-100';
                deleteBtn.innerHTML = '×';
                deleteBtn.onclick = (e) => {
                    e.stopPropagation();
                    deleteBacklightColor(index);
                };
                
                // Edit button
                const editBtn = document.createElement('button');
                editBtn.className = 'absolute -top-1 -left-1 w-4 h-4 bg-blue-500 text-white rounded-full text-xs leading-none flex items-center justify-center opacity-70 hover:opacity-100';
                editBtn.innerHTML = '✎';
                editBtn.onclick = (e) => {
                    e.stopPropagation();
                    openColorEditor(index, colorObj.hex);
                };
                
                container.appendChild(swatch);
                container.appendChild(deleteBtn);
                container.appendChild(editBtn);
                palette.appendChild(container);
            });
        }
        
        // Select backlight color
        async function selectBacklightColor(index) {
            backlightState.activeColorIndex = index;
            renderColorPalette();
            await apiCall('/api/backlight/active-color', { index: index });
        }
        
        // Delete backlight color
        async function deleteBacklightColor(index) {
            if (!confirm('Удалить этот цвет?')) return;
            
            const result = await fetch(`/api/backlight/colors?index=${index}`, { method: 'DELETE' });
            if (result.ok) {
                await loadBacklightState();
            }
        }
        
        // Open color picker modal (for adding new color)
        function openColorPicker() {
            editingColorIndex = null;
            document.getElementById('colorPickerTitle').textContent = '🎨 Добавить цвет';
            document.getElementById('colorSaveBtn').textContent = '➕ Добавить';
            document.getElementById('newColorPicker').value = '#ff9966';
            document.getElementById('colorPickerModal').classList.remove('hidden');
            document.getElementById('colorPickerModal').classList.add('flex');
        }
        
        // Open color editor (for editing existing color)
        function openColorEditor(index, currentHex) {
            editingColorIndex = index;
            document.getElementById('colorPickerTitle').textContent = '🎨 Редактировать цвет';
            document.getElementById('colorSaveBtn').textContent = '💾 Сохранить';
            document.getElementById('newColorPicker').value = currentHex;
            document.getElementById('colorPickerModal').classList.remove('hidden');
            document.getElementById('colorPickerModal').classList.add('flex');
            
            // Select this color as active so changes are visible immediately
            if (backlightState.activeColorIndex !== index) {
                selectBacklightColor(index);
            }
        }
        
        function closeColorPicker() {
            editingColorIndex = null;
            clearTimeout(colorPreviewTimer);
            document.getElementById('colorPickerModal').classList.add('hidden');
            document.getElementById('colorPickerModal').classList.remove('flex');
        }
        
        // Preview color change with debounce (auto-apply while editing)
        function previewColorChange() {
            if (editingColorIndex === null) return;  // Only for editing mode
            
            clearTimeout(colorPreviewTimer);
            colorPreviewTimer = setTimeout(async () => {
                const hex = document.getElementById('newColorPicker').value;
                const rgb = hexToRgb(hex);
                if (rgb) {
                    await apiCall('/api/backlight/colors', { 
                        index: editingColorIndex, 
                        r: rgb.r, 
                        g: rgb.g, 
                        b: rgb.b 
                    });
                    // Update local state
                    if (backlightState.colors[editingColorIndex]) {
                        backlightState.colors[editingColorIndex].hex = hex;
                    }
                    renderColorPalette();
                }
            }, 500);  // 500ms debounce
        }
        
        // Save color (add new or save edited)
        async function saveColor() {
            const hex = document.getElementById('newColorPicker').value;
            const rgb = hexToRgb(hex);
            if (!rgb) return;
            
            if (editingColorIndex !== null) {
                // Editing existing - just close (changes already applied via preview)
                closeColorPicker();
                await loadBacklightState();  // Refresh to make sure we have latest
            } else {
                // Adding new color
                const result = await apiCall('/api/backlight/colors', { r: rgb.r, g: rgb.g, b: rgb.b });
                if (result && result.success) {
                    closeColorPicker();
                    await loadBacklightState();
                } else if (result && result.error) {
                    alert(result.error);
                }
            }
        }
        
        // Update backlight auto-switch settings
        async function updateBacklightAutoSwitch() {
            const modeDelayEl = document.getElementById('blModeDelay');
            const colorDelayEl = document.getElementById('blColorDelay');
            
            if (!modeDelayEl || !colorDelayEl) {
                console.error('Auto-switch inputs not found!');
                return;
            }
            
            const modeDelay = parseInt(modeDelayEl.value) || 0;
            const colorDelay = parseInt(colorDelayEl.value) || 0;
            
            console.log('Saving auto-switch: modeDelay=' + modeDelay + ', colorDelay=' + colorDelay);
            
            const result = await apiCall('/api/backlight/auto-switch', { 
                modeDelay: modeDelay, 
                modeRandom: false,
                colorDelay: colorDelay, 
                colorRandom: false 
            });
            
            console.log('Auto-switch API result:', result);
            
            // Update local state
            backlightState.autoSwitch.modeDelay = modeDelay;
            backlightState.autoSwitch.colorDelay = colorDelay;
        }
        
        // Render backlight modes
        function renderBacklightModes() {
            const grid = document.getElementById('backlightModesGrid');
            grid.innerHTML = '';
            
            backlightModeNames.forEach((name, index) => {
                const card = document.createElement('div');
                card.className = 'mode-card glass rounded-xl p-2 cursor-pointer';
                if (index === backlightState.currentMode) {
                    card.classList.add('active');
                }
                
                card.innerHTML = `
                    <div class="text-white font-bold mb-2 text-sm">${name}</div>
                    <div class="flex gap-1">
                        <button onclick="selectBacklightMode(${index})" 
                                class="flex-1 bg-green-500 hover:bg-green-600 text-white text-xs py-1 px-1 rounded">
                            Выбрать
                        </button>
                        <button onclick="openBacklightModeSettings(${index}); event.stopPropagation();" 
                                class="bg-blue-500 hover:bg-blue-600 text-white text-xs py-1 px-2 rounded">
                            ⚙️
                        </button>
                    </div>
                `;
                grid.appendChild(card);
            });
        }
        
        // Select backlight mode
        async function selectBacklightMode(modeId) {
            backlightState.currentMode = modeId;
            renderBacklightModes();
            await apiCall('/api/backlight/mode', { mode: modeId });
        }
        
        // Open backlight mode settings
        function openBacklightModeSettings(modeId) {
            editingBacklightModeId = modeId;
            document.getElementById('blModeSettingsTitle').textContent = `${backlightModeNames[modeId]} - Настройки`;
            
            if (backlightState.modeSettings[modeId]) {
                const settings = backlightState.modeSettings[modeId];
                document.getElementById('blModeSpeed').value = settings.speed;
                document.getElementById('blModeIntensity').value = settings.intensity;
                document.getElementById('blModeBrightness').value = settings.brightness;
            }
            
            document.getElementById('blModeSettingsModal').classList.remove('hidden');
            document.getElementById('blModeSettingsModal').classList.add('flex');
        }
        
        function closeBacklightModeSettings() {
            editingBacklightModeId = null;
            document.getElementById('blModeSettingsModal').classList.add('hidden');
            document.getElementById('blModeSettingsModal').classList.remove('flex');
        }
        
        // Update backlight mode settings
        let blModeSettingsTimer = null;
        function updateBacklightModeSettings() {
            clearTimeout(blModeSettingsTimer);
            blModeSettingsTimer = setTimeout(async () => {
                if (editingBacklightModeId === null) return;
                
                const speed = document.getElementById('blModeSpeed').value;
                const intensity = document.getElementById('blModeIntensity').value;
                const brightness = document.getElementById('blModeBrightness').value;
                
                await apiCall('/api/backlight/mode/settings', {
                    modeId: editingBacklightModeId,
                    speed: parseInt(speed),
                    intensity: parseInt(intensity),
                    brightness: parseInt(brightness)
                });
                
                // Update cache
                if (backlightState.modeSettings[editingBacklightModeId]) {
                    backlightState.modeSettings[editingBacklightModeId].speed = parseInt(speed);
                    backlightState.modeSettings[editingBacklightModeId].intensity = parseInt(intensity);
                    backlightState.modeSettings[editingBacklightModeId].brightness = parseInt(brightness);
                }
            }, 300);
        }

        // Schedule functions
        function openSchedule() {
            document.getElementById('scheduleModal').classList.remove('hidden');
            document.getElementById('scheduleModal').classList.add('flex');
            loadSchedules();
            updateCurrentTime();
            // Update time every second while modal is open
            currentTimeInterval = setInterval(updateCurrentTime, 1000);
            // Reset day selection to all days
            selectedDays = 0x7F;
            updateDayButtons();
        }

        function closeSchedule() {
            document.getElementById('scheduleModal').classList.add('hidden');
            document.getElementById('scheduleModal').classList.remove('flex');
            if (currentTimeInterval) {
                clearInterval(currentTimeInterval);
                currentTimeInterval = null;
            }
        }

        function toggleDay(dayIndex) {
            // Toggle bit for this day
            selectedDays ^= (1 << dayIndex);
            updateDayButtons();
        }

        function updateDayButtons() {
            for (let i = 0; i < 7; i++) {
                const btn = document.getElementById(`day${i}`);
                if (selectedDays & (1 << i)) {
                    btn.classList.remove('bg-white', 'bg-opacity-20');
                    btn.classList.add('bg-green-500');
                } else {
                    btn.classList.remove('bg-green-500');
                    btn.classList.add('bg-white', 'bg-opacity-20');
                }
            }
        }

        async function updateCurrentTime() {
            const time = await apiCall('/api/time');
            if (time) {
                const hour = String(time.hour).padStart(2, '0');
                const minute = String(time.minute).padStart(2, '0');
                document.getElementById('currentTime').textContent = `${hour}:${minute}`;
                
                const days = ['Воскресенье', 'Понедельник', 'Вторник', 'Среда', 'Четверг', 'Пятница', 'Суббота'];
                document.getElementById('currentDay').textContent = days[time.dayOfWeek];
            }
        }

        async function loadSchedules() {
            const data = await apiCall('/api/schedules');
            if (data && data.schedules) {
                schedulesCache = data.schedules;
                renderSchedules();
            }
        }

        function renderSchedules() {
            const list = document.getElementById('scheduleList');
            list.innerHTML = '';
            
            const activeSchedules = schedulesCache.filter(s => s.enabled);
            
            if (activeSchedules.length === 0) {
                list.innerHTML = '<div class="text-white text-sm opacity-75 text-center py-2">Нет активных расписаний</div>';
                return;
            }
            
            activeSchedules.forEach(schedule => {
                const hour = String(schedule.hour).padStart(2, '0');
                const minute = String(schedule.minute).padStart(2, '0');
                const action = schedule.action ? '🟢 Включить' : '🔴 Выключить';
                
                // Decode days
                const dayNames = ['Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб', 'Вс'];
                const activeDays = [];
                for (let i = 0; i < 7; i++) {
                    if (schedule.daysOfWeek & (1 << i)) {
                        activeDays.push(dayNames[i]);
                    }
                }
                const daysStr = activeDays.length === 7 ? 'Каждый день' : activeDays.join(', ');
                
                const card = document.createElement('div');
                card.className = 'bg-white bg-opacity-10 rounded-lg p-2';
                card.innerHTML = `
                    <div class="flex justify-between items-start">
                        <div class="flex-1">
                            <div class="text-white font-bold text-sm">${hour}:${minute}</div>
                            <div class="text-white text-xs opacity-90">${action}</div>
                            <div class="text-white text-xs opacity-75">${daysStr}</div>
                        </div>
                        <button onclick="deleteSchedule(${schedule.id})" 
                                class="bg-red-500 hover:bg-red-600 text-white text-xs py-1 px-2 rounded">
                            🗑️
                        </button>
                    </div>
                `;
                list.appendChild(card);
            });
        }

        async function addSchedule() {
            const hour = parseInt(document.getElementById('scheduleHour').value);
            const minute = parseInt(document.getElementById('scheduleMinute').value);
            const action = document.getElementById('scheduleAction').value === 'true';
            
            // Validation
            if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
                alert('Неверное время! Часы: 0-23, Минуты: 0-59');
                return;
            }
            
            if (selectedDays === 0) {
                alert('Выберите хотя бы один день недели!');
                return;
            }
            
            // Find first available schedule slot
            let scheduleId = -1;
            for (let i = 0; i < schedulesCache.length; i++) {
                if (!schedulesCache[i].enabled) {
                    scheduleId = i;
                    break;
                }
            }
            
            if (scheduleId === -1) {
                alert('Достигнут лимит расписаний (максимум 10)');
                return;
            }
            
            const result = await apiCall('/api/schedules', {
                id: scheduleId,
                enabled: true,
                hour: hour,
                minute: minute,
                action: action,
                daysOfWeek: selectedDays
            });
            
            if (result && result.success) {
                await loadSchedules();
                // Reset form
                document.getElementById('scheduleHour').value = 8;
                document.getElementById('scheduleMinute').value = 0;
                document.getElementById('scheduleAction').value = 'true';
                selectedDays = 0x7F;
                updateDayButtons();
            }
        }

        async function deleteSchedule(id) {
            if (!confirm('Удалить это расписание?')) {
                return;
            }
            
            const result = await fetch(`/api/schedules?id=${id}`, { method: 'DELETE' });
            if (result.ok) {
                await loadSchedules();
            }
        }

        // Sync time from browser to ESP8266 (fallback for NTP issues)
        async function syncTimeFromBrowser() {
            const timestamp = Math.floor(Date.now() / 1000);  // Current Unix timestamp
            console.log('Syncing time from browser:', timestamp, new Date());
            
            const result = await apiCall('/api/time/set', { timestamp: timestamp });
            if (result && result.success) {
                console.log('✅ Time synced successfully');
            } else {
                console.log('⚠️ Failed to sync time');
            }
        }

        // WebSocket for logs
        let ws = null;
        let logsBuffer = [];
        let logsPaused = false;
        let maxLogsInBuffer = 200;

        function connectWebSocket() {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${protocol}//${window.location.host}/ws/logs`;
            
            ws = new WebSocket(wsUrl);
            
            ws.onopen = () => {
                console.log('WebSocket connected');
                updateWSStatus(true);
            };
            
            ws.onclose = () => {
                console.log('WebSocket disconnected');
                updateWSStatus(false);
                // Reconnect after 3 seconds
                setTimeout(connectWebSocket, 3000);
            };
            
            ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                updateWSStatus(false);
            };
            
            ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    
                    if (data.type === 'history') {
                        // Load history logs
                        data.logs.forEach(log => addLogToConsole(log.timestamp, log.message));
                    } else if (data.timestamp && data.message) {
                        // New log message
                        addLogToConsole(data.timestamp, data.message);
                    }
                } catch (e) {
                    console.error('Error parsing WebSocket message:', e);
                }
            };
        }

        function updateWSStatus(connected) {
            const statusDot = document.getElementById('wsStatus');
            const statusText = document.getElementById('wsStatusText');
            
            if (connected) {
                statusDot.className = 'inline-block w-2 h-2 rounded-full bg-green-500 mr-1';
                statusText.textContent = 'Подключено';
            } else {
                statusDot.className = 'inline-block w-2 h-2 rounded-full bg-red-500 mr-1';
                statusText.textContent = 'Отключено';
            }
        }

        function addLogToConsole(timestamp, message) {
            if (logsPaused) return;
            
            // Add to buffer
            logsBuffer.push({ timestamp, message });
            if (logsBuffer.length > maxLogsInBuffer) {
                logsBuffer.shift();
            }
            
            const console = document.getElementById('logsConsole');
            const logLine = document.createElement('div');
            logLine.className = 'log-line';
            
            // Escape HTML
            const escapedMsg = message
                .replace(/&/g, '&amp;')
                .replace(/</g, '&lt;')
                .replace(/>/g, '&gt;')
                .replace(/\\n/g, '<br>');
            
            logLine.innerHTML = `<span class="text-gray-500">[${timestamp}]</span> ${escapedMsg}`;
            console.appendChild(logLine);
            
            // Auto-scroll to bottom
            console.scrollTop = console.scrollHeight;
            
            // Limit console lines
            while (console.children.length > maxLogsInBuffer) {
                console.removeChild(console.firstChild);
            }
        }

        function clearLogs() {
            document.getElementById('logsConsole').innerHTML = '';
            logsBuffer = [];
        }

        function toggleLogsPause() {
            logsPaused = !logsPaused;
            const btn = document.getElementById('pauseLogsBtn');
            if (logsPaused) {
                btn.innerHTML = '▶️ Продолжить';
                btn.classList.remove('bg-yellow-500', 'hover:bg-yellow-600');
                btn.classList.add('bg-green-500', 'hover:bg-green-600');
            } else {
                btn.innerHTML = '⏸️ Пауза';
                btn.classList.remove('bg-green-500', 'hover:bg-green-600');
                btn.classList.add('bg-yellow-500', 'hover:bg-yellow-600');
            }
        }

        function downloadLogs() {
            const logsText = logsBuffer.map(log => `[${log.timestamp}] ${log.message}`).join('\n');
            const blob = new Blob([logsText], { type: 'text/plain' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `esp8266-logs-${new Date().toISOString().replace(/:/g, '-')}.txt`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
        }

        // Initialize on load
        window.addEventListener('DOMContentLoaded', () => {
            // Sync time from browser first
            syncTimeFromBrowser();
            
            loadState();
            // Refresh state every 5 seconds
            setInterval(loadState, 5000);
            
            // Connect WebSocket for logs
            connectWebSocket();
        });
    </script>
</body>
</html>
)rawliteral";

#endif
