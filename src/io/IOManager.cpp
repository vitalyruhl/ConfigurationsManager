#include "IOManager.h"

#define IO_LOG(...) CM_LOG("[IO] " __VA_ARGS__)
#include "core/CoreSettings.h"
#include "io/ioDefinitions.h"

#include <cmath>

namespace cm {

static std::shared_ptr<std::string> makeStableString(const String& value)
{
    return std::make_shared<std::string>(value.c_str());
}

static constexpr const char* IO_CATEGORY_PRETTY = "I/O";

namespace {
static constexpr const char* IO_SETTINGS_PAGE = IO_CATEGORY_PRETTY;

static void registerSettingPlacement(BaseSetting* setting, const String& pageName, const String& cardName, const String& groupName)
{
    if (!setting || !setting->shouldShowInWeb()) {
        return;
    }
    ConfigManager.addToSettingsGroup(setting->getKey(), pageName.c_str(), cardName.c_str(), groupName.c_str(), setting->getSortOrder());
}

enum class BindingPinType {
    DigitalOutput,
    DigitalInput,
    AnalogInput,
    AnalogOutput,
};

static bool isPinValidForBinding(const cm::io::IOPinRules& rules, int pin, BindingPinType type)
{
    switch (type) {
        case BindingPinType::DigitalOutput:
            return rules.isValidDigitalOutputPin(pin);
        case BindingPinType::DigitalInput:
            return rules.isValidDigitalInputPin(pin);
        case BindingPinType::AnalogInput:
            return rules.isValidAnalogInputPin(pin);
        case BindingPinType::AnalogOutput:
            return rules.isValidAnalogOutputPin(pin);
        default:
            return false;
    }
}

static const char* bindingPinTypeLabel(BindingPinType type)
{
    switch (type) {
        case BindingPinType::DigitalOutput:
            return "digital output";
        case BindingPinType::DigitalInput:
            return "digital input";
        case BindingPinType::AnalogInput:
            return "analog input";
        case BindingPinType::AnalogOutput:
            return "analog output";
        default:
            return "io";
    }
}

static bool validateDefaultBindingPin(int pin, BindingPinType type, const char* id, const char* caller)
{
    const cm::io::GUIMode mode = ConfigManager.getGUIMode();
    std::unique_ptr<cm::io::IOPinRules> pinRules = cm::io::createPinRulesForMode(mode);
    if (!pinRules) {
        IO_LOG("[W] %s: skip pin validation for '%s' (pin rules unavailable)", caller, id ? id : "(null)");
        return true;
    }

    if (isPinValidForBinding(*pinRules, pin, type)) {
        return true;
    }

    const cm::io::PinInfo info = pinRules->getPinInfo(pin);
    const String constraints = pinRules->describeConstraints(info.constraints);
    if (!constraints.isEmpty()) {
        IO_LOG("[E] %s: reject '%s' pin=%d (%s, %s)", caller, id ? id : "(null)", pin, bindingPinTypeLabel(type), constraints.c_str());
    } else {
        IO_LOG("[E] %s: reject '%s' pin=%d (%s)", caller, id ? id : "(null)", pin, bindingPinTypeLabel(type));
    }
    IO_LOG("[E] %s: mode=%s", caller, cm::io::toString(mode));
    return false;
}
} // namespace

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

String IOManager::formatAnalogOutputSlotKey(uint8_t slot, char suffix)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "AO%02u%c", static_cast<unsigned>(slot), suffix);
    return String(buf);
}

float IOManager::clampFloat(float value, float minValue, float maxValue)
{
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

float IOManager::mapFloat(float value, float inMin, float inMax, float outMin, float outMax)
{
    const float inRange = inMax - inMin;
    if (fabsf(inRange) < 1e-9f) {
        return outMin;
    }
    const float t = (value - inMin) / inRange;
    return outMin + t * (outMax - outMin);
}

void IOManager::addDigitalOutput(const DigitalOutputBinding& binding)
{
    if (!binding.id || !binding.id[0]) {
        IO_LOG("[ERROR] addDigitalOutput: invalid binding");
        return;
    }

    if (findIndex(binding.id) >= 0) {
        IO_LOG("[WARNING] addDigitalOutput: output '%s' already exists", binding.id);
        return;
    }

    if (!validateDefaultBindingPin(binding.defaultPin, BindingPinType::DigitalOutput, binding.id, "addDigitalOutput")) {
        return;
    }

    DigitalOutputEntry entry;
    entry.id = binding.id;
    entry.name = binding.name ? binding.name : binding.id;

    entry.slot = nextDigitalOutputSlot;
    if (nextDigitalOutputSlot < 99) {
        nextDigitalOutputSlot++;
    } else {
        IO_LOG("[WARNING] addDigitalOutput: exceeded slot range 00..99, keys may not remain stable");
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
        IO_LOG("[ERROR] addDigitalInput: invalid binding");
        return;
    }

    if (findInputIndex(binding.id) >= 0) {
        IO_LOG("[WARNING] addDigitalInput: input '%s' already exists", binding.id);
        return;
    }

    if (!validateDefaultBindingPin(binding.defaultPin, BindingPinType::DigitalInput, binding.id, "addDigitalInput")) {
        return;
    }

    DigitalInputEntry entry;
    entry.id = binding.id;
    entry.name = binding.name ? binding.name : binding.id;

    entry.slot = nextDigitalInputSlot;
    if (nextDigitalInputSlot < 99) {
        nextDigitalInputSlot++;
    } else {
        IO_LOG("[WARNING] addDigitalInput: exceeded slot range 00..99, keys may not remain stable");
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
        IO_LOG("[ERROR] addAnalogInput: invalid binding");
        return;
    }

    if (findAnalogInputIndex(binding.id) >= 0) {
        IO_LOG("[WARNING] addAnalogInput: input '%s' already exists", binding.id);
        return;
    }

    if (!validateDefaultBindingPin(binding.defaultPin, BindingPinType::AnalogInput, binding.id, "addAnalogInput")) {
        return;
    }

    AnalogInputEntry entry;
    entry.id = binding.id;
    entry.name = binding.name ? binding.name : binding.id;

    entry.slot = nextAnalogInputSlot;
    if (nextAnalogInputSlot < 99) {
        nextAnalogInputSlot++;
    } else {
        IO_LOG("[WARNING] addAnalogInput: exceeded slot range 00..99, keys may not remain stable");
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
    entry.settingsCategory = cm::CoreCategories::IO;

    entry.registerSettings = binding.registerSettings;
    entry.showPinInWeb = binding.showPinInWeb;
    entry.showMappingInWeb = binding.showMappingInWeb;
    entry.showUnitInWeb = binding.showUnitInWeb;
    entry.showDeadbandInWeb = binding.showDeadbandInWeb;
    entry.showMinEventInWeb = binding.showMinEventInWeb;

    analogInputs.push_back(std::move(entry));
}

void IOManager::addAnalogOutput(const AnalogOutputBinding& binding)
{
    if (!binding.id || !binding.id[0]) {
        IO_LOG("[ERROR] addAnalogOutput: invalid binding");
        return;
    }

    if (findAnalogOutputIndex(binding.id) >= 0) {
        IO_LOG("[WARNING] addAnalogOutput: output '%s' already exists", binding.id);
        return;
    }

    if (!validateDefaultBindingPin(binding.defaultPin, BindingPinType::AnalogOutput, binding.id, "addAnalogOutput")) {
        return;
    }

    AnalogOutputEntry entry;
    entry.id = binding.id;
    entry.name = binding.name ? binding.name : binding.id;

    entry.slot = nextAnalogOutputSlot;
    if (nextAnalogOutputSlot < 99) {
        nextAnalogOutputSlot++;
    } else {
        IO_LOG("[WARNING] addAnalogOutput: exceeded slot range 00..99, keys may not remain stable");
        nextAnalogOutputSlot++;
    }

    entry.defaultPin = binding.defaultPin;
    entry.defaultEnabled = binding.defaultEnabled;
    entry.valueMin = binding.valueMin;
    entry.valueMax = binding.valueMax;
    entry.reverse = binding.reverse;

    entry.registerSettings = binding.registerSettings;
    entry.showPinInWeb = binding.showPinInWeb;

    // Initialize state
    entry.desiredRawVolts = 0.0f;
    entry.rawVolts = 0.0f;
    entry.desiredValue = clampFloat(0.0f, entry.valueMin, entry.valueMax);
    entry.value = entry.desiredValue;

    analogOutputs.push_back(std::move(entry));
}

void IOManager::addDigitalInput(const char* id,
                                const char* name,
                                int gpioPin,
                                bool activeLow,
                                bool pullup,
                                bool pulldown,
                                bool persistSettings)
{
    DigitalInputBinding binding;
    binding.id = id;
    binding.name = name;
    binding.defaultPin = gpioPin;
    binding.defaultActiveLow = activeLow;
    binding.defaultPullup = pullup;
    binding.defaultPulldown = pulldown;
    binding.defaultEnabled = true;
    binding.registerSettings = persistSettings;
    addDigitalInput(binding);
}

void IOManager::addDigitalOutput(const char* id,
                                 const char* name,
                                 int gpioPin,
                                 bool activeLow,
                                 bool persistSettings)
{
    DigitalOutputBinding binding;
    binding.id = id;
    binding.name = name;
    binding.defaultPin = gpioPin;
    binding.defaultActiveLow = activeLow;
    binding.defaultEnabled = true;
    binding.registerSettings = persistSettings;
    addDigitalOutput(binding);
}

void IOManager::addAnalogInput(const char* id,
                               const char* name,
                               int adcPin,
                               bool persistSettings,
                               int rawMin,
                               int rawMax,
                               float outMin,
                               float outMax,
                               const char* unit,
                               int precision,
                               float deadband,
                               uint32_t minEventMs)
{
    AnalogInputBinding binding;
    binding.id = id;
    binding.name = name;
    binding.defaultPin = adcPin;
    binding.defaultEnabled = true;
    binding.defaultRawMin = rawMin;
    binding.defaultRawMax = rawMax;
    binding.defaultOutMin = outMin;
    binding.defaultOutMax = outMax;
    binding.defaultUnit = unit ? unit : "";
    binding.defaultPrecision = precision;
    binding.defaultDeadband = deadband;
    binding.defaultMinEventMs = minEventMs;
    binding.registerSettings = persistSettings;
    addAnalogInput(binding);
}

void IOManager::addAnalogOutput(const char* id,
                                const char* name,
                                int dacOrPwmPin,
                                bool persistSettings,
                                float valueMin,
                                float valueMax,
                                bool reverse)
{
    AnalogOutputBinding binding;
    binding.id = id;
    binding.name = name;
    binding.defaultPin = dacOrPwmPin;
    binding.defaultEnabled = true;
    binding.valueMin = valueMin;
    binding.valueMax = valueMax;
    binding.reverse = reverse;
    binding.registerSettings = persistSettings;
    addAnalogOutput(binding);
}


static void addAnalogRuntimeMeta(ConfigManagerRuntime& runtime,
                                 const String& group,
                                 const String& key,
                                 const String& label,
                                 const String& unit,
                                 int precision,
                                 int order,
                                 bool hasAlarm,
                                 float alarmMin,
                                 float alarmMax)
{
    RuntimeFieldMeta meta;
    meta.group = group;
    meta.key = key;
    meta.label = label;
    meta.unit = unit;
    meta.precision = precision;
    meta.order = order;

    if (hasAlarm) {
        meta.hasAlarm = true;
        meta.alarmMin = alarmMin;
        meta.alarmMax = alarmMax;
    }

    runtime.addRuntimeMeta(meta);
}

static void addAnalogOutputRuntimeMeta(ConfigManagerRuntime& runtime,
                                       const String& group,
                                       const String& key,
                                       const String& label,
                                       const String& unit,
                                       int precision,
                                       int order)
{
    addAnalogRuntimeMeta(runtime, group, key, label, unit, precision, order, false, 0.0f, 0.0f);
}

void IOManager::ensureAnalogAlarmSettings(AnalogInputEntry& entry,
                                          float alarmMin,
                                          float alarmMax)
{
    if (!entry.registerSettings) {
        return;
    }
    if (!entry.cardKeyStable || !entry.cardPrettyStable) {
        return;
    }
    if (!entry.keyAlarmMinStable || !entry.keyAlarmMaxStable) {
        return;
    }

    const char* categoryName = entry.settingsCategory.isEmpty()
        ? cm::CoreCategories::IO
        : entry.settingsCategory.c_str();

    if (!isnan(alarmMin) && !entry.alarmMinSetting) {
        entry.alarmMinSetting = &ConfigManager.addSettingFloat(entry.keyAlarmMinStable->c_str())
            .name("Alarm Min")
            .category(categoryName)
            .defaultValue(alarmMin)
            .showInWeb(true)
            .sortOrder(39)
            .categoryPretty(categoryName)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .build();
        registerSettingPlacement(entry.alarmMinSetting, categoryName, entry.cardPretty, entry.name);
    }

    if (!isnan(alarmMax) && !entry.alarmMaxSetting) {
        entry.alarmMaxSetting = &ConfigManager.addSettingFloat(entry.keyAlarmMaxStable->c_str())
            .name("Alarm Max")
            .category(categoryName)
            .defaultValue(alarmMax)
            .showInWeb(true)
            .sortOrder(40)
            .categoryPretty(categoryName)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .build();
        registerSettingPlacement(entry.alarmMaxSetting, categoryName, entry.cardPretty, entry.name);
    }
}

static void addAnalogAlarmRuntimeIndicators(ConfigManagerRuntime& runtime,
                                            const String& group,
                                            const String& id,
                                            int baseOrder,
                                            bool hasMin,
                                            bool hasMax)
{
    if (hasMin) {
        RuntimeFieldMeta meta;
        meta.group = group;
        meta.key = id + "_alarm_min";
        meta.label = "Alarm Min";
        meta.isBool = true;
        meta.boolAlarmValue = true;
        meta.order = baseOrder + 1;
        runtime.addRuntimeMeta(meta);
    }

    if (hasMax) {
        RuntimeFieldMeta meta;
        meta.group = group;
        meta.key = id + "_alarm_max";
        meta.label = "Alarm Max";
        meta.isBool = true;
        meta.boolAlarmValue = true;
        meta.order = baseOrder + 2;
        runtime.addRuntimeMeta(meta);
    }
}

void IOManager::configureAnalogInputAlarm(const char* id,
                                          float alarmMin,
                                          float alarmMax,
                                          AnalogAlarmCallbacks callbacks)
{
    const int idx = findAnalogInputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] configureAnalogInputAlarm: unknown analog input '%s'", id ? id : "(null)");
        return;
    }

    AnalogInputEntry& entry = analogInputs[static_cast<size_t>(idx)];
    entry.alarmMin = alarmMin;
    entry.alarmMax = alarmMax;
    entry.alarmCallbacks = std::move(callbacks);
}

void IOManager::registerAnalogRuntimeField(const String& group, const String& id, bool showRaw)
{
    for (auto& runtimeGroup : analogRuntimeGroups) {
        if (runtimeGroup.group != group) {
            continue;
        }

        for (const auto& field : runtimeGroup.fields) {
            if (field.id == id && field.showRaw == showRaw) {
                return;
            }
        }

        runtimeGroup.fields.push_back(AnalogRuntimeField{.id = id, .showRaw = showRaw});
        return;
    }

    AnalogRuntimeGroup newGroup;
    newGroup.group = group;
    newGroup.fields.push_back(AnalogRuntimeField{.id = id, .showRaw = showRaw});
    analogRuntimeGroups.push_back(std::move(newGroup));
}

void IOManager::configureDigitalInputEvents(const char* id,
                                            DigitalInputEventCallbacks callbacks,
                                            DigitalInputEventOptions options)
{
    const int idx = findInputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] configureDigitalInputEvents: unknown input '%s'", id ? id : "(null)");
        return;
    }

    DigitalInputEntry& entry = digitalInputs[static_cast<size_t>(idx)];
    entry.callbacks = std::move(callbacks);
    entry.eventOptions = options;
    entry.eventsEnabled = true;

    resetDigitalInputEventState(entry, millis());
}

void IOManager::configureDigitalInputEvents(const char* id,
                                            DigitalInputEventCallbacks callbacks)
{
    DigitalInputEventOptions options;
    configureDigitalInputEvents(id, std::move(callbacks), options);
}

static void ensureSettingsLayout(const char* pageName, const char* cardName, const char* groupName, int order)
{
    if (!pageName || !pageName[0]) {
        return;
    }
    const char* effectiveCard = (cardName && cardName[0]) ? cardName : pageName;
    const char* effectiveGroup = (groupName && groupName[0]) ? groupName : effectiveCard;
    ConfigManager.addSettingsPage(pageName, order);
    ConfigManager.addSettingsCard(pageName, effectiveCard, order);
    ConfigManager.addSettingsGroup(pageName, effectiveCard, effectiveGroup, order);
}

static void ensureLiveLayout(const char* pageName, const char* cardName, const char* groupName, int order)
{
    if (!pageName || !pageName[0]) {
        return;
    }
    const char* effectiveCard = (cardName && cardName[0]) ? cardName : "Live Values";
    const char* effectiveGroup = (groupName && groupName[0]) ? groupName : effectiveCard;
    ConfigManager.addLivePage(pageName, order);
    ConfigManager.addLiveCard(pageName, effectiveCard, order);
    ConfigManager.addLiveGroup(pageName, effectiveCard, effectiveGroup, order);
}

void IOManager::addDigitalInputToSettings(const char* id, const char* pageName, int order)
{
    addDigitalInputToSettingsGroup(id, pageName, pageName, pageName, order);
}

void IOManager::addDigitalInputToSettingsGroup(const char* id, const char* pageName, const char* groupName, int order)
{
    addDigitalInputToSettingsGroup(id, pageName, pageName, groupName, order);
}

void IOManager::addDigitalInputToSettingsGroup(const char* id, const char* pageName, const char* cardName, const char* groupName, int order)
{
    const int idx = findInputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] addDigitalInputToSettingsGroup: unknown input '%s'", id ? id : "(null)");
        return;
    }

    DigitalInputEntry& entry = digitalInputs[static_cast<size_t>(idx)];
    if (!entry.registerSettings) {
        IO_LOG("[WARNING] addDigitalInputToSettingsGroup: input '%s' is not persisted", entry.id.c_str());
        return;
    }

    const char* categoryName = (pageName && pageName[0]) ? pageName : cm::CoreCategories::IO;
    const char* categoryPretty = categoryName;

    if (!entry.settingsRegistered) {
        entry.cardKey = entry.id;
        if (groupName && groupName[0]) {
            entry.cardPretty = String(groupName);
        } else {
            entry.cardPretty = (cardName && cardName[0]) ? String(cardName) : entry.name;
        }
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

        entry.pin = &ConfigManager.addSettingInt(entry.keyPinStable->c_str())
            .name("GPIO")
            .category(categoryName)
            .defaultValue(entry.defaultPin)
            .showInWeb(entry.showPinInWeb)
            .sortOrder(21)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .build();

        entry.activeLow = &ConfigManager.addSettingBool(entry.keyActiveLowStable->c_str())
            .name("LOW-Active")
            .category(categoryName)
            .defaultValue(entry.defaultActiveLow)
            .showInWeb(entry.showActiveLowInWeb)
            .sortOrder(22)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .build();

        entry.pullup = &ConfigManager.addSettingBool(entry.keyPullupStable->c_str())
            .name("Pull-up")
            .category(categoryName)
            .defaultValue(entry.defaultPullup)
            .showInWeb(entry.showPullupInWeb)
            .sortOrder(23)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .build();

        entry.pulldown = &ConfigManager.addSettingBool(entry.keyPulldownStable->c_str())
            .name("Pull-down")
            .category(categoryName)
            .defaultValue(entry.defaultPulldown)
            .showInWeb(entry.showPulldownInWeb)
            .sortOrder(24)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .build();

        entry.settingsRegistered = true;
    }

    ensureSettingsLayout(pageName, cardName, groupName, order);
    const String effectiveGroup = (groupName && groupName[0]) ? String(groupName) : entry.name;
    const String effectiveCard = (cardName && cardName[0]) ? String(cardName) : entry.name;

    registerSettingPlacement(entry.pin, pageName, effectiveCard, effectiveGroup);
    registerSettingPlacement(entry.activeLow, pageName, effectiveCard, effectiveGroup);
    registerSettingPlacement(entry.pullup, pageName, effectiveCard, effectiveGroup);
    registerSettingPlacement(entry.pulldown, pageName, effectiveCard, effectiveGroup);
}

void IOManager::addDigitalOutputToSettings(const char* id, const char* pageName, int order)
{
    addDigitalOutputToSettingsGroup(id, pageName, pageName, pageName, order);
}

void IOManager::addDigitalOutputToSettingsGroup(const char* id, const char* pageName, const char* groupName, int order)
{
    addDigitalOutputToSettingsGroup(id, pageName, pageName, groupName, order);
}

void IOManager::addDigitalOutputToSettingsGroup(const char* id, const char* pageName, const char* cardName, const char* groupName, int order)
{
    const int idx = findIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] addDigitalOutputToSettingsGroup: unknown output '%s'", id ? id : "(null)");
        return;
    }

    DigitalOutputEntry& entry = digitalOutputs[static_cast<size_t>(idx)];
    if (!entry.registerSettings) {
        IO_LOG("[WARNING] addDigitalOutputToSettingsGroup: output '%s' is not persisted", entry.id.c_str());
        return;
    }

    const char* categoryName = (pageName && pageName[0]) ? pageName : cm::CoreCategories::IO;
    const char* categoryPretty = categoryName;

    if (!entry.settingsRegistered) {
        entry.cardKey = entry.id;
        if (groupName && groupName[0]) {
            entry.cardPretty = String(groupName);
        } else {
            entry.cardPretty = (cardName && cardName[0]) ? String(cardName) : entry.name;
        }
        entry.cardOrder = order;

        entry.cardKeyStable = makeStableString(entry.cardKey);
        entry.cardPrettyStable = makeStableString(entry.cardPretty);

        entry.keyPin = formatSlotKey(entry.slot, 'P');
        entry.keyActiveLow = formatSlotKey(entry.slot, 'L');

        entry.keyPinStable = makeStableString(entry.keyPin);
        entry.keyActiveLowStable = makeStableString(entry.keyActiveLow);

        entry.pin = &ConfigManager.addSettingInt(entry.keyPinStable->c_str())
            .name("GPIO")
            .category(categoryName)
            .defaultValue(entry.defaultPin)
            .showInWeb(entry.showPinInWeb)
            .sortOrder(11)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .ioPinRole(cm::io::IOPinRole::DigitalOutput)
            .build();

        entry.activeLow = &ConfigManager.addSettingBool(entry.keyActiveLowStable->c_str())
            .name("LOW-Active")
            .category(categoryName)
            .defaultValue(entry.defaultActiveLow)
            .showInWeb(entry.showActiveLowInWeb)
            .sortOrder(12)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .ioPinRole(cm::io::IOPinRole::DigitalInput)
            .build();

        entry.settingsRegistered = true;
    }

    ensureSettingsLayout(pageName, cardName, groupName, order);
    const String effectiveGroup = (groupName && groupName[0]) ? String(groupName) : entry.name;
    const String effectiveCard = (cardName && cardName[0]) ? String(cardName) : entry.name;

    registerSettingPlacement(entry.pin, pageName, effectiveCard, effectiveGroup);
    registerSettingPlacement(entry.activeLow, pageName, effectiveCard, effectiveGroup);
}

void IOManager::addAnalogInputToSettings(const char* id, const char* pageName, int order)
{
    addAnalogInputToSettingsGroup(id, pageName, pageName, pageName, order);
}

void IOManager::addAnalogInputToSettingsGroup(const char* id, const char* pageName, const char* groupName, int order)
{
    addAnalogInputToSettingsGroup(id, pageName, pageName, groupName, order);
}

void IOManager::addAnalogInputToSettingsGroup(const char* id, const char* pageName, const char* cardName, const char* groupName, int order)
{
    const int idx = findAnalogInputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] addAnalogInputToSettingsGroup: unknown analog input '%s'", id ? id : "(null)");
        return;
    }

    AnalogInputEntry& entry = analogInputs[static_cast<size_t>(idx)];
    if (!entry.registerSettings) {
        IO_LOG("[WARNING] addAnalogInputToSettingsGroup: input '%s' is not persisted", entry.id.c_str());
        return;
    }

    const char* categoryName = (pageName && pageName[0]) ? pageName : cm::CoreCategories::IO;
    const char* categoryPretty = categoryName;

    if (!entry.settingsRegistered) {
        entry.cardKey = entry.id;
        if (groupName && groupName[0]) {
            entry.cardPretty = String(groupName);
        } else {
            entry.cardPretty = (cardName && cardName[0]) ? String(cardName) : entry.name;
        }
        entry.cardOrder = order;
        entry.settingsCategory = categoryName;

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
        entry.keyAlarmMin = formatAnalogSlotKey(entry.slot, 'A');
        entry.keyAlarmMax = formatAnalogSlotKey(entry.slot, 'B');

        entry.keyPinStable = makeStableString(entry.keyPin);
        entry.keyRawMinStable = makeStableString(entry.keyRawMin);
        entry.keyRawMaxStable = makeStableString(entry.keyRawMax);
        entry.keyOutMinStable = makeStableString(entry.keyOutMin);
        entry.keyOutMaxStable = makeStableString(entry.keyOutMax);
        entry.keyUnitStable = makeStableString(entry.keyUnit);
        entry.keyDeadbandStable = makeStableString(entry.keyDeadband);
        entry.keyMinEventMsStable = makeStableString(entry.keyMinEventMs);
        entry.keyAlarmMinStable = makeStableString(entry.keyAlarmMin);
        entry.keyAlarmMaxStable = makeStableString(entry.keyAlarmMax);

        entry.pin = &ConfigManager.addSettingInt(entry.keyPinStable->c_str())
            .name("GPIO")
            .category(categoryName)
            .defaultValue(entry.defaultPin)
            .showInWeb(entry.showPinInWeb)
            .sortOrder(31)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .build();

        entry.rawMin = &ConfigManager.addSettingInt(entry.keyRawMinStable->c_str())
            .name("Raw Min")
            .category(categoryName)
            .defaultValue(entry.defaultRawMin)
            .showInWeb(entry.showMappingInWeb)
            .sortOrder(32)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .build();

        entry.rawMax = &ConfigManager.addSettingInt(entry.keyRawMaxStable->c_str())
            .name("Raw Max")
            .category(categoryName)
            .defaultValue(entry.defaultRawMax)
            .showInWeb(entry.showMappingInWeb)
            .sortOrder(33)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .build();

        entry.outMin = &ConfigManager.addSettingFloat(entry.keyOutMinStable->c_str())
            .name("Out Min")
            .category(categoryName)
            .defaultValue(entry.defaultOutMin)
            .showInWeb(entry.showMappingInWeb)
            .sortOrder(34)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .build();

        entry.outMax = &ConfigManager.addSettingFloat(entry.keyOutMaxStable->c_str())
            .name("Out Max")
            .category(categoryName)
            .defaultValue(entry.defaultOutMax)
            .showInWeb(entry.showMappingInWeb)
            .sortOrder(35)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .build();

        entry.unit = &ConfigManager.addSettingString(entry.keyUnitStable->c_str())
            .name("Unit")
            .category(categoryName)
            .defaultValue(entry.defaultUnit)
            .showInWeb(entry.showUnitInWeb)
            .sortOrder(36)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .build();

        entry.deadband = &ConfigManager.addSettingFloat(entry.keyDeadbandStable->c_str())
            .name("Deadband")
            .category(categoryName)
            .defaultValue(entry.defaultDeadband)
            .showInWeb(entry.showDeadbandInWeb)
            .sortOrder(37)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .build();

        entry.minEventMs = &ConfigManager.addSettingInt(entry.keyMinEventMsStable->c_str())
            .name("Min Event (ms)")
            .category(categoryName)
            .defaultValue(static_cast<int>(entry.defaultMinEventMs))
            .showInWeb(entry.showMinEventInWeb)
            .sortOrder(38)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .build();

        entry.settingsRegistered = true;
    }

    ensureSettingsLayout(pageName, cardName, groupName, order);
    const String effectiveGroup = (groupName && groupName[0]) ? String(groupName) : entry.name;
    const String effectiveCard = (cardName && cardName[0]) ? String(cardName) : entry.name;

    registerSettingPlacement(entry.pin, pageName, effectiveCard, effectiveGroup);
    registerSettingPlacement(entry.rawMin, pageName, effectiveCard, effectiveGroup);
    registerSettingPlacement(entry.rawMax, pageName, effectiveCard, effectiveGroup);
    registerSettingPlacement(entry.outMin, pageName, effectiveCard, effectiveGroup);
    registerSettingPlacement(entry.outMax, pageName, effectiveCard, effectiveGroup);
    registerSettingPlacement(entry.unit, pageName, effectiveCard, effectiveGroup);
    registerSettingPlacement(entry.deadband, pageName, effectiveCard, effectiveGroup);
    registerSettingPlacement(entry.minEventMs, pageName, effectiveCard, effectiveGroup);

    ensureAnalogAlarmSettings(entry, entry.alarmMin, entry.alarmMax);
}

void IOManager::addAnalogOutputToSettings(const char* id, const char* pageName, int order)
{
    addAnalogOutputToSettingsGroup(id, pageName, pageName, pageName, order);
}

void IOManager::addAnalogOutputToSettingsGroup(const char* id, const char* pageName, const char* groupName, int order)
{
    addAnalogOutputToSettingsGroup(id, pageName, pageName, groupName, order);
}

void IOManager::addAnalogOutputToSettingsGroup(const char* id, const char* pageName, const char* cardName, const char* groupName, int order)
{
    const int idx = findAnalogOutputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] addAnalogOutputToSettingsGroup: unknown analog output '%s'", id ? id : "(null)");
        return;
    }

    AnalogOutputEntry& entry = analogOutputs[static_cast<size_t>(idx)];
    if (!entry.registerSettings) {
        IO_LOG("[WARNING] addAnalogOutputToSettingsGroup: output '%s' is not persisted", entry.id.c_str());
        return;
    }

    const char* categoryName = (pageName && pageName[0]) ? pageName : cm::CoreCategories::IO;
    const char* categoryPretty = categoryName;

    if (!entry.settingsRegistered) {
        entry.cardKey = entry.id;
        if (groupName && groupName[0]) {
            entry.cardPretty = String(groupName);
        } else {
            entry.cardPretty = (cardName && cardName[0]) ? String(cardName) : entry.name;
        }
        entry.cardOrder = order;

        entry.cardKeyStable = makeStableString(entry.cardKey);
        entry.cardPrettyStable = makeStableString(entry.cardPretty);

        entry.keyPin = formatAnalogOutputSlotKey(entry.slot, 'P');
        entry.keyPinStable = makeStableString(entry.keyPin);

        entry.pin = &ConfigManager.addSettingInt(entry.keyPinStable->c_str())
            .name("GPIO")
            .category(categoryName)
            .defaultValue(entry.defaultPin)
            .showInWeb(entry.showPinInWeb)
            .sortOrder(41)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(entry.cardOrder)
            .ioPinRole(cm::io::IOPinRole::AnalogOutput)
            .build();

        entry.settingsRegistered = true;
    }

    ensureSettingsLayout(pageName, cardName, groupName, order);
    const String effectiveGroup = (groupName && groupName[0]) ? String(groupName) : entry.name;
    const String effectiveCard = (cardName && cardName[0]) ? String(cardName) : entry.name;
    registerSettingPlacement(entry.pin, pageName, effectiveCard, effectiveGroup);
}

IOManager::LiveControlHandleBool IOManager::addDigitalInputToLive(const char* id, int order,
                                                                  const char* pageName,
                                                                  const char* cardName,
                                                                  const char* groupName,
                                                                  const char* labelOverride,
                                                                  bool alarmWhenActive)
{
    const int idx = findInputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] addDigitalInputToLive: unknown input '%s'", id ? id : "(null)");
        return {};
    }

    DigitalInputEntry& entry = digitalInputs[static_cast<size_t>(idx)];
    const char* effectiveGroupName = (groupName && groupName[0]) ? groupName : ((cardName && cardName[0]) ? cardName : "inputs");
    ensureLiveLayout(pageName, cardName, effectiveGroupName, order);

    entry.runtimeGroup = String(effectiveGroupName);
    entry.runtimeLabel = (labelOverride && labelOverride[0]) ? String(labelOverride) : entry.name;
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

    entry.runtimeRegistered = true;

    LiveControlHandleBool handle;
    handle.onChange = &entry.onChangeCallback;
    handle.onClick = &entry.callbacks.onClick;
    handle.onPress = &entry.callbacks.onPress;
    handle.onRelease = &entry.callbacks.onRelease;
    handle.onLongPress = &entry.callbacks.onLongClick;
    handle.onMultiClick = &entry.callbacks.onMultiClick;
    const String entryId = entry.id;
    handle.enableEvents = [this, entryId]() { this->enableDigitalInputEvents(entryId.c_str()); };
    return handle;
}

IOManager::LiveControlHandleBool IOManager::addDigitalOutputToLive(RuntimeControlType type,
                                                                   const char* id,
                                                                   int order,
                                                                   const char* pageName,
                                                                   const char* cardName,
                                                                   const char* groupName,
                                                                   const char* labelOverride,
                                                                   const char* onLabel,
                                                                   const char* offLabel)
{
    const int idx = findIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] addDigitalOutputToLive: unknown output '%s'", id ? id : "(null)");
        return {};
    }

    const char* effectiveGroupName = (groupName && groupName[0]) ? groupName : ((cardName && cardName[0]) ? cardName : "controls");
    ensureLiveLayout(pageName, cardName, effectiveGroupName, order);

    DigitalOutputEntry& entry = digitalOutputs[static_cast<size_t>(idx)];
    const String group = String(effectiveGroupName);
    const String label = (labelOverride && labelOverride[0]) ? String(labelOverride) : entry.name;
    const String onLabelStr = (onLabel && onLabel[0]) ? String(onLabel) : String();
    const String offLabelStr = (offLabel && offLabel[0]) ? String(offLabel) : String();

    entry.runtimeGroup = group;
    ensureOutputRuntimeProvider(entry.runtimeGroup);

    if (type == RuntimeControlType::Button) {
        ConfigManager.defineRuntimeButton(group, entry.id, label, [this, id]() {
            const int outIdx = findIndex(id);
            if (outIdx < 0) return;
            auto& outEntry = digitalOutputs[static_cast<size_t>(outIdx)];
            if (outEntry.onClickCallback) {
                outEntry.onClickCallback();
            }
        }, String(), order);
    } else {
        auto getter = [this, id]() { return this->getState(id); };
        auto setter = [this, id](bool state) {
            this->setState(id, state);
            const int outIdx = findIndex(id);
            if (outIdx < 0) return;
            auto& outEntry = digitalOutputs[static_cast<size_t>(outIdx)];
            if (outEntry.onChangeCallback) {
                outEntry.onChangeCallback(state);
            }
        };

        switch (type) {
            case RuntimeControlType::Checkbox:
                ConfigManager.defineRuntimeCheckbox(group, entry.id, label, getter, setter, String(), order);
                break;
            case RuntimeControlType::MomentaryButton:
                ConfigManager.defineRuntimeMomentaryButton(group, entry.id, label, getter, setter, String(), order, onLabelStr, offLabelStr);
                break;
            case RuntimeControlType::StateButton:
                ConfigManager.defineRuntimeStateButton(group, entry.id, label, getter, setter, false, String(), order, onLabelStr, offLabelStr);
                break;
            default:
                IO_LOG("[WARNING] addDigitalOutputToLive: unsupported control type for '%s'", entry.id.c_str());
                break;
        }
    }

    LiveControlHandleBool handle;
    handle.onChange = &entry.onChangeCallback;
    handle.onClick = &entry.onClickCallback;
    return handle;
}

IOManager::LiveControlHandleFloat IOManager::addAnalogOutputToLive(const char* id,
                                                                   int order,
                                                                   float sliderMin,
                                                                   float sliderMax,
                                                                   int sliderPrecision,
                                                                   const char* pageName,
                                                                   const char* cardName,
                                                                   const char* groupName,
                                                                   const char* labelOverride,
                                                                   const char* unit)
{
    const int idx = findAnalogOutputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] addAnalogOutputToLive: unknown analog output '%s'", id ? id : "(null)");
        return {};
    }

    const char* effectiveGroupName = (groupName && groupName[0]) ? groupName : ((cardName && cardName[0]) ? cardName : "controls");
    ensureLiveLayout(pageName, cardName, effectiveGroupName, order);

    AnalogOutputEntry& entry = analogOutputs[static_cast<size_t>(idx)];
    const String group = String(effectiveGroupName);
    const String label = (labelOverride && labelOverride[0]) ? String(labelOverride) : entry.name;
    const String unitStr = (unit && unit[0]) ? String(unit) : String();

    float initValue = sliderMin;
    ConfigManager.defineRuntimeFloatSlider(
        group,
        entry.id,
        label,
        sliderMin,
        sliderMax,
        initValue,
        sliderPrecision,
        [this, id]() { return this->getValue(id); },
        [this, id](float v) {
            this->setValue(id, v);
            const int outIdx = findAnalogOutputIndex(id);
            if (outIdx < 0) return;
            auto& outEntry = analogOutputs[static_cast<size_t>(outIdx)];
            if (outEntry.onChangeCallback) {
                outEntry.onChangeCallback(v);
            }
        },
        unitStr,
        String(),
        order);

    LiveControlHandleFloat handle;
    handle.onChange = &entry.onChangeCallback;
    return handle;
}

void IOManager::addAnalogInputToLive(const char* id, int order,
                                     const char* pageName,
                                     const char* cardName,
                                     const char* groupName,
                                     const char* labelOverride,
                                     bool showRaw)
{
    const int idx = findAnalogInputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] addAnalogInputToLive: unknown analog input '%s'", id ? id : "(null)");
        return;
    }

    AnalogInputEntry& entry = analogInputs[static_cast<size_t>(idx)];
    const char* effectiveGroupName = (groupName && groupName[0]) ? groupName : ((cardName && cardName[0]) ? cardName : "analog");
    ensureLiveLayout(pageName, cardName, effectiveGroupName, order);

    const String group = String(effectiveGroupName);
    const String label = (labelOverride && labelOverride[0]) ? String(labelOverride) : entry.name;
    registerAnalogRuntimeField(group, entry.id, showRaw);
    ensureAnalogRuntimeProvider(group);

    if (showRaw) {
        const String runtimeKey = entry.id + String("_raw");
        addAnalogRuntimeMeta(ConfigManager.getRuntime(), group, runtimeKey, label, "", 0, order, false, 0.0f, 0.0f);
    } else {
        const String unitStr = entry.unit ? entry.unit->get() : entry.defaultUnit;
        addAnalogRuntimeMeta(ConfigManager.getRuntime(), group, entry.id, label, unitStr, entry.defaultPrecision, order, false, 0.0f, 0.0f);
    }
}

void IOManager::addAnalogInputToLiveWithAlarm(const char* id, int order,
                                              float alarmMin,
                                              float alarmMax,
                                              AnalogAlarmCallbacks callbacks,
                                              const char* pageName,
                                              const char* cardName,
                                              const char* groupName,
                                              const char* labelOverride)
{
    const int idx = findAnalogInputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] addAnalogInputToLiveWithAlarm: unknown analog input '%s'", id ? id : "(null)");
        return;
    }

    AnalogInputEntry& entry = analogInputs[static_cast<size_t>(idx)];
    entry.alarmMin = alarmMin;
    entry.alarmMax = alarmMax;
    entry.alarmCallbacks = std::move(callbacks);

    const char* effectiveGroupName = (groupName && groupName[0]) ? groupName : ((cardName && cardName[0]) ? cardName : "analog");
    ensureLiveLayout(pageName, cardName, effectiveGroupName, order);

    const String group = String(effectiveGroupName);
    const String label = (labelOverride && labelOverride[0]) ? String(labelOverride) : entry.name;
    registerAnalogRuntimeField(group, entry.id, false);
    ensureAnalogRuntimeProvider(group);

    const String unitStr = entry.unit ? entry.unit->get() : entry.defaultUnit;
    addAnalogRuntimeMeta(ConfigManager.getRuntime(), group, entry.id, label, unitStr, entry.defaultPrecision, order, true, alarmMin, alarmMax);
    addAnalogAlarmRuntimeIndicators(ConfigManager.getRuntime(), group, entry.id, order, !isnan(alarmMin), !isnan(alarmMax));
}

void IOManager::addAnalogOutputValueToGUI(const char* id, const char* cardName, int order,
                                          const char* runtimeLabel,
                                          const char* runtimeGroup,
                                          const char* unit,
                                          int precision)
{
    const int idx = findAnalogOutputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] addAnalogOutputValueToGUI: unknown analog output '%s'", id ? id : "(null)");
        return;
    }

    (void)cardName;
    AnalogOutputEntry& entry = analogOutputs[static_cast<size_t>(idx)];
    const String group = (runtimeGroup && runtimeGroup[0]) ? String(runtimeGroup) : String("controls");

    const String key = entry.id + "_value";
    const String label = (runtimeLabel && runtimeLabel[0]) ? String(runtimeLabel) : (entry.name + String(" Value"));
    const String unitStr = (unit && unit[0]) ? String(unit) : String();

    registerAnalogOutputRuntimeField(group, entry.id, key, AnalogOutputRuntimeKind::ScaledValue);
    ensureAnalogOutputRuntimeProvider(group);
    addAnalogOutputRuntimeMeta(ConfigManager.getRuntime(), group, key, label, unitStr, precision, order);
}

void IOManager::addAnalogOutputValueRawToGUI(const char* id, const char* cardName, int order,
                                             const char* runtimeLabel,
                                             const char* runtimeGroup)
{
    const int idx = findAnalogOutputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] addAnalogOutputValueRawToGUI: unknown analog output '%s'", id ? id : "(null)");
        return;
    }

    (void)cardName;
    AnalogOutputEntry& entry = analogOutputs[static_cast<size_t>(idx)];
    const String group = (runtimeGroup && runtimeGroup[0]) ? String(runtimeGroup) : String("controls");

    const String key = entry.id + "_dac";
    const String label = (runtimeLabel && runtimeLabel[0]) ? String(runtimeLabel) : (entry.name + String(" DAC"));

    registerAnalogOutputRuntimeField(group, entry.id, key, AnalogOutputRuntimeKind::RawDac);
    ensureAnalogOutputRuntimeProvider(group);
    addAnalogOutputRuntimeMeta(ConfigManager.getRuntime(), group, key, label, "", 0, order);
}

void IOManager::addAnalogOutputValueVoltToGUI(const char* id, const char* cardName, int order,
                                              const char* runtimeLabel,
                                              const char* runtimeGroup,
                                              int precision)
{
    const int idx = findAnalogOutputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] addAnalogOutputValueVoltToGUI: unknown analog output '%s'", id ? id : "(null)");
        return;
    }

    (void)cardName;
    AnalogOutputEntry& entry = analogOutputs[static_cast<size_t>(idx)];
    const String group = (runtimeGroup && runtimeGroup[0]) ? String(runtimeGroup) : String("controls");

    const String key = entry.id + "_volts";
    const String label = (runtimeLabel && runtimeLabel[0]) ? String(runtimeLabel) : (entry.name + String(" Volts"));

    registerAnalogOutputRuntimeField(group, entry.id, key, AnalogOutputRuntimeKind::Volts);
    ensureAnalogOutputRuntimeProvider(group);
    addAnalogOutputRuntimeMeta(ConfigManager.getRuntime(), group, key, label, "V", precision, order);
}

bool IOManager::isValidAnalogOutputPin(int pin)
{
#if defined(ARDUINO_ARCH_ESP32)
    // DAC pins on classic ESP32
    return pin == 25 || pin == 26;
#else
    (void)pin;
    return false;
#endif
}

void IOManager::reconfigureIfNeeded(AnalogOutputEntry& entry)
{
    const int pin = entry.pin ? entry.pin->get() : entry.defaultPin;
    if (!isValidAnalogOutputPin(pin)) {
        if (!entry.warningLoggedInvalidPin) {
            IO_LOG("[WARNING] AnalogOutput '%s' has invalid/unsupported pin=%d (DAC pins are 25/26)", entry.id.c_str(), pin);
            entry.warningLoggedInvalidPin = true;
        }
        entry.hasLast = true;
        entry.lastPin = pin;
        return;
    }

    entry.warningLoggedInvalidPin = false;

    if (!entry.hasLast || entry.lastPin != pin) {
        entry.lastPin = pin;
        entry.hasLast = true;
    }
}

void IOManager::applyDesiredAnalogOutput(AnalogOutputEntry& entry)
{
    const int pin = entry.pin ? entry.pin->get() : entry.defaultPin;
    if (!isValidAnalogOutputPin(pin)) {
        return;
    }

    // Raw is volts (0..3.3V). Convert to DAC 8-bit.
    static constexpr float RAW_MIN_V = 0.0f;
    static constexpr float RAW_MAX_V = 3.3f;
    const float raw = clampFloat(entry.desiredRawVolts, RAW_MIN_V, RAW_MAX_V);

#if defined(ARDUINO_ARCH_ESP32)
    const float t = (RAW_MAX_V <= RAW_MIN_V) ? 0.0f : (raw - RAW_MIN_V) / (RAW_MAX_V - RAW_MIN_V);
    int dac = static_cast<int>(lroundf(t * 255.0f));
    if (dac < 0) {
        dac = 0;
    }
    if (dac > 255) {
        dac = 255;
    }
    dacWrite(pin, static_cast<uint8_t>(dac));
#else
    (void)raw;
    IO_LOG("[ERROR] AnalogOutput '%s': DAC output not supported on this platform", entry.id.c_str());
#endif

    entry.rawVolts = raw;
    // Keep mapped value in sync (value is always derived from the physical raw output).
    const float effectiveRaw = entry.reverse ? (RAW_MAX_V - (raw - RAW_MIN_V)) : raw;
    entry.value = clampFloat(mapFloat(effectiveRaw, RAW_MIN_V, RAW_MAX_V, entry.valueMin, entry.valueMax), entry.valueMin, entry.valueMax);
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
        entry.hasLastStateForCallback = false;

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
        entry.alarmState = false;
        entry.alarmMinState = false;
        entry.alarmMaxState = false;
        entry.alarmStateInitialized = false;
        reconfigureIfNeeded(entry);
        readAnalogInput(entry);
    }

    for (auto& entry : analogOutputs) {
        entry.hasLast = false;
        entry.warningLoggedInvalidPin = false;
        reconfigureIfNeeded(entry);
        applyDesiredAnalogOutput(entry);
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
        if (entry.onChangeCallback) {
            if (!entry.hasLastStateForCallback) {
                entry.lastStateForCallback = entry.state;
                entry.hasLastStateForCallback = true;
            } else if (entry.lastStateForCallback != entry.state) {
                entry.lastStateForCallback = entry.state;
                entry.onChangeCallback(entry.state);
            }
        }
        processInputEvents(entry, nowMs);
    }

    for (auto& entry : analogInputs) {
        reconfigureIfNeeded(entry);
        readAnalogInput(entry);
        processAnalogAlarm(entry);
        processAnalogEvents(entry, nowMs);
    }

    for (auto& entry : analogOutputs) {
        reconfigureIfNeeded(entry);
        applyDesiredAnalogOutput(entry);
    }
}

bool IOManager::setValue(const char* id, float value)
{
    const int idx = findAnalogOutputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] setValue: unknown analog output '%s'", id ? id : "(null)");
        return false;
    }

    AnalogOutputEntry& entry = analogOutputs[static_cast<size_t>(idx)];
    const float v = clampFloat(value, entry.valueMin, entry.valueMax);

    // Map value -> raw volts (0..3.3)
    static constexpr float RAW_MIN_V = 0.0f;
    static constexpr float RAW_MAX_V = 3.3f;

    float raw = mapFloat(v, entry.valueMin, entry.valueMax, RAW_MIN_V, RAW_MAX_V);
    if (entry.reverse) {
        raw = RAW_MAX_V - (raw - RAW_MIN_V);
    }

    entry.desiredValue = v;
    entry.value = v;
    entry.desiredRawVolts = clampFloat(raw, RAW_MIN_V, RAW_MAX_V);
    entry.rawVolts = entry.desiredRawVolts;

    reconfigureIfNeeded(entry);
    applyDesiredAnalogOutput(entry);
    return true;
}

float IOManager::getValue(const char* id) const
{
    const int idx = findAnalogOutputIndex(id);
    if (idx < 0) {
        return NAN;
    }
    return analogOutputs[static_cast<size_t>(idx)].value;
}

bool IOManager::setRawValue(const char* id, float rawVolts)
{
    const int idx = findAnalogOutputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] setRawValue: unknown analog output '%s'", id ? id : "(null)");
        return false;
    }

    AnalogOutputEntry& entry = analogOutputs[static_cast<size_t>(idx)];
    static constexpr float RAW_MIN_V = 0.0f;
    static constexpr float RAW_MAX_V = 3.3f;

    const float physicalRaw = clampFloat(rawVolts, RAW_MIN_V, RAW_MAX_V);
    const float effectiveRaw = entry.reverse ? (RAW_MAX_V - (physicalRaw - RAW_MIN_V)) : physicalRaw;

    // Map (effective) raw volts -> value range
    const float mapped = mapFloat(effectiveRaw, RAW_MIN_V, RAW_MAX_V, entry.valueMin, entry.valueMax);

    entry.desiredRawVolts = physicalRaw;
    entry.rawVolts = physicalRaw;
    entry.desiredValue = mapped;
    entry.value = mapped;

    reconfigureIfNeeded(entry);
    applyDesiredAnalogOutput(entry);
    return true;
}

float IOManager::getRawValue(const char* id) const
{
    const int idx = findAnalogOutputIndex(id);
    if (idx < 0) {
        return NAN;
    }
    return analogOutputs[static_cast<size_t>(idx)].rawVolts;
}

bool IOManager::setDACValue(const char* id, int dacValue)
{
    const int idx = findAnalogOutputIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] setDACValue: unknown analog output '%s'", id ? id : "(null)");
        return false;
    }

    AnalogOutputEntry& entry = analogOutputs[static_cast<size_t>(idx)];

    static constexpr int DAC_MIN = 0;
    static constexpr int DAC_MAX = 255;
    static constexpr float RAW_MIN_V = 0.0f;
    static constexpr float RAW_MAX_V = 3.3f;

    const int clamped = std::max(DAC_MIN, std::min(DAC_MAX, dacValue));
    const float t = static_cast<float>(clamped) / static_cast<float>(DAC_MAX);
    const float physicalRaw = RAW_MIN_V + t * (RAW_MAX_V - RAW_MIN_V);

    const float effectiveRaw = entry.reverse ? (RAW_MAX_V - (physicalRaw - RAW_MIN_V)) : physicalRaw;
    const float mapped = mapFloat(effectiveRaw, RAW_MIN_V, RAW_MAX_V, entry.valueMin, entry.valueMax);

    entry.desiredRawVolts = physicalRaw;
    entry.rawVolts = physicalRaw;
    entry.desiredValue = mapped;
    entry.value = mapped;

    reconfigureIfNeeded(entry);
    applyDesiredAnalogOutput(entry);
    return true;
}

int IOManager::getDACValue(const char* id) const
{
    const int idx = findAnalogOutputIndex(id);
    if (idx < 0) {
        return -1;
    }

    static constexpr float RAW_MIN_V = 0.0f;
    static constexpr float RAW_MAX_V = 3.3f;
    static constexpr float DAC_MAX = 255.0f;

    const float physicalRaw = analogOutputs[static_cast<size_t>(idx)].rawVolts;
    if (isnan(physicalRaw)) {
        return -1;
    }

    const float clampedRaw = clampFloat(physicalRaw, RAW_MIN_V, RAW_MAX_V);
    const float t = (RAW_MAX_V <= RAW_MIN_V) ? 0.0f : (clampedRaw - RAW_MIN_V) / (RAW_MAX_V - RAW_MIN_V);
    const int code = static_cast<int>(lroundf(t * DAC_MAX));
    return std::max(0, std::min(255, code));
}

void IOManager::processAnalogAlarm(AnalogInputEntry& entry)
{
    const float alarmMin = entry.alarmMinSetting ? entry.alarmMinSetting->get() : entry.alarmMin;
    const float alarmMax = entry.alarmMaxSetting ? entry.alarmMaxSetting->get() : entry.alarmMax;

    const bool hasMin = !isnan(alarmMin);
    const bool hasMax = !isnan(alarmMax);
    if (!hasMin && !hasMax) {
        // No alarm configured.
        entry.alarmState = false;
        entry.alarmMinState = false;
        entry.alarmMaxState = false;
        entry.alarmStateInitialized = true;
        return;
    }

    bool newMinState = false;
    bool newMaxState = false;
    if (isnan(entry.value)) {
        newMinState = false;
        newMaxState = false;
    } else {
        if (hasMin && entry.value < alarmMin) {
            newMinState = true;
        }
        if (hasMax && entry.value > alarmMax) {
            newMaxState = true;
        }
    }

    const bool newCombinedState = newMinState || newMaxState;

    if (!entry.alarmStateInitialized) {
        // First evaluation: treat previous state as "not in alarm" and only fire if we start in alarm.
        entry.alarmStateInitialized = true;
        entry.alarmState = false;
        entry.alarmMinState = false;
        entry.alarmMaxState = false;

        if (newMinState) {
            entry.alarmMinState = true;
            if (entry.alarmCallbacks.onMinStateChanged) {
                entry.alarmCallbacks.onMinStateChanged(true);
            }
            if (entry.alarmCallbacks.onMinEnter) {
                entry.alarmCallbacks.onMinEnter();
            }
        }
        if (newMaxState) {
            entry.alarmMaxState = true;
            if (entry.alarmCallbacks.onMaxStateChanged) {
                entry.alarmCallbacks.onMaxStateChanged(true);
            }
            if (entry.alarmCallbacks.onMaxEnter) {
                entry.alarmCallbacks.onMaxEnter();
            }
        }

        if (newCombinedState) {
            entry.alarmState = true;
            if (entry.alarmCallbacks.onStateChanged) {
                entry.alarmCallbacks.onStateChanged(true);
            }
            if (entry.alarmCallbacks.onEnter) {
                entry.alarmCallbacks.onEnter();
            }
        }
        return;
    }

    if (newMinState != entry.alarmMinState) {
        entry.alarmMinState = newMinState;
        if (entry.alarmCallbacks.onMinStateChanged) {
            entry.alarmCallbacks.onMinStateChanged(newMinState);
        }
        if (newMinState) {
            if (entry.alarmCallbacks.onMinEnter) {
                entry.alarmCallbacks.onMinEnter();
            }
        } else {
            if (entry.alarmCallbacks.onMinExit) {
                entry.alarmCallbacks.onMinExit();
            }
        }
    }

    if (newMaxState != entry.alarmMaxState) {
        entry.alarmMaxState = newMaxState;
        if (entry.alarmCallbacks.onMaxStateChanged) {
            entry.alarmCallbacks.onMaxStateChanged(newMaxState);
        }
        if (newMaxState) {
            if (entry.alarmCallbacks.onMaxEnter) {
                entry.alarmCallbacks.onMaxEnter();
            }
        } else {
            if (entry.alarmCallbacks.onMaxExit) {
                entry.alarmCallbacks.onMaxExit();
            }
        }
    }

    if (newCombinedState == entry.alarmState) {
        return;
    }

    entry.alarmState = newCombinedState;

    if (entry.alarmCallbacks.onStateChanged) {
        entry.alarmCallbacks.onStateChanged(newCombinedState);
    }

    if (newCombinedState) {
        if (entry.alarmCallbacks.onEnter) {
            entry.alarmCallbacks.onEnter();
        }
    } else {
        if (entry.alarmCallbacks.onExit) {
            entry.alarmCallbacks.onExit();
        }
    }
}

bool IOManager::setState(const char* id, bool on)
{
    const int idx = findIndex(id);
    if (idx < 0) {
        IO_LOG("[WARNING] setState: unknown output '%s'", id ? id : "(null)");
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

int IOManager::findAnalogOutputIndex(const char* id) const
{
    if (!id || !id[0]) {
        return -1;
    }

    for (size_t i = 0; i < analogOutputs.size(); ++i) {
        if (analogOutputs[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
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
        IO_LOG("[WARNING] Input '%s': pull-up and pull-down both enabled, using pull-up", entry.id.c_str());
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
            IO_LOG("[WARNING] Analog input '%s' uses ADC2 pin %d; readings may be unreliable while WiFi is active", entry.id.c_str(), pin);
        }
        entry.warningLoggedInvalidPin = true;
    }
}

void IOManager::readAnalogInput(AnalogInputEntry& entry)
{
    const int pin = getAnalogPinNow(entry);
    if (!isValidAnalogPin(pin)) {
        if (!entry.warningLoggedInvalidPin) {
            IO_LOG("[WARNING] Analog input '%s' pin %d is not ADC-capable on ESP32", entry.id.c_str(), pin);
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
        const AnalogRuntimeGroup* runtimeGroupPtr = nullptr;
        for (const auto& rg : analogRuntimeGroups) {
            if (rg.group == group) {
                runtimeGroupPtr = &rg;
                break;
            }
        }
        if (!runtimeGroupPtr) {
            return;
        }

        for (const auto& field : runtimeGroupPtr->fields) {
            const int idx = findAnalogInputIndex(field.id.c_str());
            if (idx < 0) {
                continue;
            }

            const AnalogInputEntry& entry = analogInputs[static_cast<size_t>(idx)];
            if (field.showRaw) {
                const String rawKey = entry.id + "_raw";
                if (entry.rawValue < 0) {
                    data[rawKey] = nullptr;
                } else {
                    data[rawKey] = entry.rawValue;
                }
            } else {
                if (isnan(entry.value)) {
                    data[entry.id] = nullptr;
                } else {
                    data[entry.id] = entry.value;
                }

                const String minKey = entry.id + "_alarm_min";
                const String maxKey = entry.id + "_alarm_max";
                data[minKey] = entry.alarmMinState;
                data[maxKey] = entry.alarmMaxState;
            }
        }
    }, 5);

    registered.push_back(group);
}

void IOManager::registerAnalogOutputRuntimeField(const String& group, const String& id, const String& key, AnalogOutputRuntimeKind kind)
{
    for (auto& rg : analogOutputRuntimeGroups) {
        if (rg.group == group) {
            for (const auto& f : rg.fields) {
                if (f.key == key) {
                    return;
                }
            }
            rg.fields.push_back(AnalogOutputRuntimeField{ id, key, kind });
            return;
        }
    }

    AnalogOutputRuntimeGroup newGroup;
    newGroup.group = group;
    newGroup.fields.push_back(AnalogOutputRuntimeField{ id, key, kind });
    analogOutputRuntimeGroups.push_back(std::move(newGroup));
}

void IOManager::ensureAnalogOutputRuntimeProvider(const String& group)
{
    static std::vector<String> registered;
    for (const auto& g : registered) {
        if (g == group) return;
    }

    ConfigManager.getRuntime().addRuntimeProvider(group, [this, group](JsonObject& data) {
        const AnalogOutputRuntimeGroup* runtimeGroupPtr = nullptr;
        for (const auto& rg : analogOutputRuntimeGroups) {
            if (rg.group == group) {
                runtimeGroupPtr = &rg;
                break;
            }
        }
        if (!runtimeGroupPtr) {
            return;
        }

        for (const auto& field : runtimeGroupPtr->fields) {
            const int idx = findAnalogOutputIndex(field.id.c_str());
            if (idx < 0) {
                continue;
            }

            const AnalogOutputEntry& entry = analogOutputs[static_cast<size_t>(idx)];
            switch (field.kind) {
                case AnalogOutputRuntimeKind::ScaledValue:
                    data[field.key] = entry.value;
                    break;
                case AnalogOutputRuntimeKind::RawDac:
                    data[field.key] = this->getDACValue(entry.id.c_str());
                    break;
                case AnalogOutputRuntimeKind::Volts:
                    data[field.key] = entry.rawVolts;
                    break;
                default:
                    break;
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

void IOManager::resetDigitalInputEventState(DigitalInputEntry& entry, uint32_t nowMs)
{
    entry.rawState = entry.state;
    entry.debouncedState = entry.state;
    entry.lastRawChangeMs = nowMs;
    entry.pressStartMs = 0;
    entry.longFired = false;
    entry.clickCount = 0;
    entry.lastReleaseMs = 0;
}

void IOManager::enableDigitalInputEvents(const char* id)
{
    const int idx = findInputIndex(id);
    if (idx < 0) {
        return;
    }

    DigitalInputEntry& entry = digitalInputs[static_cast<size_t>(idx)];
    if (!entry.eventsEnabled) {
        entry.eventsEnabled = true;
        resetDigitalInputEventState(entry, millis());
    }
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
                    if (entry.clickCount < 255) {
                        entry.clickCount = static_cast<uint8_t>(entry.clickCount + 1);
                    }
                    entry.lastReleaseMs = nowMs;
                    if (!entry.callbacks.onMultiClick && entry.clickCount >= 2) {
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
    if (!entry.debouncedState && entry.clickCount > 0) {
        if (nowMs - entry.lastReleaseMs >= entry.eventOptions.doubleClickMs) {
            if (entry.callbacks.onMultiClick) {
                entry.callbacks.onMultiClick(entry.clickCount);
            } else if (entry.clickCount == 1 && entry.callbacks.onClick) {
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

void IOManager::ensureOutputRuntimeProvider(const String& group)
{
    static std::vector<String> registered;
    for (const auto& g : registered) {
        if (g == group) return;
    }

    ConfigManager.getRuntime().addRuntimeProvider(group, [this, group](JsonObject& data) {
        for (const auto& entry : digitalOutputs) {
            if (entry.runtimeGroup == group) {
                data[entry.id] = entry.desiredState;
            }
        }
    }, 5);

    registered.push_back(group);
}

} // namespace cm
