#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>

const char WEBPAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>🎄 WiFi LED Garland</title>
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
    </style>
</head>
<body class="p-2">
    <div class="max-w-6xl mx-auto">
        <!-- Header -->
        <div class="glass rounded-2xl shadow-2xl p-3 mb-3">
            <!-- Title and Power Toggle on same line -->
            <div class="flex items-center justify-between mb-3">
                <h1 class="text-2xl md:text-3xl font-bold text-white">
                    🎄 WiFi LED Garland
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

            <!-- Settings Button -->
            <button onclick="openSettings()" 
                    class="w-full bg-blue-500 hover:bg-blue-600 text-white font-bold py-2 px-4 rounded-xl transition duration-300">
                ⚙️ Настройки
            </button>
        </div>

        <!-- Modes Grid -->
        <div class="glass rounded-2xl shadow-2xl p-3">
            <div class="flex items-center justify-between mb-3">
                <h2 class="text-xl font-bold text-white">🎨 Режимы свечения</h2>
            </div>
            <div class="mb-3 p-2 bg-white bg-opacity-10 rounded-xl">
                <span class="text-white text-sm font-semibold">Активный режим: </span>
                <span class="text-white text-sm" id="currentModeName">-</span>
                <span class="text-white text-xs opacity-75 ml-1" id="currentModeIndex">(0/41)</span>
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
    </div>

    <!-- Settings Modal -->
    <div id="settingsModal" class="fixed inset-0 bg-black bg-opacity-50 hidden items-center justify-center z-50">
        <div class="glass rounded-2xl p-6 m-4 max-w-md w-full">
            <h2 class="text-2xl font-bold text-white mb-4">⚙️ Настройки</h2>
            
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

    <!-- Mode Settings Modal -->
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

            <div class="flex gap-2">
                <button onclick="closeModeSettings()" 
                        class="w-full bg-gray-500 hover:bg-gray-600 text-white font-bold py-2 px-3 rounded-lg text-sm">
                    Закрыть
                </button>
            </div>
        </div>
    </div>

    <script>
        // Mode names
const modeNames = [
    "Blendwave", "Rainbow Beat", "Two Sin", "One Sin Pal", "Noise8 Pal",
    "Two Sin 2", "One Sin Pal 2", "Juggle Pal", "Matrix Pal", "Two Sin 3",
    "One Sin Pal 3", "Three Sin Pal", "Serendipitous", "One Sin Pal 4", "Two Sin 4",
    "Matrix Pal 2", "Noise8 Pal 2", "Plasma", "Two Sin 5", "Rainbow March",
    "Three Sin Pal 2", "Rainbow March 2", "Noise16 Pal", "One Sin Pal 5", "Plasma 2",
    "Confetti Pal", "Two Sin 6", "Matrix Pal 3", "One Sin Pal 6", "Confetti Pal 2",
    "Plasma 3", "Juggle Pal 2", "One Sin Pal 7", "Three Sin Pal 3", "Rainbow March 3",
    "Plasma 4", "Confetti Pal 3", "Noise16 Pal 2", "Noise8 Pal 3", "Fire", "Candles"
];

        let currentModeId = 0;
        let brightnessDebounceTimer = null;
        let ledCountDebounceTimer = null;
        let modeSettingsDebounceTimer = null;
        let modeSettingsCache = [];  // Cache for mode settings
        let currentTab = 'active';  // Current tab (active or archived)

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

        // Update brightness with debounce
        function updateBrightness(value) {
            const percent = Math.round((value / 255) * 100);
            document.getElementById('brightnessValue').textContent = percent + '%';
            
            clearTimeout(brightnessDebounceTimer);
            brightnessDebounceTimer = setTimeout(async () => {
                await apiCall('/api/brightness', { value: parseInt(value) });
            }, 300);
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
            currentModeId = modeId;
            await apiCall('/api/mode', { mode: modeId });
            updateModeCards();
        }

        // Update mode cards visual state
        function updateModeCards() {
            document.querySelectorAll('.mode-card').forEach((card, index) => {
                if (index === currentModeId) {
                    card.classList.add('active');
                } else {
                    card.classList.remove('active');
                }
            });
            
            // Update current mode display
            document.getElementById('currentModeName').textContent = modeNames[currentModeId];
            document.getElementById('currentModeIndex').textContent = `(${currentModeId + 1}/${modeNames.length})`;
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
            currentModeId = modeId;
            document.getElementById('modeSettingsTitle').textContent = `${modeNames[modeId]} - Настройки`;
            
            // Load current settings from cache or server
            if (modeSettingsCache[modeId]) {
                const settings = modeSettingsCache[modeId];
                document.getElementById('modeSpeed').value = settings.speed;
                document.getElementById('modeScale').value = settings.scale;
                document.getElementById('modeBrightness').value = settings.brightness;
            }
            
            document.getElementById('modeSettingsModal').classList.remove('hidden');
            document.getElementById('modeSettingsModal').classList.add('flex');
        }

        function closeModeSettings() {
            document.getElementById('modeSettingsModal').classList.add('hidden');
            document.getElementById('modeSettingsModal').classList.remove('flex');
        }

        // Update mode settings with debounce (auto-apply)
        function updateModeSettings() {
            clearTimeout(modeSettingsDebounceTimer);
            modeSettingsDebounceTimer = setTimeout(async () => {
                const speed = document.getElementById('modeSpeed').value;
                const scale = document.getElementById('modeScale').value;
                const brightness = document.getElementById('modeBrightness').value;
                
                await apiCall('/api/mode/settings', {
                    modeId: currentModeId,
                    speed: parseInt(speed),
                    scale: parseInt(scale),
                    brightness: parseInt(brightness)
                });
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
            const state = await apiCall('/api/state');
            if (state) {
                updatePowerToggleUI(state.power);
                document.getElementById('brightnessSlider').value = state.brightness;
                document.getElementById('ledCount').value = state.numLeds;
                document.getElementById('autoSwitchDelay').value = state.autoSwitchDelay;
                document.getElementById('randomOrder').checked = state.randomOrder;
                currentModeId = state.currentMode;
                
                // Cache mode settings
                if (state.modeSettings) {
                    modeSettingsCache = state.modeSettings;
                }
                
                updateBrightness(state.brightness);
                renderModes();
                updateModeCards();
            }
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
        
        // Toggle archive status
        async function toggleArchive(modeId, event) {
            event.stopPropagation();
            const isArchived = modeSettingsCache[modeId]?.archived || false;
            await apiCall('/api/mode/archive', {
                modeId: modeId,
                archived: !isArchived
            });
            // Reload state to update cache
            await loadState();
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
                
                const archiveIcon = isArchived ? '✅' : '📦';
                const archiveText = isArchived ? 'Активировать' : 'Архивировать';
                
                card.innerHTML = `
                    <div class="text-white font-bold mb-2 text-sm">${name}</div>
                    <div class="flex gap-1 mb-1">
                        <button onclick="selectMode(${index})" 
                                class="flex-1 bg-green-500 hover:bg-green-600 text-white text-xs py-1 px-1 rounded">
                            Выбрать
                        </button>
                        <button onclick="openModeSettings(${index}); event.stopPropagation();" 
                                class="bg-blue-500 hover:bg-blue-600 text-white text-xs py-1 px-2 rounded">
                            ⚙️
                        </button>
                    </div>
                    <button onclick="toggleArchive(${index}, event)" 
                            class="w-full bg-orange-500 hover:bg-orange-600 text-white text-xs py-1 px-1 rounded">
                        ${archiveIcon} ${archiveText}
                    </button>
                `;
                grid.appendChild(card);
            });
        }

        // Initialize on load
        window.addEventListener('DOMContentLoaded', () => {
            loadState();
            // Refresh state every 5 seconds
            setInterval(loadState, 5000);
        });
    </script>
</body>
</html>
)rawliteral";

#endif
