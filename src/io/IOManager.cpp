#include "IOManager.h"
#include "core/CoreSettings.h"

namespace cm {

static std::shared_ptr<std::string> makeStableString(const String& value)
{
    return std::make_shared<std::string>(value.c_str());
}

static constexpr const char* IO_CATEGORY_PRETTY = "I/O";

String IOManager::formatSlotKey(uint8_t slot, char suffix)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "IO%02u%c", static_cast<unsigned>(slot), suffix);
    return String(buf);
}

String IOManager::formatInputSlotKey(uint8_t slot, char suffix)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "II%02u%c", static_cast<unsigned>(slot), suffix);
    return String(buf);
}

String IOManager::formatAnalogSlotKey(uint8_t slot, char suffix)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "AI%02u%c", static_cast<unsigned>(slot), suffix);
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

void IOManager::addDigitalInput(const DigitalInputBinding& binding)
{
    if (!binding.id || !binding.id[0]) {
        CM_LOG("[IOManager][ERROR] addDigitalInput: invalid binding");
        return;
    }

    if (findInputIndex(binding.id) >= 0) {
        CM_LOG("[IOManager][WARNING] addDigitalInput: input '%s' already exists", binding.id);
        return;
    }

    DigitalInputEntry entry;
    entry.id = binding.id;
    entry.name = binding.name ? binding.name : binding.id;

    entry.slot = nextDigitalInputSlot;
    if (nextDigitalInputSlot < 99) {
        nextDigitalInputSlot++;
    } else {
        CM_LOG("[IOManager][WARNING] addDigitalInput: exceeded slot range 00..99, keys may not remain stable");
        nextDigitalInputSlot++;
    }

    entry.defaultPin = binding.defaultPin;
    entry.defaultActiveLow = binding.defaultActiveLow;
    entry.defaultPullup = binding.defaultPullup;
    entry.defaultPulldown = binding.defaultPulldown;
    entry.defaultEnabled = binding.defaultEnabled;

    entry.registerSettings = binding.registerSettings;

    entry.showPinInWeb = binding.showPinInWeb;
    entry.showActiveLowInWeb = binding.showActiveLowInWeb;
    entry.showPullupInWeb = binding.showPullupInWeb;
    entry.showPulldownInWeb = binding.showPulldownInWeb;

    digitalInputs.push_back(std::move(entry));
}

void IOManager::addAnalogInput(const AnalogInputBinding& binding)
{
    if (!binding.id || !binding.id[0]) {
        CM_LOG("[IOManager][ERROR] addAnalogInput: invalid binding");
        return;
    }

    if (findAnalogInputIndex(binding.id) >= 0) {
        CM_LOG("[IOManager][WARNING] addAnalogInput: input '%s' already exists", binding.id);
        return;
    }

    AnalogInputEntry entry;
    entry.id = binding.id;
    entry.name = binding.name ? binding.name : binding.id;

    entry.slot = nextAnalogInputSlot;
    if (nextAnalogInputSlot < 99) {
        nextAnalogInputSlot++;
    } else {
        CM_LOG("[IOManager][WARNING] addAnalogInput: exceeded slot range 00..99, keys may not remain stable");
        nextAnalogInputSlot++;
    }

    entry.defaultPin = binding.defaultPin;
    entry.defaultEnabled = binding.defaultEnabled;

    entry.defaultRawMin = binding.defaultRawMin;
    entry.defaultRawMax = binding.defaultRawMax;
    entry.defaultOutMin = binding.defaultOutMin;
    entry.defaultOutMax = binding.defaultOutMax;

    entry.defaultUnit = binding.defaultUnit ? String(binding.defaultUnit) : String();
    entry.defaultPrecision = binding.defaultPrecision;
    entry.defaultDeadband = binding.defaultDeadband;
    entry.defaultMinEventMs = binding.defaultMinEventMs;

    entry.registerSettings = binding.registerSettings;
    entry.showPinInWeb = binding.showPinInWeb;
    entry.showMappingInWeb = binding.showMappingInWeb;
    entry.showUnitInWeb = binding.showUnitInWeb;
    entry.showDeadbandInWeb = binding.showDeadbandInWeb;
    entry.showMinEventInWeb = binding.showMinEventInWeb;

    analogInputs.push_back(std::move(entry));
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

    entry.cardKeyStable = makeStableString(entry.cardKey);
    entry.cardPrettyStable = makeStableString(entry.cardPretty);

    entry.keyPin = formatSlotKey(entry.slot, 'P');
    entry.keyActiveLow = formatSlotKey(entry.slot, 'L');

    entry.keyPinStable = makeStableString(entry.keyPin);
    entry.keyActiveLowStable = makeStableString(entry.keyActiveLow);

    entry.pin = std::make_unique<Config<int>>(ConfigOptions<int>{
        .key = entry.keyPinStable->c_str(),
        .name = "GPIO",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultPin,
        .showInWeb = entry.showPinInWeb,
        .sortOrder = 11,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKeyStable->c_str(),
        .cardPretty = entry.cardPrettyStable->c_str(),
        .cardOrder = entry.cardOrder,
    });

    entry.activeLow = std::make_unique<Config<bool>>(ConfigOptions<bool>{
        .key = entry.keyActiveLowStable->c_str(),
        .name = "Active LOW",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultActiveLow,
        .showInWeb = entry.showActiveLowInWeb,
        .sortOrder = 12,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKeyStable->c_str(),
        .cardPretty = entry.cardPrettyStable->c_str(),
        .cardOrder = entry.cardOrder,
    });

    ConfigManager.addSetting(entry.pin.get());
    ConfigManager.addSetting(entry.activeLow.get());

    entry.settingsRegistered = true;
}

void IOManager::addInputToGUI(const char* id, const char* cardName, int order,
                              const char* runtimeLabel,
                              const char* runtimeGroup,
                              bool alarmWhenActive)
{
    const int idx = findInputIndex(id);
    if (idx < 0) {
        CM_LOG("[IOManager][WARNING] addInputToGUI: unknown input '%s'", id ? id : "(null)");
        return;
    }

    DigitalInputEntry& entry = digitalInputs[static_cast<size_t>(idx)];
    if (entry.settingsRegistered) {
        CM_LOG("[IOManager][WARNING] addInputToGUI: input '%s' already registered", entry.id.c_str());
        return;
    }

    if (!entry.registerSettings) {
        entry.settingsRegistered = true;
        return;
    }

    entry.cardKey = entry.id;
    entry.cardPretty = (cardName && cardName[0]) ? String(cardName) : entry.name;
    entry.cardOrder = order;

    entry.cardKeyStable = makeStableString(entry.cardKey);
    entry.cardPrettyStable = makeStableString(entry.cardPretty);

    entry.keyPin = formatInputSlotKey(entry.slot, 'P');
    entry.keyActiveLow = formatInputSlotKey(entry.slot, 'L');
    entry.keyPullup = formatInputSlotKey(entry.slot, 'U');
    entry.keyPulldown = formatInputSlotKey(entry.slot, 'D');

    entry.keyPinStable = makeStableString(entry.keyPin);
    entry.keyActiveLowStable = makeStableString(entry.keyActiveLow);
    entry.keyPullupStable = makeStableString(entry.keyPullup);
    entry.keyPulldownStable = makeStableString(entry.keyPulldown);

    entry.pin = std::make_unique<Config<int>>(ConfigOptions<int>{
        .key = entry.keyPinStable->c_str(),
        .name = "GPIO",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultPin,
        .showInWeb = entry.showPinInWeb,
        .sortOrder = 21,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKeyStable->c_str(),
        .cardPretty = entry.cardPrettyStable->c_str(),
        .cardOrder = entry.cardOrder,
    });

    entry.activeLow = std::make_unique<Config<bool>>(ConfigOptions<bool>{
        .key = entry.keyActiveLowStable->c_str(),
        .name = "Active LOW",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultActiveLow,
        .showInWeb = entry.showActiveLowInWeb,
        .sortOrder = 22,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKeyStable->c_str(),
        .cardPretty = entry.cardPrettyStable->c_str(),
        .cardOrder = entry.cardOrder,
    });

    entry.pullup = std::make_unique<Config<bool>>(ConfigOptions<bool>{
        .key = entry.keyPullupStable->c_str(),
        .name = "Pull-up",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultPullup,
        .showInWeb = entry.showPullupInWeb,
        .sortOrder = 23,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKeyStable->c_str(),
        .cardPretty = entry.cardPrettyStable->c_str(),
        .cardOrder = entry.cardOrder,
    });

    entry.pulldown = std::make_unique<Config<bool>>(ConfigOptions<bool>{
        .key = entry.keyPulldownStable->c_str(),
        .name = "Pull-down",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultPulldown,
        .showInWeb = entry.showPulldownInWeb,
        .sortOrder = 24,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKeyStable->c_str(),
        .cardPretty = entry.cardPrettyStable->c_str(),
        .cardOrder = entry.cardOrder,
    });

    ConfigManager.addSetting(entry.pin.get());
    ConfigManager.addSetting(entry.activeLow.get());
    ConfigManager.addSetting(entry.pullup.get());
    ConfigManager.addSetting(entry.pulldown.get());

    entry.runtimeGroup = (runtimeGroup && runtimeGroup[0]) ? String(runtimeGroup) : String("inputs");
    entry.runtimeLabel = (runtimeLabel && runtimeLabel[0]) ? String(runtimeLabel) : entry.name;
    entry.runtimeOrder = order;
    entry.alarmWhenActive = alarmWhenActive;
    ensureInputRuntimeProvider(entry.runtimeGroup);

    RuntimeFieldMeta meta;
    meta.group = entry.runtimeGroup;
    meta.key = entry.id;
    meta.label = entry.runtimeLabel;
    meta.isBool = true;
    meta.order = entry.runtimeOrder;
    if (entry.alarmWhenActive) {
        meta.boolAlarmValue = true;
    }
    ConfigManager.getRuntime().addRuntimeMeta(meta);

    entry.settingsRegistered = true;
}

void IOManager::addAnalogInputToGUI(const char* id, const char* cardName, int order,
                                    const char* runtimeLabel,
                                    const char* runtimeGroup,
                                    bool showRaw,
                                    bool showScaled)
{
    const int idx = findAnalogInputIndex(id);
    if (idx < 0) {
        CM_LOG("[IOManager][WARNING] addAnalogInputToGUI: unknown analog input '%s'", id ? id : "(null)");
        return;
    }

    AnalogInputEntry& entry = analogInputs[static_cast<size_t>(idx)];
    if (entry.settingsRegistered) {
        CM_LOG("[IOManager][WARNING] addAnalogInputToGUI: analog input '%s' already registered", entry.id.c_str());
        return;
    }

    entry.runtimeGroup = (runtimeGroup && runtimeGroup[0]) ? String(runtimeGroup) : String("analog");
    entry.runtimeLabel = (runtimeLabel && runtimeLabel[0]) ? String(runtimeLabel) : entry.name;
    entry.runtimeOrder = order;
    entry.runtimeShowRaw = showRaw;
    entry.runtimeShowScaled = showScaled;

    ensureAnalogRuntimeProvider(entry.runtimeGroup);

    if (!entry.registerSettings) {
        entry.settingsRegistered = true;
        return;
    }

    entry.cardKey = entry.id;
    entry.cardPretty = (cardName && cardName[0]) ? String(cardName) : entry.name;
    entry.cardOrder = order;

    entry.cardKeyStable = makeStableString(entry.cardKey);
    entry.cardPrettyStable = makeStableString(entry.cardPretty);

    entry.keyPin = formatAnalogSlotKey(entry.slot, 'P');
    entry.keyRawMin = formatAnalogSlotKey(entry.slot, 'R');
    entry.keyRawMax = formatAnalogSlotKey(entry.slot, 'S');
    entry.keyOutMin = formatAnalogSlotKey(entry.slot, 'M');
    entry.keyOutMax = formatAnalogSlotKey(entry.slot, 'N');
    entry.keyUnit = formatAnalogSlotKey(entry.slot, 'U');
    entry.keyDeadband = formatAnalogSlotKey(entry.slot, 'D');
    entry.keyMinEventMs = formatAnalogSlotKey(entry.slot, 'E');

    entry.keyPinStable = makeStableString(entry.keyPin);
    entry.keyRawMinStable = makeStableString(entry.keyRawMin);
    entry.keyRawMaxStable = makeStableString(entry.keyRawMax);
    entry.keyOutMinStable = makeStableString(entry.keyOutMin);
    entry.keyOutMaxStable = makeStableString(entry.keyOutMax);
    entry.keyUnitStable = makeStableString(entry.keyUnit);
    entry.keyDeadbandStable = makeStableString(entry.keyDeadband);
    entry.keyMinEventMsStable = makeStableString(entry.keyMinEventMs);

    entry.pin = std::make_unique<Config<int>>(ConfigOptions<int>{
        .key = entry.keyPinStable->c_str(),
        .name = "GPIO",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultPin,
        .showInWeb = entry.showPinInWeb,
        .sortOrder = 31,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKeyStable->c_str(),
        .cardPretty = entry.cardPrettyStable->c_str(),
        .cardOrder = entry.cardOrder,
    });

    entry.rawMin = std::make_unique<Config<int>>(ConfigOptions<int>{
        .key = entry.keyRawMinStable->c_str(),
        .name = "Raw Min",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultRawMin,
        .showInWeb = entry.showMappingInWeb,
        .sortOrder = 32,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKeyStable->c_str(),
        .cardPretty = entry.cardPrettyStable->c_str(),
        .cardOrder = entry.cardOrder,
    });

    entry.rawMax = std::make_unique<Config<int>>(ConfigOptions<int>{
        .key = entry.keyRawMaxStable->c_str(),
        .name = "Raw Max",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultRawMax,
        .showInWeb = entry.showMappingInWeb,
        .sortOrder = 33,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKeyStable->c_str(),
        .cardPretty = entry.cardPrettyStable->c_str(),
        .cardOrder = entry.cardOrder,
    });

    entry.outMin = std::make_unique<Config<float>>(ConfigOptions<float>{
        .key = entry.keyOutMinStable->c_str(),
        .name = "Out Min",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultOutMin,
        .showInWeb = entry.showMappingInWeb,
        .sortOrder = 34,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKeyStable->c_str(),
        .cardPretty = entry.cardPrettyStable->c_str(),
        .cardOrder = entry.cardOrder,
    });

    entry.outMax = std::make_unique<Config<float>>(ConfigOptions<float>{
        .key = entry.keyOutMaxStable->c_str(),
        .name = "Out Max",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultOutMax,
        .showInWeb = entry.showMappingInWeb,
        .sortOrder = 35,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKeyStable->c_str(),
        .cardPretty = entry.cardPrettyStable->c_str(),
        .cardOrder = entry.cardOrder,
    });

    entry.unit = std::make_unique<Config<String>>(ConfigOptions<String>{
        .key = entry.keyUnitStable->c_str(),
        .name = "Unit",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultUnit,
        .showInWeb = entry.showUnitInWeb,
        .sortOrder = 36,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKeyStable->c_str(),
        .cardPretty = entry.cardPrettyStable->c_str(),
        .cardOrder = entry.cardOrder,
    });

    entry.deadband = std::make_unique<Config<float>>(ConfigOptions<float>{
        .key = entry.keyDeadbandStable->c_str(),
        .name = "Deadband",
        .category = cm::CoreCategories::IO,
        .defaultValue = entry.defaultDeadband,
        .showInWeb = entry.showDeadbandInWeb,
        .sortOrder = 37,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKeyStable->c_str(),
        .cardPretty = entry.cardPrettyStable->c_str(),
        .cardOrder = entry.cardOrder,
    });

    entry.minEventMs = std::make_unique<Config<int>>(ConfigOptions<int>{
        .key = entry.keyMinEventMsStable->c_str(),
        .name = "Min Event (ms)",
        .category = cm::CoreCategories::IO,
        .defaultValue = static_cast<int>(entry.defaultMinEventMs),
        .showInWeb = entry.showMinEventInWeb,
        .sortOrder = 38,
        .categoryPretty = IO_CATEGORY_PRETTY,
        .card = entry.cardKeyStable->c_str(),
        .cardPretty = entry.cardPrettyStable->c_str(),
        .cardOrder = entry.cardOrder,
    });

    ConfigManager.addSetting(entry.pin.get());
    ConfigManager.addSetting(entry.rawMin.get());
    ConfigManager.addSetting(entry.rawMax.get());
    ConfigManager.addSetting(entry.outMin.get());
    ConfigManager.addSetting(entry.outMax.get());
    ConfigManager.addSetting(entry.unit.get());
    ConfigManager.addSetting(entry.deadband.get());
    ConfigManager.addSetting(entry.minEventMs.get());

    // Runtime meta entries
    if (entry.runtimeShowScaled) {
        RuntimeFieldMeta meta;
        meta.group = entry.runtimeGroup;
        meta.key = entry.id;
        meta.label = entry.runtimeLabel;
        meta.unit = entry.unit ? entry.unit->get() : entry.defaultUnit;
        meta.precision = entry.defaultPrecision;
        meta.order = entry.runtimeOrder;
        ConfigManager.getRuntime().addRuntimeMeta(meta);
    }

    if (entry.runtimeShowRaw) {
        RuntimeFieldMeta meta;
        meta.group = entry.runtimeGroup;
        meta.key = String(entry.id) + "_raw";
        meta.label = String(entry.runtimeLabel) + " Raw";
        meta.unit = "";
        meta.precision = 0;
        meta.order = entry.runtimeOrder + 1;
        ConfigManager.getRuntime().addRuntimeMeta(meta);
    }

    entry.settingsRegistered = true;
}

void IOManager::configureDigitalInputEvents(const char* id,
                                            DigitalInputEventCallbacks callbacks,
                                            DigitalInputEventOptions options)
{
    const int idx = findInputIndex(id);
    if (idx < 0) {
        CM_LOG("[IOManager][WARNING] configureDigitalInputEvents: unknown input '%s'", id ? id : "(null)");
        return;
    }

    DigitalInputEntry& entry = digitalInputs[static_cast<size_t>(idx)];
    entry.callbacks = std::move(callbacks);
    entry.eventOptions = options;
    entry.eventsEnabled = true;

    // Reset event state to avoid spurious triggers.
    entry.rawState = entry.state;
    entry.debouncedState = entry.state;
    entry.lastRawChangeMs = millis();
    entry.pressStartMs = 0;
    entry.longFired = false;
    entry.clickCount = 0;
    entry.lastReleaseMs = 0;
}

void IOManager::configureDigitalInputEvents(const char* id,
                                            DigitalInputEventCallbacks callbacks)
{
    DigitalInputEventOptions options;
    configureDigitalInputEvents(id, std::move(callbacks), options);
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
    startupLongPressWindowEndsMs = millis() + STARTUP_LONG_PRESS_WINDOW_MS;

    for (auto& entry : digitalOutputs) {
        entry.desiredState = false;
        entry.hasLast = false;
        reconfigureIfNeeded(entry);
        applyDesiredState(entry);
    }

    for (auto& entry : digitalInputs) {
        entry.hasLast = false;
        reconfigureIfNeeded(entry);
        readInputState(entry);

        if (entry.eventsEnabled) {
            const uint32_t nowMs = millis();
            entry.rawState = entry.state;
            entry.debouncedState = entry.state;
            entry.lastRawChangeMs = nowMs;
            entry.pressStartMs = entry.state ? nowMs : 0;
            entry.longFired = false;
            entry.clickCount = 0;
            entry.lastReleaseMs = 0;
        }
    }

    for (auto& entry : analogInputs) {
        entry.lastRawValue = -1;
        entry.lastValue = NAN;
        entry.lastEventMs = millis();
        entry.warningLoggedInvalidPin = false;
        reconfigureIfNeeded(entry);
        readAnalogInput(entry);
    }
}

bool IOManager::isStartupLongPressWindowActive(uint32_t nowMs) const
{
    // millis() wrap-safe comparison: active while now <= end.
    return static_cast<int32_t>(nowMs - startupLongPressWindowEndsMs) <= 0;
}

void IOManager::update()
{
    for (auto& entry : digitalOutputs) {
        reconfigureIfNeeded(entry);
        applyDesiredState(entry);
    }

    const uint32_t nowMs = millis();
    for (auto& entry : digitalInputs) {
        reconfigureIfNeeded(entry);
        readInputState(entry);
        processInputEvents(entry, nowMs);
    }

    for (auto& entry : analogInputs) {
        reconfigureIfNeeded(entry);
        readAnalogInput(entry);
        processAnalogEvents(entry, nowMs);
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

bool IOManager::getInputState(const char* id) const
{
    const int idx = findInputIndex(id);
    if (idx < 0) {
        return false;
    }

    const DigitalInputEntry& entry = digitalInputs[static_cast<size_t>(idx)];
    return entry.state;
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

int IOManager::getAnalogRawValue(const char* id) const
{
    const int idx = findAnalogInputIndex(id);
    if (idx < 0) {
        return -1;
    }

    const AnalogInputEntry& entry = analogInputs[static_cast<size_t>(idx)];
    return entry.rawValue;
}

float IOManager::getAnalogValue(const char* id) const
{
    const int idx = findAnalogInputIndex(id);
    if (idx < 0) {
        return NAN;
    }

    const AnalogInputEntry& entry = analogInputs[static_cast<size_t>(idx)];
    return entry.value;
}

bool IOManager::isValidPin(int pin)
{
    return pin >= 0 && pin <= 39;
}

bool IOManager::isValidAnalogPin(int pin)
{
    // ESP32 Arduino: ADC1 pins 32-39, ADC2 pins 0,2,4,12-15,25-27.
    // Note: ADC2 reads can be unreliable while WiFi is active.
    if (pin >= 32 && pin <= 39) return true;
    if (pin == 0 || pin == 2 || pin == 4) return true;
    if (pin >= 12 && pin <= 15) return true;
    if (pin >= 25 && pin <= 27) return true;
    return false;
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

int IOManager::findInputIndex(const char* id) const
{
    if (!id || !id[0]) return -1;
    for (size_t i = 0; i < digitalInputs.size(); i++) {
        if (digitalInputs[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

int IOManager::findAnalogInputIndex(const char* id) const
{
    if (!id || !id[0]) return -1;
    for (size_t i = 0; i < analogInputs.size(); i++) {
        if (analogInputs[i].id == id) return static_cast<int>(i);
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

bool IOManager::isInputActiveLowNow(const DigitalInputEntry& entry)
{
    if (entry.activeLow) {
        return entry.activeLow->get();
    }
    return entry.defaultActiveLow;
}

bool IOManager::isInputPullupNow(const DigitalInputEntry& entry)
{
    if (entry.pullup) {
        return entry.pullup->get();
    }
    return entry.defaultPullup;
}

bool IOManager::isInputPulldownNow(const DigitalInputEntry& entry)
{
    if (entry.pulldown) {
        return entry.pulldown->get();
    }
    return entry.defaultPulldown;
}

int IOManager::getInputPinNow(const DigitalInputEntry& entry)
{
    if (entry.pin) {
        return entry.pin->get();
    }
    return entry.defaultPin;
}

int IOManager::getAnalogPinNow(const AnalogInputEntry& entry)
{
    if (entry.pin) {
        return entry.pin->get();
    }
    return entry.defaultPin;
}

int IOManager::getAnalogRawMinNow(const AnalogInputEntry& entry)
{
    if (entry.rawMin) return entry.rawMin->get();
    return entry.defaultRawMin;
}

int IOManager::getAnalogRawMaxNow(const AnalogInputEntry& entry)
{
    if (entry.rawMax) return entry.rawMax->get();
    return entry.defaultRawMax;
}

float IOManager::getAnalogOutMinNow(const AnalogInputEntry& entry)
{
    if (entry.outMin) return entry.outMin->get();
    return entry.defaultOutMin;
}

float IOManager::getAnalogOutMaxNow(const AnalogInputEntry& entry)
{
    if (entry.outMax) return entry.outMax->get();
    return entry.defaultOutMax;
}

float IOManager::getAnalogDeadbandNow(const AnalogInputEntry& entry)
{
    if (entry.deadband) return entry.deadband->get();
    return entry.defaultDeadband;
}

uint32_t IOManager::getAnalogMinEventMsNow(const AnalogInputEntry& entry)
{
    if (entry.minEventMs) {
        const int ms = entry.minEventMs->get();
        if (ms < 0) return 0;
        return static_cast<uint32_t>(ms);
    }
    return entry.defaultMinEventMs;
}

float IOManager::mapAnalogValue(int raw, int rawMin, int rawMax, float outMin, float outMax)
{
    if (rawMax == rawMin) {
        return outMin;
    }

    const float t = (static_cast<float>(raw) - static_cast<float>(rawMin)) /
                    (static_cast<float>(rawMax) - static_cast<float>(rawMin));
    return outMin + t * (outMax - outMin);
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

void IOManager::reconfigureIfNeeded(DigitalInputEntry& entry)
{
    const int pin = getInputPinNow(entry);
    const bool activeLow = isInputActiveLowNow(entry);
    const bool pullup = isInputPullupNow(entry);
    const bool pulldown = isInputPulldownNow(entry);

    if (!isValidPin(pin)) {
        entry.hasLast = false;
        entry.state = false;
        return;
    }

    if (entry.hasLast && entry.lastPin == pin && entry.lastActiveLow == activeLow && entry.lastPullup == pullup && entry.lastPulldown == pulldown) {
        return;
    }

    if (pullup && pulldown) {
        // Prefer pull-up to stay deterministic.
        CM_LOG("[IOManager][WARNING] Input '%s': pull-up and pull-down both enabled, using pull-up", entry.id.c_str());
    }

    if (pullup) {
        pinMode(pin, INPUT_PULLUP);
    } else if (pulldown) {
        pinMode(pin, INPUT_PULLDOWN);
    } else {
        pinMode(pin, INPUT);
    }
    entry.lastPin = pin;
    entry.lastActiveLow = activeLow;
    entry.lastPullup = pullup;
    entry.lastPulldown = pulldown;
    entry.hasLast = true;
}

void IOManager::reconfigureIfNeeded(AnalogInputEntry& entry)
{
    const int pin = getAnalogPinNow(entry);
    if (!isValidAnalogPin(pin)) {
        entry.rawValue = -1;
        entry.value = NAN;
        return;
    }

    // No pinMode required for analogRead on ESP32, but we keep a best-effort config for clarity.
    pinMode(pin, INPUT);

    if (!entry.warningLoggedInvalidPin) {
        // Warn once about ADC2 pins (WiFi interaction). GPIO4 is ADC2.
        if (!(pin >= 32 && pin <= 39)) {
            CM_LOG("[IOManager][WARNING] Analog input '%s' uses ADC2 pin %d; readings may be unreliable while WiFi is active", entry.id.c_str(), pin);
        }
        entry.warningLoggedInvalidPin = true;
    }
}

void IOManager::readAnalogInput(AnalogInputEntry& entry)
{
    const int pin = getAnalogPinNow(entry);
    if (!isValidAnalogPin(pin)) {
        if (!entry.warningLoggedInvalidPin) {
            CM_LOG("[IOManager][WARNING] Analog input '%s' pin %d is not ADC-capable on ESP32", entry.id.c_str(), pin);
            entry.warningLoggedInvalidPin = true;
        }
        entry.rawValue = -1;
        entry.value = NAN;
        return;
    }

    if (!entry.defaultEnabled) {
        entry.rawValue = -1;
        entry.value = NAN;
        return;
    }

    const int raw = analogRead(pin);
    entry.rawValue = raw;

    const int rawMin = getAnalogRawMinNow(entry);
    const int rawMax = getAnalogRawMaxNow(entry);
    const float outMin = getAnalogOutMinNow(entry);
    const float outMax = getAnalogOutMaxNow(entry);
    entry.value = mapAnalogValue(raw, rawMin, rawMax, outMin, outMax);
}

void IOManager::processAnalogEvents(AnalogInputEntry& entry, uint32_t nowMs)
{
    // This is a minimal baseline: store last values and allow periodic refresh based on minEventMs.
    const float db = getAnalogDeadbandNow(entry);
    const uint32_t minEventMs = getAnalogMinEventMsNow(entry);

    const bool hasValue = !isnan(entry.value);
    const bool hadValue = !isnan(entry.lastValue);

    bool trigger = false;
    if (hasValue && hadValue) {
        const float diff = fabsf(entry.value - entry.lastValue);
        if (diff >= db) {
            trigger = true;
        }
    } else if (hasValue != hadValue) {
        trigger = true;
    }

    if (!trigger && minEventMs > 0) {
        if (nowMs - entry.lastEventMs >= minEventMs) {
            trigger = true;
        }
    }

    if (trigger) {
        entry.lastRawValue = entry.rawValue;
        entry.lastValue = entry.value;
        entry.lastEventMs = nowMs;
    }
}

void IOManager::ensureAnalogRuntimeProvider(const String& group)
{
    static std::vector<String> registered;
    for (const auto& g : registered) {
        if (g == group) return;
    }

    ConfigManager.getRuntime().addRuntimeProvider(group, [this, group](JsonObject& data) {
        for (const auto& entry : analogInputs) {
            if (entry.runtimeGroup != group) {
                continue;
            }

            if (entry.runtimeShowScaled) {
                if (isnan(entry.value)) {
                    data[entry.id] = nullptr;
                } else {
                    data[entry.id] = entry.value;
                }
            }

            if (entry.runtimeShowRaw) {
                const String rawKey = String(entry.id) + "_raw";
                if (entry.rawValue < 0) {
                    data[rawKey] = nullptr;
                } else {
                    data[rawKey] = entry.rawValue;
                }
            }
        }
    }, 5);

    registered.push_back(group);
}

void IOManager::readInputState(DigitalInputEntry& entry)
{
    const int pin = getInputPinNow(entry);
    if (!isValidPin(pin)) {
        entry.state = false;
        return;
    }

    const bool activeLow = isInputActiveLowNow(entry);
    const int level = digitalRead(pin);
    entry.state = activeLow ? (level == LOW) : (level == HIGH);
}

void IOManager::processInputEvents(DigitalInputEntry& entry, uint32_t nowMs)
{
    if (!entry.eventsEnabled) {
        return;
    }

    const bool newRaw = entry.state;
    if (newRaw != entry.rawState) {
        entry.rawState = newRaw;
        entry.lastRawChangeMs = nowMs;
    }

    const uint32_t debounceMs = entry.eventOptions.debounceMs;
    if (entry.debouncedState != entry.rawState) {
        if (nowMs - entry.lastRawChangeMs >= debounceMs) {
            // Debounced edge
            entry.debouncedState = entry.rawState;

            if (entry.debouncedState) {
                // Press
                entry.pressStartMs = nowMs;
                entry.longFired = false;
                if (entry.callbacks.onPress) {
                    entry.callbacks.onPress();
                }
            } else {
                // Release
                if (entry.callbacks.onRelease) {
                    entry.callbacks.onRelease();
                }

                if (!entry.longFired) {
                    entry.clickCount = static_cast<uint8_t>(entry.clickCount + 1);
                    entry.lastReleaseMs = nowMs;
                    if (entry.clickCount >= 2) {
                        if (entry.callbacks.onDoubleClick) {
                            entry.callbacks.onDoubleClick();
                        }
                        entry.clickCount = 0;
                    }
                } else {
                    entry.clickCount = 0;
                }
            }
        }
    }

    // Long click (fires once per press)
    if (entry.debouncedState && !entry.longFired) {
        if (nowMs - entry.pressStartMs >= entry.eventOptions.longClickMs) {
            entry.longFired = true;
            entry.clickCount = 0;
            if (entry.callbacks.onLongPressOnStartup && isStartupLongPressWindowActive(nowMs)) {
                entry.callbacks.onLongPressOnStartup();
            } else if (entry.callbacks.onLongClick) {
                entry.callbacks.onLongClick();
            }
        }
    }

    // Single click timeout
    if (!entry.debouncedState && entry.clickCount == 1) {
        if (nowMs - entry.lastReleaseMs >= entry.eventOptions.doubleClickMs) {
            if (entry.callbacks.onClick) {
                entry.callbacks.onClick();
            }
            entry.clickCount = 0;
        }
    }
}

void IOManager::ensureInputRuntimeProvider(const String& group)
{
    static std::vector<String> registered;
    for (const auto& g : registered) {
        if (g == group) return;
    }

    ConfigManager.getRuntime().addRuntimeProvider(group, [this, group](JsonObject& data) {
        for (const auto& entry : digitalInputs) {
            if (entry.runtimeGroup == group) {
                data[entry.id] = entry.state;
            }
        }
    }, 5);

    registered.push_back(group);
}

} // namespace cm
