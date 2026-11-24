#include "diagnostics.h"

Diagnostics diag;

/**
 * Инициализирует счётчики; минимумы кучи стартуют с максимального значения.
 */
Diagnostics::Diagnostics()
    : loopStartTime(0), windowStart(0), lastWarnTime(0),
      taskName(nullptr), taskStartTime(0),
      loopCount(0), ledFrames(0), maxLoopMs(0), maxLedMs(0),
      worstTask(nullptr), worstTaskMs(0) {
    memset(&snap, 0, sizeof(snap));
    snap.minFreeHeap = UINT32_MAX;
    snap.minMaxFreeBlock = UINT32_MAX;
    snap.slowestTask = "";
}

/**
 * Пишет предупреждение в лог не чаще DIAG_WARN_INTERVAL_MS.
 * @param reason   описание
 * @param duration длительность, мс
 */
void Diagnostics::warn(const char* reason, unsigned long duration) {
    unsigned long now = millis();
    if (lastWarnTime != 0 && now - lastWarnTime < DIAG_WARN_INTERVAL_MS) return;
    lastWarnTime = now;
    LOG_PRINTF("⚠️ [DIAG] %s (%lums), heap=%u\n", reason, duration, ESP.getFreeHeap());
}

/**
 * Обновляет минимумы свободной кучи и самого большого свободного блока.
 * Именно maxFreeBlock показывает, хватит ли памяти на очередной ответ/буфер.
 */
void Diagnostics::sampleHeap() {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    if (freeHeap < snap.minFreeHeap) snap.minFreeHeap = freeHeap;
    if (maxBlock < snap.minMaxFreeBlock) snap.minMaxFreeBlock = maxBlock;
}

/**
 * Отмечает начало итерации loop().
 */
void Diagnostics::loopStart() {
    loopStartTime = millis();
    if (windowStart == 0) windowStart = loopStartTime;
}

/**
 * Завершает итерацию loop(): обновляет максимумы и раз в окно публикует снимок.
 */
void Diagnostics::loopEnd() {
    unsigned long now = millis();
    uint32_t duration = now - loopStartTime;
    loopCount++;
    if (duration > maxLoopMs) maxLoopMs = duration;
    if (duration > snap.maxLoopMsBoot) snap.maxLoopMsBoot = duration;
    if (duration > SUSPICIOUS_LOOP_MS) warn("Loop too slow", duration);

    sampleHeap();

    // Окно закончилось — фиксируем метрики и обнуляем накопители
    unsigned long elapsed = now - windowStart;
    if (elapsed >= DIAG_WINDOW_MS) {
        snap.loopsPerSec = (uint32_t)((uint64_t)loopCount * 1000 / elapsed);
        snap.ledFps = (uint32_t)((uint64_t)ledFrames * 1000 / elapsed);
        snap.maxLoopMsWindow = maxLoopMs;
        snap.maxLedFrameMs = maxLedMs;
        snap.slowestTask = worstTask ? worstTask : "";
        snap.slowestTaskMs = worstTaskMs;

        windowStart = now;
        loopCount = 0;
        ledFrames = 0;
        maxLoopMs = 0;
        maxLedMs = 0;
        worstTask = nullptr;
        worstTaskMs = 0;
    }
}

/**
 * Начало измеряемой стадии цикла (без вложенности).
 * @param name статическая строка с именем стадии
 */
void Diagnostics::taskStart(const char* name) {
    taskName = name;
    taskStartTime = millis();
}

/**
 * Конец текущей стадии: учитываем её длительность в «самой медленной стадии» окна.
 */
void Diagnostics::taskEnd() {
    if (taskName == nullptr) return;
    uint32_t duration = millis() - taskStartTime;
    if (duration > worstTaskMs) {
        worstTaskMs = duration;
        worstTask = taskName;
    }
    if (duration > SUSPICIOUS_TASK_MS) {
        char msg[48];
        snprintf(msg, sizeof(msg), "Slow task: %s", taskName);
        warn(msg, duration);
    }
    taskName = nullptr;
}

/**
 * Учитывает выведенный кадр LED.
 * @param durationMs время расчёта эффекта + FastLED.show()
 */
void Diagnostics::recordLedFrame(uint32_t durationMs) {
    ledFrames++;
    if (durationMs > maxLedMs) maxLedMs = durationMs;
}

/**
 * Учитывает сохранение настроек в EEPROM.
 * @param durationMs длительность записи во флеш
 */
void Diagnostics::recordEepromSave(uint32_t durationMs) {
    snap.eepromSaves++;
    snap.lastSaveMs = durationMs;
}
