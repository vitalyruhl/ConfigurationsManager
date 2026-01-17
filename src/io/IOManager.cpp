#include "IOManager.h"

namespace cm {

void IOManager::addDigitalOutput(const DigitalOutputBinding& binding)
{
    if (!binding.id || !binding.id[0] || !binding.pin || !binding.activeLow) {
        CM_LOG("[IOManager][ERROR] addDigitalOutput: invalid binding");
        return;
    }

    if (findIndex(binding.id) >= 0) {
        CM_LOG("[IOManager][WARNING] addDigitalOutput: output '%s' already exists", binding.id);
        return;
    }

    DigitalOutputEntry entry;
    entry.id = binding.id;
    entry.pin = binding.pin;
    entry.activeLow = binding.activeLow;
    entry.enabled = binding.enabled;

    digitalOutputs.push_back(entry);
}

void IOManager::begin()
{
    for (auto& entry : digitalOutputs) {
        entry.desiredState = false;
        entry.hasLast = false;
        reconfigureIfNeeded(entry);
        applyDesiredState(entry);
    }
}

void IOManager::update()
{
    for (auto& entry : digitalOutputs) {
        reconfigureIfNeeded(entry);
        applyDesiredState(entry);
    }
}

bool IOManager::setState(const char* id, bool on)
{
    const int idx = findIndex(id);
    if (idx < 0) {
        CM_LOG("[IOManager][WARNING] setState: unknown output '%s'", id ? id : "(null)");
        return false;
    }

    DigitalOutputEntry& entry = digitalOutputs[static_cast<size_t>(idx)];
    entry.desiredState = on;

    reconfigureIfNeeded(entry);
    applyDesiredState(entry);
    return true;
}

bool IOManager::getState(const char* id) const
{
    const int idx = findIndex(id);
    if (idx < 0) {
        return false;
    }

    const DigitalOutputEntry& entry = digitalOutputs[static_cast<size_t>(idx)];
    if (!isEnabledNow(entry)) {
        return false;
    }

    const int pin = getPinNow(entry);
    if (!isValidPin(pin)) {
        return false;
    }

    const bool activeLow = isActiveLowNow(entry);
    const int val = digitalRead(pin);
    return activeLow ? (val == LOW) : (val == HIGH);
}

bool IOManager::isConfigured(const char* id) const
{
    const int idx = findIndex(id);
    if (idx < 0) {
        return false;
    }

    const DigitalOutputEntry& entry = digitalOutputs[static_cast<size_t>(idx)];
    return isValidPin(getPinNow(entry));
}

bool IOManager::isValidPin(int pin)
{
    return pin >= 0 && pin <= 39;
}

int IOManager::findIndex(const char* id) const
{
    if (!id || !id[0]) {
        return -1;
    }

    for (size_t i = 0; i < digitalOutputs.size(); i++) {
        if (digitalOutputs[i].id == id) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

bool IOManager::isEnabledNow(const DigitalOutputEntry& entry)
{
    return entry.enabled ? entry.enabled->get() : true;
}

bool IOManager::isActiveLowNow(const DigitalOutputEntry& entry)
{
    return entry.activeLow ? entry.activeLow->get() : true;
}

int IOManager::getPinNow(const DigitalOutputEntry& entry)
{
    return entry.pin ? entry.pin->get() : -1;
}

void IOManager::writePinState(int pin, bool activeLow, bool on)
{
    const int level = on ? (activeLow ? LOW : HIGH) : (activeLow ? HIGH : LOW);
    digitalWrite(pin, level);
}

void IOManager::applyDesiredState(DigitalOutputEntry& entry)
{
    const int pin = getPinNow(entry);
    if (!isValidPin(pin)) {
        return;
    }

    const bool enabled = isEnabledNow(entry);
    const bool activeLow = isActiveLowNow(entry);

    pinMode(pin, OUTPUT);
    writePinState(pin, activeLow, enabled ? entry.desiredState : false);
}

void IOManager::reconfigureIfNeeded(DigitalOutputEntry& entry)
{
    const int newPin = getPinNow(entry);
    const bool newActiveLow = isActiveLowNow(entry);
    const bool newEnabled = isEnabledNow(entry);

    if (!entry.hasLast) {
        entry.lastPin = newPin;
        entry.lastActiveLow = newActiveLow;
        entry.lastEnabled = newEnabled;
        entry.hasLast = true;
        return;
    }

    const bool pinChanged = (newPin != entry.lastPin);
    const bool polarityChanged = (newActiveLow != entry.lastActiveLow);
    const bool enabledChanged = (newEnabled != entry.lastEnabled);

    if (!pinChanged && !polarityChanged && !enabledChanged) {
        return;
    }

    if (pinChanged && isValidPin(entry.lastPin)) {
        // Best-effort: switch old pin to inactive state.
        pinMode(entry.lastPin, OUTPUT);
        writePinState(entry.lastPin, entry.lastActiveLow, false);
    }

    entry.lastPin = newPin;
    entry.lastActiveLow = newActiveLow;
    entry.lastEnabled = newEnabled;
}

} // namespace cm
