#pragma once
#include <ConfigManager.h>
#include "../settings/SettingsPersistence.h"

namespace telemetry {
class FullRefreshSetting final : public Config<int> {
public:
    explicit FullRefreshSetting(const ConfigOptions<int>& options):Config<int>(options) {}
    bool fromJSON(const JsonVariant& value) override {
        if(!value.is<int>() || value.as<int>()<5 || value.as<int>()>3600)return false;
        set(value.as<int>());return true;
    }
};
// Application-owned persistent keys never collide with CM's transport-only Settings.
// Construct once in place: Config copy/assignment does not transfer key ownership safely.
struct MqttUiSettings {
    Config<bool> enableMQTT;
    Config<String> server;
    Config<int> port;
    Config<String> username;
    Config<String> password;
    Config<String> clientId;
    Config<String> publishTopicBase;
    Config<float> publishIntervalSec;
    Config<int> listenIntervalMs;
    FullRefreshSetting fullRefreshIntervalSec;

    MqttUiSettings(const char* broker, int brokerPort, const char* user,
                   const char* secret, bool enabled, const char* client)
        : enableMQTT(ConfigOptions<bool>{.key="SBTMQTTEnable", .name="Enable MQTT", .category="MQTT", .defaultValue=enabled, .sortOrder=1}),
          server(ConfigOptions<String>{.key="SBTMQTTHost", .name="Server", .category="MQTT", .defaultValue=String(broker), .sortOrder=2}),
          port(ConfigOptions<int>{.key="SBTMQTTPort", .name="Port", .category="MQTT", .defaultValue=brokerPort, .sortOrder=3}),
          username(ConfigOptions<String>{.key="SBTMQTTUser", .name="Username", .category="MQTT", .defaultValue=String(user), .sortOrder=4}),
          password(ConfigOptions<String>{.key="SBTMQTTPass", .name="Password", .category="MQTT", .defaultValue=String(secret), .isPassword=true, .sortOrder=5}),
          clientId(ConfigOptions<String>{.key="SBTMQTTClientId", .name="Client ID", .category="MQTT", .defaultValue=String(client), .sortOrder=6}),
          publishTopicBase(ConfigOptions<String>{.key="SBTMQTTBaseTopic", .name="Base Topic", .category="MQTT", .defaultValue=String("SolarBatteryTelemetry"), .sortOrder=10}),
          publishIntervalSec(ConfigOptions<float>{.key="SBTMQTTPubPer", .name="Publish Interval (s)", .category="MQTT", .defaultValue=5.0f, .sortOrder=11}),
          listenIntervalMs(ConfigOptions<int>{.key="SBTMQTTListenMs", .name="Listen Interval (ms)", .category="MQTT", .defaultValue=0, .sortOrder=12}),
          fullRefreshIntervalSec(ConfigOptions<int>{.key="SBTMQTTRefresh", .name="Full refresh interval (s, 5-3600)", .category="MQTT", .defaultValue=60, .sortOrder=13}) {}

    MqttUiSettings(const MqttUiSettings&) = delete;
    MqttUiSettings& operator=(const MqttUiSettings&) = delete;

    bool migrate(Preferences& prefs) {
        bool ok = true;
        ok = sbt::migrateSettingIfMissing(prefs, enableMQTT, "MQTTEnable") && ok;
        ok = sbt::migrateSettingIfMissing(prefs, server, "MQTTHost") && ok;
        ok = sbt::migrateSettingIfMissing(prefs, port, "MQTTPort") && ok;
        ok = sbt::migrateSettingIfMissing(prefs, username, "MQTTUser") && ok;
        ok = sbt::migrateSettingIfMissing(prefs, password, "MQTTPass") && ok;
        ok = sbt::migrateSettingIfMissing(prefs, clientId, "MQTTClientId") && ok;
        ok = sbt::migrateSettingIfMissing(prefs, publishTopicBase, "MQTTBaseTopic") && ok;
        ok = sbt::migrateSettingIfMissing(prefs, publishIntervalSec, "MQTTPubPer") && ok;
        ok = sbt::migrateSettingIfMissing(prefs, listenIntervalMs, "MQTTListenMs") && ok;
        return ok;
    }

    bool attachTo(ConfigManagerClass& cm) {
        BaseSetting* settings[] = {&enableMQTT, &server, &port, &username, &password,
            &clientId, &publishTopicBase, &publishIntervalSec, &listenIntervalMs, &fullRefreshIntervalSec};
        for (BaseSetting* setting : settings) if (setting->hasError()) return false;
        for (BaseSetting* setting : settings) if (!cm.addSetting(setting)) return false;
        return true;
    }
};
} // namespace telemetry
