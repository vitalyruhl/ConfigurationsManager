#include "IOManager.h"
#include "core/CoreSettings.h"

namespace cm {

static constexpr const char* IO_CATEGORY_PRETTY = "I/O";

String IOManager::formatSlotKey(uint8_t slot, char suffix)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "IO%02u%c", static_cast<unsigned>(slot), suffix);
    return String(buf);
}

void IOManager::addDigitalOutput(const DigitalOutputBinding& binding)
{
    if (!binding.id || !binding.id[0]) {
        CM_LOG("[IOManager][ERROR] addDigitalOutput: invalid binding");
        return;
    }

    if (findIndex(binding.id) >= 0) {
        CM_LOG("[IOManager][WARNING] addDigitalOutput: output '%s' already exists", binding.id);
        return;
    }

    DigitalOutputEntry entry;
    entry.id = binding.id;
    entry.name = binding.name ? binding.name : binding.id;

    entry.slot = nextDigitalOutputSlot;
    if (nextDigitalOutputSlot < 99) {
        nextDigitalOutputSlot++;
    } else {
        CM_LOG("[IOManager][WARNING] addDigitalOutput: exceeded slot range 00..99, keys may not remain stable");
        nextDigitalOutputSlot++;
    }

    entry.defaultPin = binding.defaultPin;
    entry.defaultActiveLow = binding.defaultActiveLow;
    entry.defaultEnabled = binding.defaultEnabled;

    entry.registerSettings = binding.registerSettings;

    entry.showPinInWeb = binding.showPinInWeb;
    entry.showActiveLowInWeb = binding.showActiveLowInWeb;

    digitalOutputs.push_back(std::move(entry));
}

void IOManager::addIOtoGUI(const char* id, const char* cardName, int order)
{
    const int idx = findIndex(id);
    if (idx < 0) {
        CM_LOG("[IOManager][WARNING] addIOtoGUI: unknown output '%s'", id ? id : "(null)");
        return;
    }

    DigitalOutputEntry& entry = digitalOutputs[static_cast<size_t>(idx)];
    if (entry.settingsRegistered) {
        CM_LOG("[IOManager][WARNING] addIOtoGUI: output '%s' already registered", entry.id.c_str());
        return;
    }

    if (!entry.registerSettings) {
        // Runtime-only / programmatic output: no Settings tab entries and no persistence.
        entry.settingsRegistered = true;
        return;
    }

    entry.cardKey = entry.id; // stable card key
    entry.cardPretty = (cardName && cardName[0]) ? String(cardName) : entry.name;
    entry.cardOrder = order;

    entry.keyPin = formatSlotKey(entry.slot, 'P');
    entry.keyActiveLow = formatSlotKey(entry.slot, 'L');

    entry.pin = std::make_unique<Config<int>>(ConfigOptions<int>{
        .key = entry.keyPin.c_str(),
        .name = "GPIO",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultPin,
        .showInWeb = entry.showPinInWeb,
        .sortOrder = 11,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKey.c_str(),
        .cardPretty = entry.cardPretty.c_str(),
        .cardOrder = entry.cardOrder,
    });

    entry.activeLow = std::make_unique<Config<bool>>(ConfigOptions<bool>{
        .key = entry.keyActiveLow.c_str(),
        .name = "Active LOW",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultActiveLow,
        .showInWeb = entry.showActiveLowInWeb,
        .sortOrder = 12,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKey.c_str(),
        .cardPretty = entry.cardPretty.c_str(),
        .cardOrder = entry.cardOrder,
    });

    ConfigManager.addSetting(entry.pin.get());
    ConfigManager.addSetting(entry.activeLow.get());

    entry.settingsRegistered = true;
}

void IOManager::addIOtoGUI(const char* id, const char* cardName, int order, RuntimeControlType type,
                           std::function<void()> onPress,
                           const char* runtimeLabel,
                           const char* runtimeGroup)
{
    addIOtoGUI(id, cardName, order);

    const int idx = findIndex(id);
    if (idx < 0) {
        return;
    }
    const DigitalOutputEntry& entry = digitalOutputs[static_cast<size_t>(idx)];

    if (type != RuntimeControlType::Button) {
        CM_LOG("[IOManager][WARNING] addIOtoGUI(runtime): output '%s' uses 1 callback but type is not Button", entry.id.c_str());
        return;
    }
    if (!onPress) {
        CM_LOG("[IOManager][WARNING] addIOtoGUI(runtime): output '%s' missing onPress callback", entry.id.c_str());
        return;
    }

    const String group = (runtimeGroup && runtimeGroup[0]) ? String(runtimeGroup) : String("controls");
    const String label = (runtimeLabel && runtimeLabel[0]) ? String(runtimeLabel) : entry.name;
    ConfigManager.defineRuntimeButton(group, entry.id, label, onPress, String(), order);
}

void IOManager::addIOtoGUI(const char* id, const char* cardName, int order, RuntimeControlType type,
                           std::function<bool()> getter,
                           std::function<void(bool)> setter,
                           const char* runtimeLabel,
                           const char* runtimeGroup,
                           const char* runtimeOnLabel,
                           const char* runtimeOffLabel)
{
    addIOtoGUI(id, cardName, order);

    const int idx = findIndex(id);
    if (idx < 0) {
        return;
    }
    const DigitalOutputEntry& entry = digitalOutputs[static_cast<size_t>(idx)];

    if (!getter || !setter) {
        CM_LOG("[IOManager][WARNING] addIOtoGUI(runtime): output '%s' missing getter/setter", entry.id.c_str());
        return;
    }

    const String group = (runtimeGroup && runtimeGroup[0]) ? String(runtimeGroup) : String("controls");
    const String label = (runtimeLabel && runtimeLabel[0]) ? String(runtimeLabel) : entry.name;
    const String onLabel = (runtimeOnLabel && runtimeOnLabel[0]) ? String(runtimeOnLabel) : String();
    const String offLabel = (runtimeOffLabel && runtimeOffLabel[0]) ? String(runtimeOffLabel) : String();

    switch (type) {
        case RuntimeControlType::Checkbox:
            ConfigManager.defineRuntimeCheckbox(group, entry.id, label, getter, setter, String(), order);
            break;
        case RuntimeControlType::MomentaryButton:
            ConfigManager.defineRuntimeMomentaryButton(group, entry.id, label, getter, setter, String(), order, onLabel, offLabel);
            break;
        case RuntimeControlType::StateButton:
            ConfigManager.defineRuntimeStateButton(group, entry.id, label, getter, setter, false, String(), order, onLabel, offLabel);
            break;
        default:
            CM_LOG("[IOManager][WARNING] addIOtoGUI(runtime): output '%s' uses 2 callbacks but type is Button", entry.id.c_str());
            break;
    }
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

bool IOManager::isActiveLowNow(const DigitalOutputEntry& entry)
{
    return entry.activeLow ? entry.activeLow->get() : entry.defaultActiveLow;
}

int IOManager::getPinNow(const DigitalOutputEntry& entry)
{
    return entry.pin ? entry.pin->get() : entry.defaultPin;
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
    const bool activeLow = isActiveLowNow(entry);

    pinMode(pin, OUTPUT);
    writePinState(pin, activeLow, entry.desiredState);
}

void IOManager::reconfigureIfNeeded(DigitalOutputEntry& entry)
{
    const int newPin = getPinNow(entry);
    const bool newActiveLow = isActiveLowNow(entry);

    if (!entry.hasLast) {
        entry.lastPin = newPin;
        entry.lastActiveLow = newActiveLow;
        entry.hasLast = true;
        return;
    }

    const bool pinChanged = (newPin != entry.lastPin);
    const bool polarityChanged = (newActiveLow != entry.lastActiveLow);

    if (!pinChanged && !polarityChanged) {
        return;
    }

    if (pinChanged && isValidPin(entry.lastPin)) {
        // Best-effort: switch old pin to inactive state.
        pinMode(entry.lastPin, OUTPUT);
        writePinState(entry.lastPin, entry.lastActiveLow, false);
    }

    entry.lastPin = newPin;
    entry.lastActiveLow = newActiveLow;
}

} // namespace cm
