#pragma once

#include <Arduino.h>

namespace cm::helpers {

// Utility functions shared across examples and modules.
float mapFloat(float value, float inMin, float inMax, float outMin, float outMax);
float computeDewPoint(float temperatureC, float relHumidityPct);

// Generic pulse/blink helper with non-blocking and blocking APIs.
class PulseOutput {
public:
    enum class ActiveLevel : uint8_t { ActiveLow = 0, ActiveHigh = 1 };

    PulseOutput(uint8_t pin, ActiveLevel level = ActiveLevel::ActiveHigh);

    // Configure defaults for subsequent setPulse() calls.
    void setDefault(uint16_t count, uint32_t periodMs);

    // One-shot non-blocking pulse sequence using defaults.
    void setPulse();
    // Non-blocking one-shot with custom count and optional period.
    void setPulse(uint16_t count);
    void setPulse(uint16_t count, uint32_t periodMs);

    // Repeating non-blocking pattern: perform count pulses, then wait gapMs, repeat.
    void setPulseRepeat(uint16_t count, uint32_t periodMs, uint32_t gapMs = 0);

    // Blocking helper: immediately pulse count times with given period (on/off 50/50).
    void setPulseWait(uint16_t count, uint32_t periodMs);

    // Force output on/off and cancel active patterns.
    void force(bool on);
    // Stop any running pattern and turn output off.
    void stop();

    // Drive state machine (call regularly from loop()).
    void loop();
    static void loopAll();

private:
    void writeLevel(bool onLogical);
    void startSequence(uint16_t count, uint32_t onMs, uint32_t offMs, bool repeat, uint32_t gapMs);

    uint8_t pin_;
    bool activeHigh_;

    // defaults
    uint16_t defCount_ = 1;
    uint32_t defPeriodMs_ = 500;

    // runtime state
    bool forced_ = false;
    bool forcedOn_ = false;
    bool blinking_ = false;
    bool phaseOn_ = false;
    uint16_t targetCount_ = 0;
    uint16_t pulsesDone_ = 0;
    uint32_t onMs_ = 0;
    uint32_t offMs_ = 0;
    uint32_t gapMs_ = 0;
    bool autoRepeat_ = false;
    unsigned long lastChangeMs_ = 0;
    bool inGap_ = false;

    // intrusive list to service all instances
    static PulseOutput* head_;
    PulseOutput* next_ = nullptr;
};

// Convenience wrapper for quick blocking pulses without keeping an instance.
void pulseWait(uint8_t pin, PulseOutput::ActiveLevel level, uint16_t count, uint32_t periodMs);

} // namespace cm::helpers
