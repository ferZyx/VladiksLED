#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <Arduino.h>
#include "logger.h"

// Thresholds for suspicious behavior
#define SUSPICIOUS_LOOP_MS 50
#define LOG_INTERVAL_MS 5000

struct TaskStats {
    const char* name;
    unsigned long startTime;
    unsigned long maxDuration;
    unsigned long totalDuration;
    unsigned long count;
};

class Diagnostics {
private:
    unsigned long loopStartTime;
    unsigned long lastLogTime;
    unsigned long frameCount;
    
    // Track specific tasks
    TaskStats currentTask;
    
    void logSuspicious(const char* reason, unsigned long duration);

public:
    Diagnostics();
    
    void loopStart();
    void loopEnd();
    
    void taskStart(const char* name);
    void taskEnd();  // Ends the currently running task
    
    void printStats();
};

extern Diagnostics diag;

#endif
