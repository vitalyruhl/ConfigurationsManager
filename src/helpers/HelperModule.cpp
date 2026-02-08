#include "HelperModule.h"
#include <cmath>

namespace cm::helpers {

PulseOutput* PulseOutput::head_ = nullptr;

float mapFloat(float value, float inMin, float inMax, float outMin, float outMax)
{
    const float denom = (inMax - inMin);
    if (denom == 0.0f) {
        return outMin;
    }
    return (value - inMin) * (outMax - outMin) / denom + outMin;
}

float computeDewPoint(float temperatureC, float relHumidityPct)
{
    if (isnan(temperatureC) || isnan(relHumidityPct)) return NAN;
    if (relHumidityPct <= 0.0f) relHumidityPct = 0.1f;
    if (relHumidityPct > 100.0f) relHumidityPct = 100.0f;
    constexpr float a = 17.62f;
    constexpr float b = 243.12f;
    const float rh = relHumidityPct / 100.0f;
    const float gamma = (a * temperatureC) / (b + temperatureC) + log(rh);
    return (b * gamma) / (a - gamma);
}

PulseOutput::PulseOutput(uint8_t pin, ActiveLevel level)
    : pin_(pin), activeHigh_(level == ActiveLevel::ActiveHigh)
{
    pinMode(pin_, OUTPUT);
    writeLevel(false);
    // register instance
    next_ = head_;
    head_ = this;
}

void PulseOutput::setDefault(uint16_t count, uint32_t periodMs)
{
    if (count == 0) count = 1;
    if (periodMs < 2) periodMs = 2;
    defCount_ = count;
    defPeriodMs_ = periodMs;
}

void PulseOutput::setPulse()
{
    setPulse(defCount_, defPeriodMs_);
}

void PulseOutput::setPulse(uint16_t count)
{
    setPulse(count, defPeriodMs_);
}

void PulseOutput::setPulse(uint16_t count, uint32_t periodMs)
{
    if (count == 0) count = 1;
    if (periodMs < 2) periodMs = 2;
    const uint32_t onMs = periodMs / 2;
    const uint32_t offMs = periodMs - onMs;
    startSequence(count, onMs, offMs, false, 0);
}

void PulseOutput::setPulseRepeat(uint16_t count, uint32_t periodMs, uint32_t gapMs)
{
    if (count == 0) count = 1;
    if (periodMs < 2) periodMs = 2;
    const uint32_t onMs = periodMs / 2;
    const uint32_t offMs = periodMs - onMs;
    startSequence(count, onMs, offMs, true, gapMs);
}

void PulseOutput::setPulseWait(uint16_t count, uint32_t periodMs)
{
    if (count == 0) count = 1;
    if (periodMs < 2) periodMs = 2;
    const uint32_t onMs = periodMs / 2;
    const uint32_t offMs = periodMs - onMs;
    forced_ = false;
    blinking_ = false;
    for (uint16_t i = 0; i < count; ++i) {
        writeLevel(true);
        delay(onMs);
        writeLevel(false);
        delay(offMs);
    }
}

void PulseOutput::force(bool on)
{
    forced_ = true;
    forcedOn_ = on;
    blinking_ = false;
    autoRepeat_ = false;
    inGap_ = false;
    writeLevel(on);
}

void PulseOutput::stop()
{
    blinking_ = false;
    autoRepeat_ = false;
    inGap_ = false;
    forced_ = false;
    writeLevel(false);
}

void PulseOutput::loop()
{
    const unsigned long now = millis();

    if (forced_) {
        return;
    }
    if (!blinking_) {
        return;
    }
    if (inGap_) {
        if (now - lastChangeMs_ >= gapMs_) {
            inGap_ = false;
            pulsesDone_ = 0;
            phaseOn_ = false;
            lastChangeMs_ = now;
        }
        return;
    }
    if (phaseOn_) {
        if (now - lastChangeMs_ >= onMs_) {
            phaseOn_ = false;
            lastChangeMs_ = now;
            writeLevel(false);
        }
        return;
    }
    // OFF phase
    if (pulsesDone_ >= targetCount_) {
        if (autoRepeat_) {
            inGap_ = true;
            lastChangeMs_ = now;
        } else {
            blinking_ = false;
        }
        return;
    }

    if (now == lastChangeMs_ || (now - lastChangeMs_) >= offMs_) {
        phaseOn_ = true;
        lastChangeMs_ = now;
        ++pulsesDone_;
        writeLevel(true);
    }
}

void PulseOutput::loopAll()
{
    for (PulseOutput* it = head_; it != nullptr; it = it->next_) {
        it->loop();
    }
}

void PulseOutput::writeLevel(bool onLogical)
{
    const bool level = activeHigh_ ? onLogical : !onLogical;
    digitalWrite(pin_, level ? HIGH : LOW);
}

void PulseOutput::startSequence(uint16_t count, uint32_t onMs, uint32_t offMs, bool repeat, uint32_t gapMs)
{
    forced_ = false;
    autoRepeat_ = repeat;
    gapMs_ = gapMs;
    targetCount_ = count;
    pulsesDone_ = 0;
    onMs_ = onMs;
    offMs_ = offMs;
    phaseOn_ = false;
    inGap_ = false;
    blinking_ = true;
    lastChangeMs_ = millis();
    writeLevel(false);
}

void pulseWait(uint8_t pin, PulseOutput::ActiveLevel level, uint16_t count, uint32_t periodMs)
{
    if (count == 0) count = 1;
    if (periodMs < 2) periodMs = 2;
    const uint32_t onMs = periodMs / 2;
    const uint32_t offMs = periodMs - onMs;
    const bool activeHigh = (level == PulseOutput::ActiveLevel::ActiveHigh);

    pinMode(pin, OUTPUT);
    for (uint16_t i = 0; i < count; ++i) {
        digitalWrite(pin, activeHigh ? HIGH : LOW);
        delay(onMs);
        digitalWrite(pin, activeHigh ? LOW : HIGH);
        delay(offMs);
    }
}

} // namespace cm::helpers
