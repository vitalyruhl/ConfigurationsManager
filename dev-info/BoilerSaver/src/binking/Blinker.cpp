#include "Blinker.h"

Blinker* Blinker::s_head = nullptr;

Blinker::Blinker(uint8_t pin, ActiveLevel level)
    : _pin(pin), _activeHigh(level == HIGH_ACTIVE) {
    pinMode(_pin, OUTPUT);
    writeLevel(false); // default OFF
    // register in intrusive list
    _next = s_head;
    s_head = this;
}

void Blinker::SetBlinkRate(uint16_t count, uint32_t frequencyMs) {
    if (count == 0) count = 1;
    if (frequencyMs < 2) frequencyMs = 2; // avoid zero/odd issues
    _defCount = count;
    _defFrequencyMs = frequencyMs;
}

void Blinker::blink() {
    blink(_defCount, _defFrequencyMs);
}

void Blinker::blink(uint16_t count) {
    blink(count, _defFrequencyMs);
}

void Blinker::blink(uint16_t count, uint32_t frequencyMs) {
    if (count == 0) count = 1;
    if (frequencyMs < 2) frequencyMs = 2;
    const uint32_t onMs = frequencyMs / 2;
    const uint32_t offMs = frequencyMs - onMs;
    startSequence(count, onMs, offMs, /*autoRepeat*/ false, /*gapMs*/ 0);
}

void Blinker::repeat(uint16_t count, uint32_t frequencyMs, uint32_t gapMs) {
    if (count == 0) count = 1;
    if (frequencyMs < 2) frequencyMs = 2;
    const uint32_t onMs = frequencyMs / 2;
    const uint32_t offMs = frequencyMs - onMs;
    startSequence(count, onMs, offMs, /*autoRepeat*/ true, gapMs);
}

void Blinker::Turn(bool on) {
    _forced = true;
    _forcedOn = on;
    _blinking = false;
    _autoRepeat = false;
    _inGap = false;
    writeLevel(on);
}

void Blinker::stop() {
    _blinking = false;
    _autoRepeat = false;
    _inGap = false;
    _forced = false;
    writeLevel(false);
}

void Blinker::loop() {
    const unsigned long now = millis();

    if (_forced) {
        // nothing to do until Turn() changes state
        return;
    }

    if (!_blinking) {
        // idle
        return;
    }

    if (_inGap) {
        if (now - _lastChangeMs >= _gapMs) {
            // restart next sequence
            _inGap = false;
            _pulsesDone = 0;
            _phaseOn = false;
            _lastChangeMs = now;
        }
        return;
    }

    if (_phaseOn) {
        if (now - _lastChangeMs >= _onMs) {
            // turn OFF phase
            _phaseOn = false;
            _lastChangeMs = now;
            writeLevel(false);
        }
        return;
    } else {
        // OFF phase
        if (_pulsesDone >= _targetCount) {
            // sequence complete
            if (_autoRepeat) {
                // enter gap before repeating
                _inGap = true;
                _lastChangeMs = now;
            } else {
                _blinking = false;
            }
            return;
        }

        // time to start next ON phase?
        if (now == _lastChangeMs || (now - _lastChangeMs) >= _offMs) {
            _phaseOn = true;
            _lastChangeMs = now;
            _pulsesDone++;
            writeLevel(true);
        }
        return;
    }
}

void Blinker::loopAll() {
    for (Blinker* it = s_head; it != nullptr; it = it->_next) {
        it->loop();
    }
}

void Blinker::writeLevel(bool onLogical) {
    // Translate logical ON/OFF to electrical level based on polarity
    const bool level = _activeHigh ? onLogical : !onLogical;
    digitalWrite(_pin, level ? HIGH : LOW);
}

void Blinker::startSequence(uint16_t count, uint32_t onMs, uint32_t offMs, bool autoRepeat, uint32_t gapMs) {
    _forced = false;
    _autoRepeat = autoRepeat;
    _gapMs = gapMs;
    _targetCount = count;
    _pulsesDone = 0;
    _onMs = onMs;
    _offMs = offMs;
    _phaseOn = false; // start in OFF, will flip to ON on first tick
    _inGap = false;
    _blinking = true;
    _lastChangeMs = millis();
    writeLevel(false);
}
