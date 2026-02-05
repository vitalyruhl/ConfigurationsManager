#pragma once

#include <Arduino.h>
#include <chrono>
#include <functional>
#include <memory>
#include <vector>

#include "ConfigManager.h"

namespace cm {

enum class AlarmKind {
    DigitalActive,
    DigitalInactive,
    AnalogBelow,
    AnalogAbove,
    AnalogOutsideWindow
};

enum class AlarmSeverity : uint8_t {
    Alarm,
    Warning
};

enum class AlarmState : uint8_t {
    Ok = 0,
    Active = 1,
    Below = 2,
    Above = 3
};

class AlarmManager {
public:
    struct DigitalAlarmConfig {
        const char* id = nullptr;
        const char* name = nullptr;
        AlarmKind kind = AlarmKind::DigitalActive;
        AlarmSeverity severity = AlarmSeverity::Alarm;
        bool enabled = true;
        std::function<bool()> getter;
        const bool* source = nullptr;
    };

    struct AnalogAlarmConfig {
        const char* id = nullptr;
        const char* name = nullptr;
        AlarmKind kind = AlarmKind::AnalogOutsideWindow;
        AlarmSeverity severity = AlarmSeverity::Alarm;
        bool enabled = false;
        bool minActive = true;
        bool maxActive = true;
        float thresholdMin = 0.0f;
        float thresholdMax = 0.0f;
        std::function<float()> getter;
        const float* source = nullptr;
    };

    class AlarmHandle {
    public:
        AlarmHandle() = default;
        AlarmHandle(AlarmManager* owner, size_t index);

        AlarmHandle& onAlarmCome(std::function<void()> cb);
        AlarmHandle& onAlarmGone(std::function<void()> cb);
        AlarmHandle& onAlarmStay(std::function<void()> cb,
                                 std::chrono::milliseconds interval = std::chrono::milliseconds(10000));
        AlarmHandle& onStateChanged(std::function<void(bool)> cb);
        AlarmHandle& onStateCodeChanged(std::function<void(AlarmState)> cb);
        AlarmHandle& bindActive(bool* target);
        AlarmHandle& bindState(AlarmState* target);
        AlarmHandle& setStyle(const RuntimeFieldStyle& style);
        AlarmHandle& addCSSClass(const char* target, const char* cssClass);
        AlarmHandle& setDisplayColors(const char* normalColor, const char* alarmColor, bool blink);

    private:
        AlarmManager* manager = nullptr;
        size_t entryIndex = 0;
    };

    AlarmHandle addDigitalAlarm(const DigitalAlarmConfig& cfg);
    AlarmHandle addAnalogAlarm(const AnalogAlarmConfig& cfg);

    AlarmHandle addDigitalAlarm(const char* id,
                                const char* name,
                                std::function<bool()> getter,
                                AlarmKind kind = AlarmKind::DigitalActive,
                                bool enabled = true,
                                AlarmSeverity severity = AlarmSeverity::Alarm);
    AlarmHandle addDigitalAlarm(const char* id,
                                const char* name,
                                const bool* source,
                                AlarmKind kind = AlarmKind::DigitalActive,
                                bool enabled = true,
                                AlarmSeverity severity = AlarmSeverity::Alarm);

    AlarmHandle addAnalogAlarm(const char* id,
                               const char* name,
                               std::function<float()> getter,
                               AlarmKind kind,
                               float thresholdMin,
                               float thresholdMax,
                               bool minActive,
                               bool maxActive,
                               bool enabled = false,
                               AlarmSeverity severity = AlarmSeverity::Alarm);
    AlarmHandle addAnalogAlarm(const char* id,
                               const char* name,
                               const float* source,
                               AlarmKind kind,
                               float thresholdMin,
                               float thresholdMax,
                               bool minActive,
                               bool maxActive,
                               bool enabled = false,
                               AlarmSeverity severity = AlarmSeverity::Alarm);

    AlarmHandle addDigitalWarning(const DigitalAlarmConfig& cfg);
    AlarmHandle addAnalogWarning(const AnalogAlarmConfig& cfg);

    void addAlarmToLive(const char* alarmId,
                        int order,
                        const char* pageName = nullptr,
                        const char* cardName = nullptr,
                        const char* groupName = nullptr,
                        const char* labelOverride = nullptr);
    void addWarningToLive(const char* alarmId,
                          int order,
                          const char* pageName = nullptr,
                          const char* cardName = nullptr,
                          const char* groupName = nullptr,
                          const char* labelOverride = nullptr);

    void addAlarmToSettingsGroup(const char* alarmId,
                                 const char* pageName,
                                 const char* groupName,
                                 int order);
    void addAlarmToSettingsGroup(const char* alarmId,
                                 const char* pageName,
                                 const char* cardName,
                                 const char* groupName,
                                 int order);

    void setUpdateInterval(uint32_t intervalMs);
    void update();

private:
    struct AlarmPlacement {
        String group;
        String key;
        String label;
        int order = 100;
        AlarmSeverity severity = AlarmSeverity::Alarm;
    };

    struct AlarmEntry {
        String id;
        String name;
        AlarmKind kind = AlarmKind::DigitalActive;
        AlarmSeverity severity = AlarmSeverity::Alarm;
        bool enabledDefault = true;
        bool minActiveDefault = false;
        bool maxActiveDefault = false;
        float thresholdMin = 0.0f;
        float thresholdMax = 0.0f;
        std::function<bool()> digitalGetter;
        std::function<float()> analogGetter;
        const bool* digitalSource = nullptr;
        const float* analogSource = nullptr;

        bool settingsRegistered = false;
        std::shared_ptr<std::string> cardKeyStable;
        std::shared_ptr<std::string> cardPrettyStable;
        std::shared_ptr<std::string> keyEnabledStable;
        std::shared_ptr<std::string> keyMinActiveStable;
        std::shared_ptr<std::string> keyMaxActiveStable;
        std::shared_ptr<std::string> keyThresholdMinStable;
        std::shared_ptr<std::string> keyThresholdMaxStable;

        Config<bool>* enabledSetting = nullptr;
        Config<bool>* minActiveSetting = nullptr;
        Config<bool>* maxActiveSetting = nullptr;
        Config<float>* thresholdMinSetting = nullptr;
        Config<float>* thresholdMaxSetting = nullptr;

        bool active = false;
        AlarmState state = AlarmState::Ok;
        std::function<void()> onEnter;
        std::function<void()> onExit;
        std::function<void()> onStay;
        std::function<void(bool)> onStateChanged;
        std::function<void(AlarmState)> onStateCodeChanged;
        std::chrono::milliseconds stayInterval{0};
        std::chrono::milliseconds lastStay{0};
        bool* boundActive = nullptr;
        AlarmState* boundState = nullptr;

        RuntimeFieldStyle style;
        std::vector<AlarmPlacement> placements;
    };

    uint32_t updateIntervalMs = 1500;
    uint32_t lastUpdateMs = 0;

    struct AlarmLiveGroup {
        String group;
        std::vector<size_t> entries;
    };

    std::vector<AlarmEntry> alarms;
    std::vector<AlarmLiveGroup> liveGroups;

    AlarmEntry* findAlarm(const char* id);
    const AlarmEntry* findAlarm(const char* id) const;
    AlarmEntry* findAlarmByIndex(size_t index);

    static bool isAnalogKind(AlarmKind kind);
    static bool isDigitalKind(AlarmKind kind);

    static RuntimeFieldStyle buildDefaultStyle(AlarmSeverity severity, bool blink);
    static RuntimeFieldStyle buildWarningStyle();
    static RuntimeFieldStyle buildAlarmStyle();

    void ensureLiveGroupProvider(const String& group);
    void updateLiveMetaStyle(const AlarmEntry& entry);
    void registerSettings(AlarmEntry& entry, const char* pageName, const char* cardName, const char* groupName, int order);
    void registerPlacement(AlarmEntry& entry,
                           const char* pageName,
                           const char* cardName,
                           const char* groupName,
                           const char* labelOverride,
                           int order,
                           AlarmSeverity severity);

    bool isEnabledNow(const AlarmEntry& entry) const;
    bool isMinActiveNow(const AlarmEntry& entry) const;
    bool isMaxActiveNow(const AlarmEntry& entry) const;
    float thresholdMinNow(const AlarmEntry& entry) const;
    float thresholdMaxNow(const AlarmEntry& entry) const;
    bool readDigitalSource(const AlarmEntry& entry, bool& value) const;
    bool readAnalogSource(const AlarmEntry& entry, float& value) const;
};

} // namespace cm
