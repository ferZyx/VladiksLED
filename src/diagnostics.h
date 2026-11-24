#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <Arduino.h>
#include "logger.h"

// Порог «подозрительно медленной» итерации loop() и отдельной стадии (мс)
#define SUSPICIOUS_LOOP_MS 50
#define SUSPICIOUS_TASK_MS 20
// Окно усреднения метрик (мс)
#define DIAG_WINDOW_MS 5000
// Не чаще одного предупреждения о медленной стадии за этот интервал (мс),
// чтобы логи сами не становились причиной тормозов
#define DIAG_WARN_INTERVAL_MS 10000

/**
 * Снимок метрик за последнее завершённое окно DIAG_WINDOW_MS и с момента загрузки.
 */
struct DiagSnapshot {
    uint32_t loopsPerSec;        // итераций loop() в секунду (окно)
    uint32_t maxLoopMsWindow;    // самая долгая итерация loop() за окно
    uint32_t maxLoopMsBoot;      // самая долгая итерация loop() с загрузки
    uint32_t ledFps;             // реально выведенных кадров в секунду (окно)
    uint32_t maxLedFrameMs;      // самый долгий кадр (эффект + show) за окно
    uint32_t minFreeHeap;        // минимум свободной кучи с загрузки
    uint32_t minMaxFreeBlock;    // минимум самого большого свободного блока с загрузки
    uint32_t eepromSaves;        // количество сохранений EEPROM с загрузки
    uint32_t lastSaveMs;         // длительность последнего сохранения EEPROM
    const char* slowestTask;     // стадия с самой долгой длительностью за окно
    uint32_t slowestTaskMs;      // её длительность
};

/**
 * Лёгкая диагностика главного цикла: тайминги стадий, FPS, минимум кучи.
 * Все вызовы — из loop() (однопоточно), блокировки не нужны.
 */
class Diagnostics {
private:
    unsigned long loopStartTime;
    unsigned long windowStart;
    unsigned long lastWarnTime;

    const char* taskName;
    unsigned long taskStartTime;

    // Накопители текущего окна
    uint32_t loopCount;
    uint32_t ledFrames;
    uint32_t maxLoopMs;
    uint32_t maxLedMs;
    const char* worstTask;
    uint32_t worstTaskMs;

    DiagSnapshot snap;

    void warn(const char* reason, unsigned long duration);
    void sampleHeap();

public:
    Diagnostics();

    void loopStart();
    void loopEnd();

    void taskStart(const char* name);
    void taskEnd();

    void recordLedFrame(uint32_t durationMs);
    void recordEepromSave(uint32_t durationMs);

    const DiagSnapshot& snapshot() const { return snap; }
};

extern Diagnostics diag;

#endif
