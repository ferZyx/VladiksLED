#include "diagnostics.h"

Diagnostics diag;

Diagnostics::Diagnostics() {
    loopStartTime = 0;
    lastLogTime = 0;
    frameCount = 0;
    currentTask = {nullptr, 0, 0, 0, 0};
}

void Diagnostics::loopStart() {
    loopStartTime = millis();
    frameCount++;
}

void Diagnostics::loopEnd() {
    unsigned long duration = millis() - loopStartTime;
    
    if (duration > SUSPICIOUS_LOOP_MS) {
        logSuspicious("Loop too slow", duration);
    }
    
    // Periodic stats logging
    if (millis() - lastLogTime > LOG_INTERVAL_MS) {
        float fps = frameCount * 1000.0 / (millis() - lastLogTime);
        // LOG_PRINT("FPS: ");
        // LOG_PRINTLN(String(fps));
        
        lastLogTime = millis();
        frameCount = 0;
    }
}

void Diagnostics::taskStart(const char* name) {
    if (currentTask.name != nullptr) {
        // If a task is already running, we might be nesting or forgot to close one.
        // For simplicity, let's just warn or handle it gracefully.
        // But for this simple implementation, assume single-threaded, non-nested for big blocks.
    }
    currentTask.name = name;
    currentTask.startTime = millis();
}

void Diagnostics::taskEnd() {
    if (currentTask.name == nullptr) return;
    
    unsigned long duration = millis() - currentTask.startTime;
    
    // Detect slow tasks individually
    if (duration > 20) { // 20ms is quite long for a single task like "WiFi" or "LEDs"
        String msg = "Slow task: ";
        msg += currentTask.name;
        logSuspicious(msg.c_str(), duration);
    }
    
    currentTask.name = nullptr;
}

void Diagnostics::logSuspicious(const char* reason, unsigned long duration) {
    String msg = "⚠️ [DIAG] ";
    msg += reason;
    msg += " (";
    msg += duration;
    msg += "ms)";
    LOG_PRINTLN(msg);
}

void Diagnostics::printStats() {
    // Implement if we want to print accumulated stats on demand
}
