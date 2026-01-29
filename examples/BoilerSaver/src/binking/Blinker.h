#pragma once

#include <Arduino.h>

// Lightweight non-blocking blinker for LEDs, buzzers, etc.
// Supports one-shot and repeating blink sequences with a simple loop() call.
class Blinker {
public:
    // Active level for the GPIO (some boards have active-low LEDs/buzzers)
    enum ActiveLevel { LOW_ACTIVE = 0, HIGH_ACTIVE = 1 };

    // Construct a blinker on a given pin and active polarity.
    Blinker(uint8_t pin, ActiveLevel level = HIGH_ACTIVE);

    // Set default blink rate for subsequent blink() calls.
    // count: number of pulses in one sequence; frequencyMs: full period per pulse (on+off).
    void SetBlinkRate(uint16_t count = 1, uint32_t frequencyMs = 500);

    // Trigger a blink sequence using defaults.
    void blink();
    // Trigger a blink sequence overriding count only (frequency uses current default).
    void blink(uint16_t count);
    // Trigger a blink sequence overriding count and frequency.
    void blink(uint16_t count, uint32_t frequencyMs);

    // Immediately turn the output ON/OFF and cancel any running sequence.
    void Turn(bool on);

    // Start or update a repeating blink pattern: performs `count` pulses and then
    // waits `gapMs` before starting again.
    // frequencyMs is the full pulse period (on+off); on/off are split evenly.
    void repeat(uint16_t count, uint32_t frequencyMs, uint32_t gapMs = 0);

    // Stop any ongoing blink/repeat and leave output OFF.
    void stop();

    // Non-blocking state machine step. Call regularly from loop().
    void loop();

    // Convenience: service all created Blinker instances.
    static void loopAll();

private:
    void writeLevel(bool onLogical);
    void startSequence(uint16_t count, uint32_t onMs, uint32_t offMs, bool autoRepeat, uint32_t gapMs);

    uint8_t _pin;
    bool _activeHigh;

    // default config
    uint16_t _defCount = 1;
    uint32_t _defFrequencyMs = 500; // on+off period

    // runtime state
    bool _forced = false;    // if true, forced on/off and no blinking
    bool _forcedOn = false;

    bool _blinking = false;  // currently running a sequence
    bool _phaseOn = false;   // current phase inside a pulse
    uint16_t _targetCount = 0;
    uint16_t _pulsesDone = 0;
    uint32_t _onMs = 0;
    uint32_t _offMs = 0;
    uint32_t _gapMs = 0;     // extra idle gap after a sequence when repeating
    bool _autoRepeat = false;
    unsigned long _lastChangeMs = 0;
    bool _inGap = false;     // true when idling in repeat gap

    // intrusive list to allow loopAll()
    static Blinker* s_head;
    Blinker* _next = nullptr;
};
