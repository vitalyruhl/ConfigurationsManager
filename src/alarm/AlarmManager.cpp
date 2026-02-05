#include "AlarmManager.h"

#include <algorithm>
#include <cmath>

#define ALARM_LOG(...) CM_LOG("[ALARM] " __VA_ARGS__)
#define ALARM_LOG_VERBOSE(...) CM_LOG_VERBOSE("[ALARM] " __VA_ARGS__)

namespace cm {

namespace {
static std::shared_ptr<std::string> makeStableString(const String& value)
{
    return std::make_shared<std::string>(value.c_str());
}

static constexpr const char* DEFAULT_LIVE_PAGE = "Live";
static constexpr const char* DEFAULT_LIVE_CARD = "Alarms";
static constexpr int DEFAULT_LIVE_ORDER = 1;

static constexpr const char* COLOR_OK = "#1f7a3a";
static constexpr const char* COLOR_ALARM = "#8a1b2d";
static constexpr const char* COLOR_WARN = "#c78a1a";
static constexpr const char* ALARM_PULSE = "alarmPulse 1.1s ease-in-out infinite";

static void registerSettingPlacement(BaseSetting* setting,
                                     const char* pageName,
                                     const char* cardName,
                                     const char* groupName)
{
    if (!setting || !setting->shouldShowInWeb()) {
        return;
    }
    ConfigManager.addToSettingsGroup(setting->getKey(), pageName, cardName, groupName, setting->getSortOrder());
}

static bool isFiniteFloat(float value)
{
    return !std::isnan(value) && std::isfinite(value);
}
} // namespace

AlarmManager::AlarmHandle::AlarmHandle(AlarmManager* owner, size_t index)
    : manager(owner), entryIndex(index)
{
}

AlarmManager::AlarmHandle& AlarmManager::AlarmHandle::onAlarmCome(std::function<void()> cb)
{
    if (manager) {
        if (auto* entry = manager->findAlarmByIndex(entryIndex)) {
            entry->onEnter = std::move(cb);
        }
    }
    return *this;
}

AlarmManager::AlarmHandle& AlarmManager::AlarmHandle::onAlarmGone(std::function<void()> cb)
{
    if (manager) {
        if (auto* entry = manager->findAlarmByIndex(entryIndex)) {
            entry->onExit = std::move(cb);
        }
    }
    return *this;
}

AlarmManager::AlarmHandle& AlarmManager::AlarmHandle::onAlarmStay(std::function<void()> cb,
                                                                  std::chrono::milliseconds interval)
{
    if (manager) {
        if (auto* entry = manager->findAlarmByIndex(entryIndex)) {
            entry->onStay = std::move(cb);
            if (interval.count() < 0) {
                interval = std::chrono::milliseconds(0);
            }
            entry->stayInterval = interval;
        }
    }
    return *this;
}

AlarmManager::AlarmHandle& AlarmManager::AlarmHandle::onStateChanged(std::function<void(bool)> cb)
{
    if (manager) {
        if (auto* entry = manager->findAlarmByIndex(entryIndex)) {
            entry->onStateChanged = std::move(cb);
        }
    }
    return *this;
}

AlarmManager::AlarmHandle& AlarmManager::AlarmHandle::onStateCodeChanged(std::function<void(AlarmState)> cb)
{
    if (manager) {
        if (auto* entry = manager->findAlarmByIndex(entryIndex)) {
            entry->onStateCodeChanged = std::move(cb);
        }
    }
    return *this;
}

AlarmManager::AlarmHandle& AlarmManager::AlarmHandle::bindActive(bool* target)
{
    if (manager) {
        if (auto* entry = manager->findAlarmByIndex(entryIndex)) {
            entry->boundActive = target;
            if (target) {
                *target = entry->active;
            }
        }
    }
    return *this;
}

AlarmManager::AlarmHandle& AlarmManager::AlarmHandle::bindState(AlarmState* target)
{
    if (manager) {
        if (auto* entry = manager->findAlarmByIndex(entryIndex)) {
            entry->boundState = target;
            if (target) {
                *target = entry->state;
            }
        }
    }
    return *this;
}

AlarmManager::AlarmHandle& AlarmManager::AlarmHandle::setStyle(const RuntimeFieldStyle& style)
{
    if (manager) {
        if (auto* entry = manager->findAlarmByIndex(entryIndex)) {
            entry->style = style;
            manager->updateLiveMetaStyle(*entry);
        }
    }
    return *this;
}

AlarmManager::AlarmHandle& AlarmManager::AlarmHandle::addCSSClass(const char* target, const char* cssClass)
{
    if (manager) {
        if (auto* entry = manager->findAlarmByIndex(entryIndex)) {
            entry->style.rule(target).addCSSClass(cssClass);
            manager->updateLiveMetaStyle(*entry);
        }
    }
    return *this;
}

AlarmManager::AlarmHandle& AlarmManager::AlarmHandle::setDisplayColors(const char* normalColor,
                                                                       const char* alarmColor,
                                                                       bool blink)
{
    if (manager) {
        if (auto* entry = manager->findAlarmByIndex(entryIndex)) {
            entry->style.rule("stateDotOnFalse").set("background", normalColor ? normalColor : COLOR_OK).set("border", "none");
            entry->style.rule("stateDotOnTrue").set("background", normalColor ? normalColor : COLOR_OK).set("border", "none");
            entry->style.rule("stateDotOnAlarm").set("background", alarmColor ? alarmColor : COLOR_ALARM)
                .set("border", "none")
                .set("animation", blink ? ALARM_PULSE : "none");
            manager->updateLiveMetaStyle(*entry);
        }
    }
    return *this;
}

AlarmManager::AlarmHandle AlarmManager::addDigitalAlarm(const DigitalAlarmConfig& cfg)
{
    if (!cfg.id || !cfg.id[0]) {
        ALARM_LOG("[ERROR] addDigitalAlarm: missing id");
        return AlarmHandle();
    }
    if (!cfg.getter && !cfg.source) {
        ALARM_LOG("[ERROR] addDigitalAlarm: missing source for '%s'", cfg.id);
        return AlarmHandle();
    }
    if (findAlarm(cfg.id)) {
        ALARM_LOG("[WARNING] addDigitalAlarm: alarm '%s' already exists", cfg.id);
        return AlarmHandle();
    }
    if (!isDigitalKind(cfg.kind)) {
        ALARM_LOG("[WARNING] addDigitalAlarm: alarm '%s' uses non-digital kind", cfg.id);
    }

    AlarmEntry entry;
    entry.id = cfg.id;
    entry.name = cfg.name ? cfg.name : cfg.id;
    entry.kind = cfg.kind;
    entry.severity = cfg.severity;
    entry.enabledDefault = cfg.enabled;
    entry.digitalGetter = cfg.getter;
    entry.digitalSource = cfg.source;
    entry.style = (cfg.severity == AlarmSeverity::Alarm) ? buildAlarmStyle() : buildWarningStyle();

    alarms.push_back(std::move(entry));
    ALARM_LOG_VERBOSE("Registered digital alarm '%s' (%s)", alarms.back().name.c_str(), alarms.back().id.c_str());
    return AlarmHandle(this, alarms.size() - 1);
}

AlarmManager::AlarmHandle AlarmManager::addAnalogAlarm(const AnalogAlarmConfig& cfg)
{
    if (!cfg.id || !cfg.id[0]) {
        ALARM_LOG("[ERROR] addAnalogAlarm: missing id");
        return AlarmHandle();
    }
    if (!cfg.getter && !cfg.source) {
        ALARM_LOG("[ERROR] addAnalogAlarm: missing source for '%s'", cfg.id);
        return AlarmHandle();
    }
    if (findAlarm(cfg.id)) {
        ALARM_LOG("[WARNING] addAnalogAlarm: alarm '%s' already exists", cfg.id);
        return AlarmHandle();
    }
    if (!isAnalogKind(cfg.kind)) {
        ALARM_LOG("[WARNING] addAnalogAlarm: alarm '%s' uses non-analog kind", cfg.id);
    }

    AlarmEntry entry;
    entry.id = cfg.id;
    entry.name = cfg.name ? cfg.name : cfg.id;
    entry.kind = cfg.kind;
    entry.severity = cfg.severity;
    entry.enabledDefault = cfg.enabled;
    entry.minActiveDefault = cfg.minActive;
    entry.maxActiveDefault = cfg.maxActive;
    entry.thresholdMin = cfg.thresholdMin;
    entry.thresholdMax = cfg.thresholdMax;
    entry.analogGetter = cfg.getter;
    entry.analogSource = cfg.source;
    entry.style = (cfg.severity == AlarmSeverity::Alarm) ? buildAlarmStyle() : buildWarningStyle();

    alarms.push_back(std::move(entry));
    ALARM_LOG_VERBOSE("Registered analog alarm '%s' (%s)", alarms.back().name.c_str(), alarms.back().id.c_str());
    return AlarmHandle(this, alarms.size() - 1);
}

AlarmManager::AlarmHandle AlarmManager::addDigitalAlarm(const char* id,
                                                        const char* name,
                                                        std::function<bool()> getter,
                                                        AlarmKind kind,
                                                        bool enabled,
                                                        AlarmSeverity severity)
{
    DigitalAlarmConfig cfg;
    cfg.id = id;
    cfg.name = name;
    cfg.kind = kind;
    cfg.enabled = enabled;
    cfg.severity = severity;
    cfg.getter = std::move(getter);
    return addDigitalAlarm(cfg);
}

AlarmManager::AlarmHandle AlarmManager::addDigitalAlarm(const char* id,
                                                        const char* name,
                                                        const bool* source,
                                                        AlarmKind kind,
                                                        bool enabled,
                                                        AlarmSeverity severity)
{
    DigitalAlarmConfig cfg;
    cfg.id = id;
    cfg.name = name;
    cfg.kind = kind;
    cfg.enabled = enabled;
    cfg.severity = severity;
    cfg.source = source;
    return addDigitalAlarm(cfg);
}

AlarmManager::AlarmHandle AlarmManager::addAnalogAlarm(const char* id,
                                                       const char* name,
                                                       std::function<float()> getter,
                                                       AlarmKind kind,
                                                       float thresholdMin,
                                                       float thresholdMax,
                                                       bool minActive,
                                                       bool maxActive,
                                                       bool enabled,
                                                       AlarmSeverity severity)
{
    AnalogAlarmConfig cfg;
    cfg.id = id;
    cfg.name = name;
    cfg.kind = kind;
    cfg.thresholdMin = thresholdMin;
    cfg.thresholdMax = thresholdMax;
    cfg.minActive = minActive;
    cfg.maxActive = maxActive;
    cfg.enabled = enabled;
    cfg.severity = severity;
    cfg.getter = std::move(getter);
    return addAnalogAlarm(cfg);
}

AlarmManager::AlarmHandle AlarmManager::addAnalogAlarm(const char* id,
                                                       const char* name,
                                                       const float* source,
                                                       AlarmKind kind,
                                                       float thresholdMin,
                                                       float thresholdMax,
                                                       bool minActive,
                                                       bool maxActive,
                                                       bool enabled,
                                                       AlarmSeverity severity)
{
    AnalogAlarmConfig cfg;
    cfg.id = id;
    cfg.name = name;
    cfg.kind = kind;
    cfg.thresholdMin = thresholdMin;
    cfg.thresholdMax = thresholdMax;
    cfg.minActive = minActive;
    cfg.maxActive = maxActive;
    cfg.enabled = enabled;
    cfg.severity = severity;
    cfg.source = source;
    return addAnalogAlarm(cfg);
}

AlarmManager::AlarmHandle AlarmManager::addDigitalWarning(const DigitalAlarmConfig& cfg)
{
    DigitalAlarmConfig adjusted = cfg;
    adjusted.severity = AlarmSeverity::Warning;
    return addDigitalAlarm(adjusted);
}

AlarmManager::AlarmHandle AlarmManager::addAnalogWarning(const AnalogAlarmConfig& cfg)
{
    AnalogAlarmConfig adjusted = cfg;
    adjusted.severity = AlarmSeverity::Warning;
    return addAnalogAlarm(adjusted);
}

void AlarmManager::addAlarmToLive(const char* alarmId,
                                  int order,
                                  const char* pageName,
                                  const char* cardName,
                                  const char* groupName,
                                  const char* labelOverride)
{
    auto* entry = findAlarm(alarmId);
    if (!entry) {
        ALARM_LOG("[WARNING] addAlarmToLive: unknown alarm '%s'", alarmId ? alarmId : "(null)");
        return;
    }
    registerPlacement(*entry, pageName, cardName, groupName, labelOverride, order, entry->severity);
}

void AlarmManager::addWarningToLive(const char* alarmId,
                                    int order,
                                    const char* pageName,
                                    const char* cardName,
                                    const char* groupName,
                                    const char* labelOverride)
{
    auto* entry = findAlarm(alarmId);
    if (!entry) {
        ALARM_LOG("[WARNING] addWarningToLive: unknown alarm '%s'", alarmId ? alarmId : "(null)");
        return;
    }
    registerPlacement(*entry, pageName, cardName, groupName, labelOverride, order, AlarmSeverity::Warning);
}

void AlarmManager::addAlarmToSettingsGroup(const char* alarmId,
                                           const char* pageName,
                                           const char* groupName,
                                           int order)
{
    addAlarmToSettingsGroup(alarmId, pageName, pageName, groupName, order);
}

void AlarmManager::addAlarmToSettingsGroup(const char* alarmId,
                                           const char* pageName,
                                           const char* cardName,
                                           const char* groupName,
                                           int order)
{
    auto* entry = findAlarm(alarmId);
    if (!entry) {
        ALARM_LOG("[WARNING] addAlarmToSettingsGroup: unknown alarm '%s'", alarmId ? alarmId : "(null)");
        return;
    }
    registerSettings(*entry, pageName, cardName, groupName, order);
}

AlarmManager::AlarmEntry* AlarmManager::findAlarm(const char* id)
{
    if (!id || !id[0]) {
        return nullptr;
    }
    for (auto& entry : alarms) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

const AlarmManager::AlarmEntry* AlarmManager::findAlarm(const char* id) const
{
    if (!id || !id[0]) {
        return nullptr;
    }
    for (const auto& entry : alarms) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

AlarmManager::AlarmEntry* AlarmManager::findAlarmByIndex(size_t index)
{
    if (index >= alarms.size()) {
        return nullptr;
    }
    return &alarms[index];
}

bool AlarmManager::isAnalogKind(AlarmKind kind)
{
    return kind == AlarmKind::AnalogBelow || kind == AlarmKind::AnalogAbove || kind == AlarmKind::AnalogOutsideWindow;
}

bool AlarmManager::isDigitalKind(AlarmKind kind)
{
    return kind == AlarmKind::DigitalActive || kind == AlarmKind::DigitalInactive;
}

RuntimeFieldStyle AlarmManager::buildDefaultStyle(AlarmSeverity severity, bool blink)
{
    const char* alarmColor = (severity == AlarmSeverity::Alarm) ? COLOR_ALARM : COLOR_WARN;
    RuntimeFieldStyle style;
    style.rule("stateDotOnFalse").set("background", COLOR_OK).set("border", "none");
    style.rule("stateDotOnTrue").set("background", COLOR_OK).set("border", "none");
    style.rule("stateDotOnAlarm")
        .set("background", alarmColor)
        .set("border", "none")
        .set("animation", blink ? ALARM_PULSE : "none");
    return style;
}

RuntimeFieldStyle AlarmManager::buildWarningStyle()
{
    return buildDefaultStyle(AlarmSeverity::Warning, false);
}

RuntimeFieldStyle AlarmManager::buildAlarmStyle()
{
    return buildDefaultStyle(AlarmSeverity::Alarm, true);
}

void AlarmManager::ensureLiveGroupProvider(const String& group)
{
    for (auto& liveGroup : liveGroups) {
        if (liveGroup.group == group) {
            return;
        }
    }

    AlarmLiveGroup newGroup;
    newGroup.group = group;
    liveGroups.push_back(std::move(newGroup));

    ConfigManager.getRuntime().addRuntimeProvider(group, [this, group](JsonObject& data) {
        const AlarmLiveGroup* target = nullptr;
        for (const auto& entry : liveGroups) {
            if (entry.group == group) {
                target = &entry;
                break;
            }
        }
        if (!target) {
            return;
        }
        for (const auto idx : target->entries) {
            if (idx >= alarms.size()) {
                continue;
            }
            const auto& alarm = alarms[idx];
            data[alarm.id] = alarm.active;
        }
    });
}

void AlarmManager::updateLiveMetaStyle(const AlarmEntry& entry)
{
    for (const auto& placement : entry.placements) {
        if (placement.severity != entry.severity) {
            continue;
        }
        RuntimeFieldMeta* meta = ConfigManager.getRuntime().findRuntimeMeta(placement.group, placement.key);
        if (meta) {
            meta->style = entry.style;
        }
    }
}

void AlarmManager::registerSettings(AlarmEntry& entry,
                                    const char* pageName,
                                    const char* cardName,
                                    const char* groupName,
                                    int order)
{
    const char* categoryName = (pageName && pageName[0]) ? pageName : DEFAULT_LIVE_CARD;
    const char* categoryPretty = categoryName;
    if (!entry.settingsRegistered) {
        const String cardKey = entry.id;
        const String cardPretty = (groupName && groupName[0])
            ? String(groupName)
            : ((cardName && cardName[0]) ? String(cardName) : entry.name);

        entry.keyEnabledStable = makeStableString(String("Alarm.") + entry.id + ".Enabled");
        entry.keyMinActiveStable = makeStableString(String("Alarm.") + entry.id + ".MinActive");
        entry.keyMaxActiveStable = makeStableString(String("Alarm.") + entry.id + ".MaxActive");
        entry.keyThresholdMinStable = makeStableString(String("Alarm.") + entry.id + ".Min");
        entry.keyThresholdMaxStable = makeStableString(String("Alarm.") + entry.id + ".Max");

        entry.cardKeyStable = makeStableString(cardKey);
        entry.cardPrettyStable = makeStableString(cardPretty);

        ALARM_LOG_VERBOSE("Registering settings for alarm '%s' (%s)", entry.name.c_str(), entry.id.c_str());

        entry.enabledSetting = &ConfigManager.addSettingBool(entry.keyEnabledStable->c_str())
            .name("Enabled")
            .category(categoryName)
            .defaultValue(entry.enabledDefault)
            .showInWeb(true)
            .sortOrder(10)
            .categoryPretty(categoryPretty)
            .card(entry.cardKeyStable->c_str())
            .cardPretty(entry.cardPrettyStable->c_str())
            .cardOrder(order)
            .build();

        if (isAnalogKind(entry.kind)) {
            entry.minActiveSetting = &ConfigManager.addSettingBool(entry.keyMinActiveStable->c_str())
                .name("Min Active")
                .category(categoryName)
                .defaultValue(entry.minActiveDefault)
                .showInWeb(entry.kind != AlarmKind::AnalogAbove)
                .sortOrder(11)
                .categoryPretty(categoryPretty)
                .card(entry.cardKeyStable->c_str())
                .cardPretty(entry.cardPrettyStable->c_str())
                .cardOrder(order)
                .build();

            entry.maxActiveSetting = &ConfigManager.addSettingBool(entry.keyMaxActiveStable->c_str())
                .name("Max Active")
                .category(categoryName)
                .defaultValue(entry.maxActiveDefault)
                .showInWeb(entry.kind != AlarmKind::AnalogBelow)
                .sortOrder(12)
                .categoryPretty(categoryPretty)
                .card(entry.cardKeyStable->c_str())
                .cardPretty(entry.cardPrettyStable->c_str())
                .cardOrder(order)
                .build();

            entry.thresholdMinSetting = &ConfigManager.addSettingFloat(entry.keyThresholdMinStable->c_str())
                .name("Alarm Min")
                .category(categoryName)
                .defaultValue(entry.thresholdMin)
                .showInWeb(entry.kind != AlarmKind::AnalogAbove)
                .sortOrder(20)
                .categoryPretty(categoryPretty)
                .card(entry.cardKeyStable->c_str())
                .cardPretty(entry.cardPrettyStable->c_str())
                .cardOrder(order)
                .build();

            entry.thresholdMaxSetting = &ConfigManager.addSettingFloat(entry.keyThresholdMaxStable->c_str())
                .name("Alarm Max")
                .category(categoryName)
                .defaultValue(entry.thresholdMax)
                .showInWeb(entry.kind != AlarmKind::AnalogBelow)
                .sortOrder(21)
                .categoryPretty(categoryPretty)
                .card(entry.cardKeyStable->c_str())
                .cardPretty(entry.cardPrettyStable->c_str())
                .cardOrder(order)
                .build();
        }

        entry.settingsRegistered = true;
    }

    const String effectiveGroup = (groupName && groupName[0]) ? String(groupName) : entry.name;
    const String effectiveCard = (cardName && cardName[0]) ? String(cardName) : entry.name;

    ConfigManager.addSettingsPage(categoryName, order);
    ConfigManager.addSettingsCard(categoryName, effectiveCard.c_str(), order);
    ConfigManager.addSettingsGroup(categoryName, effectiveCard.c_str(), effectiveGroup.c_str(), order);
    registerSettingPlacement(entry.enabledSetting, categoryName, effectiveCard.c_str(), effectiveGroup.c_str());
    registerSettingPlacement(entry.minActiveSetting, categoryName, effectiveCard.c_str(), effectiveGroup.c_str());
    registerSettingPlacement(entry.maxActiveSetting, categoryName, effectiveCard.c_str(), effectiveGroup.c_str());
    registerSettingPlacement(entry.thresholdMinSetting, categoryName, effectiveCard.c_str(), effectiveGroup.c_str());
    registerSettingPlacement(entry.thresholdMaxSetting, categoryName, effectiveCard.c_str(), effectiveGroup.c_str());
}

void AlarmManager::registerPlacement(AlarmEntry& entry,
                                     const char* pageName,
                                     const char* cardName,
                                     const char* groupName,
                                     const char* labelOverride,
                                     int order,
                                     AlarmSeverity severity)
{
    const char* effectivePage = (pageName && pageName[0]) ? pageName : DEFAULT_LIVE_PAGE;
    const char* effectiveCard = (cardName && cardName[0]) ? cardName : DEFAULT_LIVE_CARD;
    const char* effectiveGroup = (groupName && groupName[0]) ? groupName : effectiveCard;

    const int resolvedOrder = (order <= 0) ? DEFAULT_LIVE_ORDER : order;
    ConfigManager.addLivePage(effectivePage, resolvedOrder);
    ConfigManager.addLiveCard(effectivePage, effectiveCard, resolvedOrder);
    ConfigManager.addLiveGroup(effectivePage, effectiveCard, effectiveGroup, resolvedOrder);

    RuntimeFieldMeta meta;
    meta.group = effectiveGroup;
    meta.sourceGroup = effectiveGroup;
    meta.page = effectivePage;
    meta.card = effectiveCard;
    meta.key = entry.id;
    meta.label = labelOverride && labelOverride[0] ? labelOverride : entry.name;
    meta.isBool = true;
    meta.hasAlarm = true;
    meta.boolAlarmValue = true;
    meta.alarmWhenTrue = true;
    meta.order = resolvedOrder;
    meta.style = (severity == entry.severity) ? entry.style : buildDefaultStyle(severity, severity == AlarmSeverity::Alarm);

    RuntimeFieldMeta* existingMeta = ConfigManager.getRuntime().findRuntimeMeta(meta.group, meta.key);
    if (existingMeta) {
        existingMeta->label = meta.label;
        existingMeta->order = meta.order;
        existingMeta->style = meta.style;
        existingMeta->boolAlarmValue = meta.boolAlarmValue;
        existingMeta->hasAlarm = meta.hasAlarm;
        existingMeta->alarmWhenTrue = meta.alarmWhenTrue;
    } else {
        ConfigManager.getRuntime().addRuntimeMeta(meta);
    }

    bool placementFound = false;
    for (auto& placement : entry.placements) {
        if (placement.group == effectiveGroup && placement.key == entry.id && placement.severity == severity) {
            placement.label = meta.label;
            placement.order = resolvedOrder;
            placementFound = true;
            break;
        }
    }
    if (!placementFound) {
        AlarmPlacement placement;
        placement.group = effectiveGroup;
        placement.key = entry.id;
        placement.label = meta.label;
        placement.order = resolvedOrder;
        placement.severity = severity;
        entry.placements.push_back(std::move(placement));
    }

    ensureLiveGroupProvider(effectiveGroup);
    const size_t entryIndex = static_cast<size_t>(&entry - &alarms[0]);
    for (auto& group : liveGroups) {
        if (group.group == effectiveGroup) {
            if (std::find(group.entries.begin(), group.entries.end(), entryIndex) == group.entries.end()) {
                group.entries.push_back(entryIndex);
            }
            break;
        }
    }
}

bool AlarmManager::isEnabledNow(const AlarmEntry& entry) const
{
    return entry.enabledSetting ? entry.enabledSetting->get() : entry.enabledDefault;
}

bool AlarmManager::isMinActiveNow(const AlarmEntry& entry) const
{
    return entry.minActiveSetting ? entry.minActiveSetting->get() : entry.minActiveDefault;
}

bool AlarmManager::isMaxActiveNow(const AlarmEntry& entry) const
{
    return entry.maxActiveSetting ? entry.maxActiveSetting->get() : entry.maxActiveDefault;
}

float AlarmManager::thresholdMinNow(const AlarmEntry& entry) const
{
    return entry.thresholdMinSetting ? entry.thresholdMinSetting->get() : entry.thresholdMin;
}

float AlarmManager::thresholdMaxNow(const AlarmEntry& entry) const
{
    return entry.thresholdMaxSetting ? entry.thresholdMaxSetting->get() : entry.thresholdMax;
}

bool AlarmManager::readDigitalSource(const AlarmEntry& entry, bool& value) const
{
    if (entry.digitalGetter) {
        value = entry.digitalGetter();
        return true;
    }
    if (entry.digitalSource) {
        value = *entry.digitalSource;
        return true;
    }
    return false;
}

bool AlarmManager::readAnalogSource(const AlarmEntry& entry, float& value) const
{
    if (entry.analogGetter) {
        value = entry.analogGetter();
        return true;
    }
    if (entry.analogSource) {
        value = *entry.analogSource;
        return true;
    }
    return false;
}

void AlarmManager::update()
{
    const uint32_t nowMsRaw = millis();
    if (updateIntervalMs > 0 && lastUpdateMs != 0) {
        if (static_cast<uint32_t>(nowMsRaw - lastUpdateMs) < updateIntervalMs) {
            return;
        }
    }
    lastUpdateMs = nowMsRaw;
    const auto now = std::chrono::milliseconds(nowMsRaw);
    for (auto& entry : alarms) {
        bool nextActive = false;
        AlarmState nextState = AlarmState::Ok;
        bool enabled = isEnabledNow(entry);
        if (!enabled) {
            nextActive = false;
            nextState = AlarmState::Ok;
        } else if (isDigitalKind(entry.kind)) {
            bool value = false;
            if (!readDigitalSource(entry, value)) {
                continue;
            }
            const bool activeWhen = (entry.kind == AlarmKind::DigitalActive);
            nextActive = (value == activeWhen);
            nextState = nextActive ? AlarmState::Active : AlarmState::Ok;
        } else if (isAnalogKind(entry.kind)) {
            float value = 0.0f;
            if (!readAnalogSource(entry, value)) {
                continue;
            }
            const bool minActive = isMinActiveNow(entry);
            const bool maxActive = isMaxActiveNow(entry);
            const float minVal = thresholdMinNow(entry);
            const float maxVal = thresholdMaxNow(entry);

            if (entry.kind == AlarmKind::AnalogBelow) {
                if (minActive && isFiniteFloat(minVal) && value < minVal) {
                    nextActive = true;
                    nextState = AlarmState::Below;
                }
            } else if (entry.kind == AlarmKind::AnalogAbove) {
                if (maxActive && isFiniteFloat(maxVal) && value > maxVal) {
                    nextActive = true;
                    nextState = AlarmState::Above;
                }
            } else {
                if (minActive && isFiniteFloat(minVal) && value < minVal) {
                    nextActive = true;
                    nextState = AlarmState::Below;
                } else if (maxActive && isFiniteFloat(maxVal) && value > maxVal) {
                    nextActive = true;
                    nextState = AlarmState::Above;
                }
            }
        }

        const bool wasActive = entry.active;
        const AlarmState previousState = entry.state;

        if (nextActive != wasActive) {
            entry.active = nextActive;
            if (entry.onStateChanged) {
                entry.onStateChanged(nextActive);
            }
            if (nextActive && entry.onEnter) {
                entry.onEnter();
            }
            if (!nextActive && entry.onExit) {
                entry.onExit();
            }
            if (nextActive) {
                entry.lastStay = now;
            }
        }

        if (nextState != previousState) {
            entry.state = nextState;
            if (entry.onStateCodeChanged) {
                entry.onStateCodeChanged(nextState);
            }
        }

        if (entry.boundActive) {
            *entry.boundActive = entry.active;
        }
        if (entry.boundState) {
            *entry.boundState = entry.state;
        }

        if (entry.active && entry.onStay && entry.stayInterval.count() > 0) {
            if (now - entry.lastStay >= entry.stayInterval) {
                entry.lastStay = now;
                entry.onStay();
            }
        }
    }
}

void AlarmManager::setUpdateInterval(uint32_t intervalMs)
{
    updateIntervalMs = intervalMs;
    lastUpdateMs = 0;
}

} // namespace cm
