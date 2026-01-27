#pragma once

#include <Arduino.h>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "ConfigManager.h" // Config<> + Runtime + CM_LOG

// Optional module: requires explicit include by the consumer.
// Dependency note: This header requires PubSubClient to be available in the build of the consuming project.
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

namespace cm {

// Optional global hooks (similar to WiFi hooks). Define them in your sketch if needed.
void onMQTTConnected() __attribute__((weak));
void onMQTTDisconnected() __attribute__((weak));
void onMQTTStateChanged(int state) __attribute__((weak));
void onNewMQTTMessage(const char* topic, const char* payload, unsigned int length) __attribute__((weak));

inline void onMQTTConnected() {}
inline void onMQTTDisconnected() {}
inline void onMQTTStateChanged(int) {}
inline void onNewMQTTMessage(const char*, const char*, unsigned int) {}

class MQTTManager {
public:
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Failed,
    };

    struct MqttMessageView {
        const char* topic = nullptr;
        const byte* payload = nullptr;
        unsigned int length = 0;
    };

    using ConnectedCallback = std::function<void()>;
    using DisconnectedCallback = std::function<void()>;
    using MessageCallback = std::function<void(char* topic, byte* payload, unsigned int length)>;
    using NewMessageCallback = std::function<void(const MqttMessageView&)>;
    using StateChangedCallback = std::function<void(ConnectionState)>;

    // Option A: use the singleton and call attach() from setup().
    static MQTTManager& instance()
    {
        static MQTTManager inst;
        return inst;
    }

    struct Settings {
        Config<bool> enableMQTT;
        Config<String> server;
        Config<int> port;
        Config<String> username;
        Config<String> password;
        Config<String> clientId;

        // Topic base (optional; used by helpers in examples)
        Config<String> publishTopicBase;

        // Intervals
        // - Publish interval in seconds. If 0: publish-on-change for send items.
        Config<float> publishIntervalSec;
        // - Listen interval in milliseconds. If 0: process MQTT in every loop.
        Config<int> listenIntervalMs;

        Settings();
    };

    Settings& settings() { return settings_; }
    const Settings& settings() const { return settings_; }

    // Attach to ConfigManager and auto-register baseline settings.
    // This keeps MQTT optional: only projects that include this header and call attach() use it.
    void attach(ConfigManagerClass& configManager);

    // One-liner helper similar to IOManager GUI helpers.
    // Registers the MQTT runtime provider (no GUI fields are auto-added).
    void addToGUI(ConfigManagerClass& configManager,
                  const char* runtimeGroup = "mqtt",
                  int providerOrder = 2,
                  int baseOrder = 10);

    static const char* mqttStateToString(ConnectionState state);
    String getMqttBaseTopic() const;

    bool publishTopic(const char* id, bool retained = false);
    bool publishTopic(ConfigManagerClass& configManager, const char* id, bool retained = false);
    bool publishTopicImmediately(const char* id, bool retained = false);
    bool publishTopicImmediately(ConfigManagerClass& configManager, const char* id, bool retained = false);

    bool publishExtraTopic(const char* id, const char* topic, const String& value, bool retained = false);
    bool publishExtraTopic(ConfigManagerClass& configManager, const char* id, const char* topic, const String& value, bool retained = false);
    bool publishExtraTopicImmediately(const char* id, const char* topic, const String& value, bool retained = false);
    bool publishExtraTopicImmediately(ConfigManagerClass& configManager, const char* id, const char* topic, const String& value, bool retained = false);

    // Individual meta helpers (requested):
    void addLastTopicToGUI(ConfigManagerClass& configManager,
                           const char* runtimeGroup = "mqtt",
                           int order = 20,
                           const char* label = "Last Topic",
                           const char* card = nullptr);

    void addLastPayloadToGUI(ConfigManagerClass& configManager,
                             const char* runtimeGroup = "mqtt",
                             int order = 21,
                             const char* label = "Last Payload",
                             const char* card = nullptr);

    void addLastMessageAgeToGUI(ConfigManagerClass& configManager,
                                const char* runtimeGroup = "mqtt",
                                int order = 22,
                                const char* label = "Last Message Age",
                                const char* unit = "ms",
                                const char* card = nullptr);

    // Explicit GUI opt-in for MQTT receive items.
    void addMQTTTopicTooGUI(ConfigManagerClass& configManager,
                            const char* id,
                            const char* card = nullptr,
                            int order = -1,
                            const char* runtimeGroup = "mqtt");

    // Hooks (names preferred by you). Old onConnected/onDisconnected/onMessage remain available.
    void onMQTTConnect(ConnectedCallback callback) { onMqttConnect_ = std::move(callback); }
    void onMQTTDisconnect(DisconnectedCallback callback) { onMqttDisconnect_ = std::move(callback); }
    void onNewMQTTMessage(NewMessageCallback callback) { onNewMqttMessage_ = std::move(callback); }
    void onMQTTStateChanged(StateChangedCallback callback) { onStateChanged_ = std::move(callback); }

    void onConnected(ConnectedCallback callback) { onConnected_ = std::move(callback); }
    void onDisconnected(DisconnectedCallback callback) { onDisconnected_ = std::move(callback); }
    void onMessage(MessageCallback callback) { onMessage_ = std::move(callback); }

    // Manual overrides are still available (but usually you use settings + attach()).
    void setServer(const char* server, uint16_t port = 1883);
    void setCredentials(const char* username, const char* password);
    void setClientId(const char* clientId);
    void setKeepAlive(uint16_t keepAliveSec = 60);
    void setMaxRetries(uint8_t maxRetries = 10);
    void setRetryInterval(unsigned long retryIntervalMs = 5000);
    void setBufferSize(uint16_t size = 256);

    bool begin();
    void update();
    void loop();
    void disconnect();

    bool isConnected() const;
    ConnectionState getState() const { return state_; }
    uint8_t getCurrentRetry() const { return currentRetry_; }
    unsigned long getLastConnectionAttempt() const { return lastConnectionAttemptMs_; }
    unsigned long getUptime() const;
    uint32_t getReconnectCount() const { return reconnectCount_; }

    const String& getLastTopic() const { return lastTopic_; }
    const String& getLastPayload() const { return lastPayload_; }
    unsigned long getLastMessageAgeMs() const { return lastMessageMs_ > 0 ? (millis() - lastMessageMs_) : 0; }

    struct SystemInfo {
        uint32_t uptimeMs = 0;
        uint32_t freeHeap = 0;
        uint32_t minFreeHeap = 0;
        uint32_t maxAllocHeap = 0;
        uint32_t flashSizeBytes = 0;
        uint32_t cpuFreqMHz = 0;
        String chipModel;
        uint32_t chipRevision = 0;
        String sdkVersion;

        String hostname;
        String ssid;
        int rssi = 0;
        String ip;
        String mac;
    };

    // System info publishing helper:
    // Topics: <publishTopicBase>/System-Info/ESP and /WiFi
    String getSystemInfoTopic() const;
    SystemInfo collectSystemInfo() const;
    bool publishSystemInfo(const SystemInfo& info, bool retained = false);
    bool publishSystemInfoNow(bool retained = false);

    bool publish(const char* topic, const char* payload, bool retained = false);
    bool publish(const char* topic, const String& payload, bool retained = false);
    bool subscribe(const char* topic, uint8_t qos = 0);
    bool unsubscribe(const char* topic);

    // Topic receive helpers: creates Settings entries (topic + optional json key path), stores the parsed value.
    // GUI entries are explicit via addMQTTTopicTooGUI().
    // - If jsonKeyPath is empty or "none": payload is interpreted as plain value.
    // - If payload is JSON and jsonKeyPath is set (example: "E320.Power_in"): value is extracted.
    void addMQTTTopicReceiveFloat(const char* id,
                                 const char* label,
                                 const char* defaultTopic,
                                 float* target,
                                 const char* unit = nullptr,
                                 int precision = 2,
                                 const char* defaultJsonKeyPath = "none",
                                 bool addToSettings = false);

    void addMQTTTopicReceiveInt(const char* id,
                               const char* label,
                               const char* defaultTopic,
                               int* target,
                               const char* unit = nullptr,
                               const char* defaultJsonKeyPath = "none",
                               bool addToSettings = false);

    void addMQTTTopicReceiveBool(const char* id,
                                const char* label,
                                const char* defaultTopic,
                                bool* target,
                                const char* defaultJsonKeyPath = "none",
                                bool addToSettings = false);

    void addMQTTTopicReceiveString(const char* id,
                                  const char* label,
                                  const char* defaultTopic,
                                  String* target,
                                  const char* defaultJsonKeyPath = "none",
                                  bool addToSettings = false);

private:
    MQTTManager();
    ~MQTTManager();

    enum class ValueType {
        Float,
        Int,
        Bool,
        String,
    };

    struct ReceiveItem {
        String id;
        String label;
        ValueType type = ValueType::String;

        std::unique_ptr<char[]> idKeyC;
        std::unique_ptr<char[]> labelC;
        std::unique_ptr<char[]> topicKeyC;
        std::unique_ptr<char[]> topicNameC;
        std::unique_ptr<char[]> jsonKeyKeyC;
        std::unique_ptr<char[]> jsonKeyNameC;

        std::unique_ptr<Config<String>> topic;
        std::unique_ptr<Config<String>> jsonKeyPath;
        String topicValue;
        String jsonKeyPathValue;
        String lastSubscribedTopic;
        bool addToSettings = false;

        bool settingsAdded = false;

        const char* unit = nullptr;
        int precision = 2;

        void* target = nullptr;

        // Runtime ordering inside the MQTT card.
        int runtimeOrder = 0;
    };

    WiFiClient wifiClient_;
    PubSubClient mqttClient_;

    Settings settings_;
    ConfigManagerClass* configManager_ = nullptr;
    bool settingsRegistered_ = false;
    bool runtimeProviderRegistered_ = false;
    bool systemGuiRegistered_ = false;

    // Connection behavior
    uint16_t keepAliveSec_ = 60;
    uint8_t maxRetries_ = 10;
    unsigned long retryIntervalMs_ = 5000;

    ConnectionState state_ = ConnectionState::Disconnected;
    uint8_t currentRetry_ = 0;
    unsigned long lastConnectionAttemptMs_ = 0;
    unsigned long connectionStartMs_ = 0;
    uint32_t reconnectCount_ = 0;

    // Throttling
    unsigned long lastClientLoopMs_ = 0;
    unsigned long lastPublishMs_ = 0;
    unsigned long lastSystemInfoPublishMs_ = 0;

    // Runtime info
    String lastTopic_;
    String lastPayload_;
    unsigned long lastMessageMs_ = 0;

    // Topic registry
    std::vector<ReceiveItem> receiveItems_;
    int nextReceiveSortOrder_ = 200; // after baseline settings
    int nextReceiveRuntimeOrder_ = 200;

    // Callbacks
    ConnectedCallback onConnected_;
    DisconnectedCallback onDisconnected_;
    MessageCallback onMessage_;

    ConnectedCallback onMqttConnect_;
    DisconnectedCallback onMqttDisconnect_;
    NewMessageCallback onNewMqttMessage_;
    StateChangedCallback onStateChanged_;

    void configureFromSettings_();
    void applySettingsCallbacks_();
    void maybePublishSendItems_();
    void maybePublishSystemInfo_();
    void resetPublishSchedule_();
    void maybeClientLoop_();

    void attemptConnection_();
    void handleConnection_();
    void handleDisconnection_();
    void setState_(ConnectionState newState);

    void handleIncomingMessage_(const char* topic, const byte* payload, unsigned int length);
    void handleReceiveItems_(const char* topic, const byte* payload, unsigned int length);
    void updateReceiveSubscription_(ReceiveItem& item, bool force);
    ReceiveItem* findReceiveItemById_(const char* id);
    String getReceiveTopic_(const ReceiveItem& item) const;
    String getReceiveJsonKeyPath_(const ReceiveItem& item) const;
    bool buildReceivePayload_(const ReceiveItem& item, String& outPayload) const;

    static bool isNoneKeyPath_(const String& keyPath);
    static bool isLikelyNumberString_(const String& value);
    static bool tryExtractJsonValueAsString_(const String& payload, const String& keyPath, String& outValue);
    static bool tryParseBool_(const String& value, bool& outBool);
    static String formatUptimeHuman_(uint32_t uptimeMs);

    template <typename TInt>
    static bool tryParseInt_(const String& value, TInt& outInt)
    {
        String s = value;
        s.trim();
        if (!isLikelyNumberString_(s)) {
            return false;
        }
        outInt = static_cast<TInt>(s.toInt());
        return true;
    }

    static bool tryParseFloat_(const String& value, float& outFloat)
    {
        String s = value;
        s.trim();
        if (!isLikelyNumberString_(s)) {
            return false;
        }
        outFloat = s.toFloat();
        return true;
    }

    void ensureReceiveSettingsRegistered_();
    void registerReceiveItemSettings_(ReceiveItem& item);
    void registerReceiveItemRuntimeMeta_(ConfigManagerClass& configManager,
                                        ReceiveItem& item,
                                        const char* runtimeGroup,
                                        int order,
                                        const char* card);

    struct PublishStamp {
        String key;
        unsigned long lastMs = 0;
    };
    std::vector<PublishStamp> publishStamps_;
    PublishStamp* getPublishStamp_(const String& key);
    bool allowPublishNow_(const String& key, bool immediate) const;
    void markPublishedNow_(const String& key);

    static std::unique_ptr<char[]> makeCString_(const String& value)
    {
        const size_t len = value.length();
        std::unique_ptr<char[]> buf(new char[len + 1]);
        memcpy(buf.get(), value.c_str(), len + 1);
        return buf;
    }

    static void mqttCallbackTrampoline_(char* topic, byte* payload, unsigned int length);
    inline static MQTTManager* instanceForCallback_ = nullptr;
};

// ------------------------ Implementation (header-only) ------------------------

inline MQTTManager::Settings::Settings()
    : enableMQTT(ConfigOptions<bool>{
          .key = "MQTTEnable",
          .name = "Enable MQTT",
          .category = "MQTT",
          .defaultValue = false,
          .sortOrder = 1})
    , server(ConfigOptions<String>{
          .key = "MQTTHost",
          .name = "Server",
          .category = "MQTT",
          .defaultValue = String(""),
          .sortOrder = 2})
    , port(ConfigOptions<int>{
          .key = "MQTTPort",
          .name = "Port",
          .category = "MQTT",
          .defaultValue = 1883,
          .sortOrder = 3})
    , username(ConfigOptions<String>{
          .key = "MQTTUser",
          .name = "Username",
          .category = "MQTT",
          .defaultValue = String(""),
          .sortOrder = 4})
    , password(ConfigOptions<String>{
          .key = "MQTTPass",
          .name = "Password",
          .category = "MQTT",
          .defaultValue = String(""),
          .isPassword = true,
          .sortOrder = 5})
    , clientId(ConfigOptions<String>{
          .key = "MQTTClientId",
          .name = "Client ID",
          .category = "MQTT",
          .defaultValue = String(""),
          .sortOrder = 6})
    , publishTopicBase(ConfigOptions<String>{
          .key = "MQTTBaseTopic",
          .name = "Base Topic",
          .category = "MQTT",
          .defaultValue = String("MQTT"),
          .sortOrder = 10})
    , publishIntervalSec(ConfigOptions<float>{
          .key = "MQTTPubPer",
          .name = "Publish Interval (s)",
          .category = "MQTT",
          .defaultValue = 10.0f,
          .sortOrder = 11})
    , listenIntervalMs(ConfigOptions<int>{
          .key = "MQTTListenMs",
          .name = "Listen Interval (ms)",
          .category = "MQTT",
          .defaultValue = 500,
          .sortOrder = 12})
{
}

inline MQTTManager::MQTTManager()
    : mqttClient_(wifiClient_)
{
    if (instanceForCallback_ != nullptr && instanceForCallback_ != this) {
        CM_LOG("[MQTTManager][WARNING] Multiple instances detected; callbacks will target the last created instance");
    }
    instanceForCallback_ = this;
    mqttClient_.setCallback(&MQTTManager::mqttCallbackTrampoline_);
}

inline MQTTManager::~MQTTManager()
{
    disconnect();
    if (instanceForCallback_ == this) {
        instanceForCallback_ = nullptr;
    }
}

inline void MQTTManager::attach(ConfigManagerClass& configManager)
{
    configManager_ = &configManager;
    if (!settingsRegistered_) {
        configManager.addSetting(&settings_.enableMQTT);
        configManager.addSetting(&settings_.server);
        configManager.addSetting(&settings_.port);
        configManager.addSetting(&settings_.username);
        configManager.addSetting(&settings_.password);
        configManager.addSetting(&settings_.clientId);
        configManager.addSetting(&settings_.publishTopicBase);
        configManager.addSetting(&settings_.publishIntervalSec);
        configManager.addSetting(&settings_.listenIntervalMs);
        settingsRegistered_ = true;
    }

    applySettingsCallbacks_();
    configureFromSettings_();
}

inline void MQTTManager::addToGUI(ConfigManagerClass& configManager,
                                  const char* runtimeGroup,
                                  int providerOrder,
                                  int baseOrder)
{
    (void)baseOrder;

    if (!configManager_) {
        configManager_ = &configManager;
    }

    if (!runtimeProviderRegistered_) {
        // Provider contains runtime values only. GUI fields are opt-in.
        configManager.getRuntime().addRuntimeProvider(runtimeGroup, [this](JsonObject& data) {
            data["enabled"] = settings_.enableMQTT.get();
            data["wifi"] = WiFi.isConnected();
            data["connected"] = isConnected();
            data["state"] = static_cast<int>(getState());
            data["reconnects"] = getReconnectCount();
            data["retry"] = getCurrentRetry();
            data["uptimeMs"] = getUptime();
            data["lastTopic"] = lastTopic_;
            data["lastPayload"] = lastPayload_;
            data["lastMsgAgeMs"] = lastMessageMs_ > 0 ? (millis() - lastMessageMs_) : 0;

            for (auto& item : receiveItems_) {
                if (!item.target) {
                    continue;
                }
                switch (item.type) {
                case ValueType::Float:
                    data[item.id] = *static_cast<float*>(item.target);
                    break;
                case ValueType::Int:
                    data[item.id] = *static_cast<int*>(item.target);
                    break;
                case ValueType::Bool:
                    data[item.id] = *static_cast<bool*>(item.target);
                    break;
                case ValueType::String:
                    data[item.id] = *static_cast<String*>(item.target);
                    break;
                }
            }
        }, providerOrder);

        runtimeProviderRegistered_ = true;
    }

    if (!systemGuiRegistered_) {
        configManager.getRuntime().addRuntimeProvider("system", [this](JsonObject& data) {
            data["mqttEnabled"] = settings_.enableMQTT.get();
            data["mqttConnected"] = isConnected();
            data["mqttReconnects"] = getReconnectCount();
        }, 1);

        auto upsertSystemMeta = [&configManager](const String& key,
                                                 const String& label,
                                                 int order,
                                                 bool isBool,
                                                 int precision = 0) {
            RuntimeFieldMeta* existing = configManager.getRuntime().findRuntimeMeta("system", key);
            if (existing) {
                existing->label = label;
                existing->order = order;
                if (isBool) {
                    existing->isBool = true;
                }
                existing->precision = precision;
                return;
            }
            RuntimeFieldMeta meta;
            meta.group = "system";
            meta.key = key;
            meta.label = label;
            meta.order = order;
            meta.isBool = isBool;
            meta.precision = precision;
            configManager.getRuntime().addRuntimeMeta(meta);
        };

        upsertSystemMeta("mqttEnabled", "MQTT Enabled", 4, true);
        upsertSystemMeta("mqttConnected", "MQTT Connected", 5, true);
        upsertSystemMeta("mqttReconnects", "MQTT Reconnect Count", 6, false, 0);

        systemGuiRegistered_ = true;
    }
}

inline const char* MQTTManager::mqttStateToString(ConnectionState state)
{
    switch (state) {
    case ConnectionState::Disconnected:
        return "disconnected";
    case ConnectionState::Connecting:
        return "connecting";
    case ConnectionState::Connected:
        return "connected";
    case ConnectionState::Failed:
        return "failed";
    default:
        return "unknown";
    }
}

inline String MQTTManager::getMqttBaseTopic() const
{
    String base = settings_.publishTopicBase.get();
    base.trim();
    if (base.isEmpty()) {
        base = settings_.clientId.get();
        base.trim();
    }
    while (base.endsWith("/")) {
        base.remove(base.length() - 1);
    }
    return base;
}

inline void MQTTManager::addLastTopicToGUI(ConfigManagerClass& configManager,
                                           const char* runtimeGroup,
                                           int order,
                                           const char* label,
                                           const char* card)
{
    RuntimeFieldMeta* existing = configManager.getRuntime().findRuntimeMeta(runtimeGroup, "lastTopic");
    if (existing) {
        existing->label = label;
        existing->order = order;
        if (card && card[0]) {
            existing->card = card;
        }
        return;
    }

    RuntimeFieldMeta lastTopicMeta;
    lastTopicMeta.group = runtimeGroup;
    lastTopicMeta.key = "lastTopic";
    lastTopicMeta.label = label;
    lastTopicMeta.order = order;
    if (card && card[0]) {
        lastTopicMeta.card = card;
    }
    configManager.getRuntime().addRuntimeMeta(lastTopicMeta);
}

inline void MQTTManager::addLastPayloadToGUI(ConfigManagerClass& configManager,
                                             const char* runtimeGroup,
                                             int order,
                                             const char* label,
                                             const char* card)
{
    RuntimeFieldMeta* existing = configManager.getRuntime().findRuntimeMeta(runtimeGroup, "lastPayload");
    if (existing) {
        existing->label = label;
        existing->order = order;
        existing->isString = true;
        if (card && card[0]) {
            existing->card = card;
        }
        return;
    }

    RuntimeFieldMeta lastPayloadMeta;
    lastPayloadMeta.group = runtimeGroup;
    lastPayloadMeta.key = "lastPayload";
    lastPayloadMeta.label = label;
    lastPayloadMeta.order = order;
    lastPayloadMeta.isString = true;
    if (card && card[0]) {
        lastPayloadMeta.card = card;
    }
    configManager.getRuntime().addRuntimeMeta(lastPayloadMeta);
}

inline void MQTTManager::addLastMessageAgeToGUI(ConfigManagerClass& configManager,
                                                const char* runtimeGroup,
                                                int order,
                                                const char* label,
                                                const char* unit,
                                                const char* card)
{
    RuntimeFieldMeta* existing = configManager.getRuntime().findRuntimeMeta(runtimeGroup, "lastMsgAgeMs");
    if (existing) {
        existing->label = label;
        existing->order = order;
        if (unit && unit[0]) {
            existing->unit = unit;
        }
        existing->precision = 0;
        if (card && card[0]) {
            existing->card = card;
        }
        return;
    }

    RuntimeFieldMeta lastAgeMeta;
    lastAgeMeta.group = runtimeGroup;
    lastAgeMeta.key = "lastMsgAgeMs";
    lastAgeMeta.label = label;
    lastAgeMeta.unit = unit ? unit : "";
    lastAgeMeta.precision = 0;
    lastAgeMeta.order = order;
    if (card && card[0]) {
        lastAgeMeta.card = card;
    }
    configManager.getRuntime().addRuntimeMeta(lastAgeMeta);
}

inline void MQTTManager::addMQTTTopicTooGUI(ConfigManagerClass& configManager,
                                            const char* id,
                                            const char* card,
                                            int order,
                                            const char* runtimeGroup)
{
    if (!runtimeProviderRegistered_) {
        addToGUI(configManager, runtimeGroup);
    }

    if (!id || !id[0]) {
        CM_LOG("[MQTTManager][WARNING] addMQTTTopicTooGUI: id is empty");
        return;
    }

    auto it = std::find_if(receiveItems_.begin(), receiveItems_.end(),
                           [id](const ReceiveItem& item) { return item.id == id; });
    if (it == receiveItems_.end()) {
        CM_LOG("[MQTTManager][WARNING] addMQTTTopicTooGUI: id not found: %s", id);
        return;
    }

    const int resolvedOrder = (order >= 0) ? order : it->runtimeOrder;
    registerReceiveItemRuntimeMeta_(configManager, *it, runtimeGroup, resolvedOrder, card);
}

inline bool MQTTManager::publishTopic(const char* id, bool retained)
{
    if (!id || !id[0]) {
        CM_LOG("[MQTTManager][WARNING] publishTopic: id is empty");
        return false;
    }

    ReceiveItem* item = findReceiveItemById_(id);
    if (!item) {
        CM_LOG("[MQTTManager][WARNING] publishTopic: id not found: %s", id);
        return false;
    }

    const String base = getMqttBaseTopic();
    if (base.isEmpty()) {
        return false;
    }

    String payload;
    if (!buildReceivePayload_(*item, payload)) {
        return false;
    }

    const String key = String("publish:") + id;
    if (!allowPublishNow_(key, false)) {
        return false;
    }

    const String topic = base + "/" + String(id);
    const bool ok = publish(topic.c_str(), payload, retained);
    if (ok) {
        markPublishedNow_(key);
    }
    return ok;
}

inline bool MQTTManager::publishTopic(ConfigManagerClass& configManager, const char* id, bool retained)
{
    configManager_ = &configManager;
    return publishTopic(id, retained);
}

inline bool MQTTManager::publishTopicImmediately(const char* id, bool retained)
{
    if (!id || !id[0]) {
        CM_LOG("[MQTTManager][WARNING] publishTopicImmediately: id is empty");
        return false;
    }

    ReceiveItem* item = findReceiveItemById_(id);
    if (!item) {
        CM_LOG("[MQTTManager][WARNING] publishTopicImmediately: id not found: %s", id);
        return false;
    }

    const String base = getMqttBaseTopic();
    if (base.isEmpty()) {
        return false;
    }

    String payload;
    if (!buildReceivePayload_(*item, payload)) {
        return false;
    }

    const String topic = base + "/" + String(id);
    return publish(topic.c_str(), payload, retained);
}

inline bool MQTTManager::publishTopicImmediately(ConfigManagerClass& configManager, const char* id, bool retained)
{
    configManager_ = &configManager;
    return publishTopicImmediately(id, retained);
}

inline bool MQTTManager::publishExtraTopic(const char* id, const char* topic, const String& value, bool retained)
{
    if (!id || !id[0]) {
        CM_LOG("[MQTTManager][WARNING] publishExtraTopic: id is empty");
        return false;
    }
    if (!topic || !topic[0]) {
        CM_LOG("[MQTTManager][WARNING] publishExtraTopic: topic is empty");
        return false;
    }

    const String key = String("extra:") + id;
    if (!allowPublishNow_(key, false)) {
        return false;
    }

    const bool ok = publish(topic, value, retained);
    if (ok) {
        markPublishedNow_(key);
    }
    return ok;
}

inline bool MQTTManager::publishExtraTopic(ConfigManagerClass& configManager, const char* id, const char* topic, const String& value, bool retained)
{
    configManager_ = &configManager;
    return publishExtraTopic(id, topic, value, retained);
}

inline bool MQTTManager::publishExtraTopicImmediately(const char* id, const char* topic, const String& value, bool retained)
{
    if (!id || !id[0]) {
        CM_LOG("[MQTTManager][WARNING] publishExtraTopicImmediately: id is empty");
        return false;
    }
    if (!topic || !topic[0]) {
        CM_LOG("[MQTTManager][WARNING] publishExtraTopicImmediately: topic is empty");
        return false;
    }
    return publish(topic, value, retained);
}

inline bool MQTTManager::publishExtraTopicImmediately(ConfigManagerClass& configManager, const char* id, const char* topic, const String& value, bool retained)
{
    configManager_ = &configManager;
    return publishExtraTopicImmediately(id, topic, value, retained);
}

inline void MQTTManager::setServer(const char* server, uint16_t port)
{
    settings_.server.set(server ? String(server) : String());
    settings_.port.set(static_cast<int>(port));
    mqttClient_.setServer(settings_.server.get().c_str(), static_cast<uint16_t>(settings_.port.get()));
}

inline void MQTTManager::setCredentials(const char* username, const char* password)
{
    settings_.username.set(username ? String(username) : String());
    settings_.password.set(password ? String(password) : String());
}

inline void MQTTManager::setClientId(const char* clientId)
{
    settings_.clientId.set(clientId ? String(clientId) : String());
}

inline void MQTTManager::setKeepAlive(uint16_t keepAliveSec)
{
    keepAliveSec_ = keepAliveSec;
    mqttClient_.setKeepAlive(keepAliveSec_);
}

inline void MQTTManager::setMaxRetries(uint8_t maxRetries)
{
    maxRetries_ = maxRetries;
}

inline void MQTTManager::setRetryInterval(unsigned long retryIntervalMs)
{
    retryIntervalMs_ = retryIntervalMs;
}

inline void MQTTManager::setBufferSize(uint16_t size)
{
    mqttClient_.setBufferSize(size);
}

inline bool MQTTManager::begin()
{
    if (settings_.server.get().isEmpty()) {
        CM_LOG("[MQTTManager][ERROR] begin: server is not set");
        return false;
    }

    String cid = settings_.clientId.get();
    cid.trim();
    if (cid.isEmpty()) {
        cid = "ESP32_" + String(WiFi.macAddress());
        cid.replace(":", "");
        settings_.clientId.set(cid);
    }

    mqttClient_.setServer(settings_.server.get().c_str(), static_cast<uint16_t>(settings_.port.get()));
    setState_(ConnectionState::Disconnected);
    currentRetry_ = 0;
    lastConnectionAttemptMs_ = 0;
    return true;
}

inline void MQTTManager::update() { loop(); }

inline void MQTTManager::loop()
{
    if (!settings_.enableMQTT.get()) {
        if (state_ != ConnectionState::Disconnected) {
            disconnect();
        }
        return;
    }

    if (!WiFi.isConnected()) {
        if (state_ == ConnectionState::Connected) {
            handleDisconnection_();
        }
        return;
    }

    // Connection state machine
    switch (state_) {
    case ConnectionState::Disconnected:
        if (currentRetry_ < maxRetries_) {
            if (millis() - lastConnectionAttemptMs_ >= retryIntervalMs_) {
                attemptConnection_();
            }
        } else {
            setState_(ConnectionState::Failed);
        }
        break;

    case ConnectionState::Connecting:
        if (millis() - lastConnectionAttemptMs_ >= 5000) {
            currentRetry_++;
            setState_(ConnectionState::Disconnected);
        }
        break;

    case ConnectionState::Connected:
        if (!mqttClient_.connected()) {
            handleDisconnection_();
        } else {
            maybeClientLoop_();
            maybePublishSendItems_();
            maybePublishSystemInfo_();
        }
        break;

    case ConnectionState::Failed:
        if (millis() - lastConnectionAttemptMs_ >= 30000) {
            currentRetry_ = 0;
            setState_(ConnectionState::Disconnected);
        }
        break;
    }
}

inline void MQTTManager::disconnect()
{
    if (mqttClient_.connected()) {
        mqttClient_.disconnect();
    }
    setState_(ConnectionState::Disconnected);
    currentRetry_ = 0;
}

inline bool MQTTManager::isConnected() const
{
    return state_ == ConnectionState::Connected && const_cast<PubSubClient&>(mqttClient_).connected();
}

inline unsigned long MQTTManager::getUptime() const
{
    if (state_ == ConnectionState::Connected && connectionStartMs_ > 0) {
        return millis() - connectionStartMs_;
    }
    return 0;
}

inline bool MQTTManager::publish(const char* topic, const char* payload, bool retained)
{
    if (!isConnected()) {
        return false;
    }
    if (topic && topic[0]) {
        CM_LOG_VERBOSE("[MQTT][TX] %s", topic);
    }
    return mqttClient_.publish(topic, payload, retained);
}

inline bool MQTTManager::publish(const char* topic, const String& payload, bool retained)
{
    return publish(topic, payload.c_str(), retained);
}

inline String MQTTManager::getSystemInfoTopic() const
{
    const String base = getMqttBaseTopic();
    if (base.isEmpty()) {
        return String();
    }
    return base + "/System-Info";
}

inline MQTTManager::SystemInfo MQTTManager::collectSystemInfo() const
{
    SystemInfo info;
    info.uptimeMs = millis();
    info.freeHeap = static_cast<uint32_t>(ESP.getFreeHeap());
#if defined(ESP32)
    info.minFreeHeap = static_cast<uint32_t>(ESP.getMinFreeHeap());
    info.maxAllocHeap = static_cast<uint32_t>(ESP.getMaxAllocHeap());
#endif
    info.flashSizeBytes = static_cast<uint32_t>(ESP.getFlashChipSize());
    info.cpuFreqMHz = static_cast<uint32_t>(ESP.getCpuFreqMHz());
    info.chipModel = String(ESP.getChipModel());
    info.chipRevision = static_cast<uint32_t>(ESP.getChipRevision());
    info.sdkVersion = String(ESP.getSdkVersion());

    const char* hn = WiFi.getHostname();
    info.hostname = hn ? String(hn) : String();
    info.ssid = WiFi.SSID();
    info.rssi = WiFi.RSSI();
    info.ip = WiFi.isConnected() ? WiFi.localIP().toString() : String();
    info.mac = WiFi.macAddress();

    return info;
}

inline bool MQTTManager::publishSystemInfo(const SystemInfo& info, bool retained)
{
    const String baseTopic = getSystemInfoTopic();
    if (baseTopic.isEmpty()) {
        return false;
    }

    const String uptimeHuman = formatUptimeHuman_(info.uptimeMs);

    if (retained) {
        publish(baseTopic.c_str(), "", true);
    }

    auto publishPayload = [this, retained](const String& topic, const String& payload) -> bool {
        // PubSubClient has a fixed internal buffer (default ~256 bytes). System-Info JSON can be bigger.
        // Grow the buffer on demand to avoid publish failures.
        const size_t required = payload.length() + 1; // include null terminator
        if (required > 0) {
            const size_t desired = required + 64; // headroom for topic + overhead
            const size_t capped = desired > 2048 ? 2048 : desired;
            mqttClient_.setBufferSize(static_cast<uint16_t>(capped));
        }
        return publish(topic.c_str(), payload, retained);
    };

    StaticJsonDocument<512> espDoc;
    espDoc["uptimeMs"] = info.uptimeMs;
    espDoc["uptimeHuman"] = uptimeHuman;
    espDoc["freeHeap"] = info.freeHeap;
    if (info.minFreeHeap > 0) espDoc["minFreeHeap"] = info.minFreeHeap;
    if (info.maxAllocHeap > 0) espDoc["maxAllocHeap"] = info.maxAllocHeap;
    espDoc["flashSizeBytes"] = info.flashSizeBytes;
    espDoc["cpuFreqMHz"] = info.cpuFreqMHz;
    espDoc["chipModel"] = info.chipModel;
    espDoc["chipRevision"] = info.chipRevision;
    espDoc["sdkVersion"] = info.sdkVersion;

    String espPayload;
    serializeJson(espDoc, espPayload);
    const String espTopic = baseTopic + "/ESP";
    const bool okEsp = publishPayload(espTopic, espPayload);

    StaticJsonDocument<512> wifiDoc;
    wifiDoc["uptimeMs"] = info.uptimeMs;
    wifiDoc["uptimeHuman"] = uptimeHuman;
    wifiDoc["hostname"] = info.hostname;
    wifiDoc["ssid"] = info.ssid;
    wifiDoc["rssi"] = info.rssi;
    wifiDoc["ip"] = info.ip;
    wifiDoc["mac"] = info.mac;
    wifiDoc["connected"] = WiFi.isConnected();

    String wifiPayload;
    serializeJson(wifiDoc, wifiPayload);
    const String wifiTopic = baseTopic + "/WiFi";
    const bool okWiFi = publishPayload(wifiTopic, wifiPayload);

    return okEsp && okWiFi;
}

inline bool MQTTManager::publishSystemInfoNow(bool retained)
{
    return publishSystemInfo(collectSystemInfo(), retained);
}

inline bool MQTTManager::subscribe(const char* topic, uint8_t qos)
{
    if (!isConnected()) {
        return false;
    }
    return mqttClient_.subscribe(topic, qos);
}

inline bool MQTTManager::unsubscribe(const char* topic)
{
    if (!isConnected()) {
        return false;
    }
    return mqttClient_.unsubscribe(topic);
}

inline void MQTTManager::addMQTTTopicReceiveFloat(const char* id,
                                                  const char* label,
                                                  const char* defaultTopic,
                                                  float* target,
                                                  const char* unit,
                                                  int precision,
                                                  const char* defaultJsonKeyPath,
                                                  bool addToSettings)
{
    ReceiveItem item;
    item.id = id ? String(id) : String();
    item.label = label ? String(label) : item.id;
    item.type = ValueType::Float;
    item.unit = unit;
    item.precision = precision;
    item.target = target;
    item.runtimeOrder = nextReceiveRuntimeOrder_++;
    item.addToSettings = addToSettings;

    item.idKeyC = makeCString_(item.id);
    item.labelC = makeCString_(item.label);
    item.topicKeyC = makeCString_(String("MQTTRxT_") + item.id);
    item.topicNameC = makeCString_(item.label + String(" Topic"));
    item.jsonKeyKeyC = makeCString_(String("MQTTRxK_") + item.id);
    item.jsonKeyNameC = makeCString_(item.label + String(" JSON Key"));

    if (addToSettings) {
        item.topic = std::make_unique<Config<String>>(ConfigOptions<String>{
            .key = item.topicKeyC.get(),
            .name = item.topicNameC.get(),
            .category = "MQTT-Topics",
            .defaultValue = String(defaultTopic ? defaultTopic : ""),
            .sortOrder = nextReceiveSortOrder_++,
            .categoryPretty = "MQTT Topics"});

        item.jsonKeyPath = std::make_unique<Config<String>>(ConfigOptions<String>{
            .key = item.jsonKeyKeyC.get(),
            .name = item.jsonKeyNameC.get(),
            .category = "MQTT-Topics",
            .defaultValue = String(defaultJsonKeyPath ? defaultJsonKeyPath : "none"),
            .sortOrder = nextReceiveSortOrder_++,
            .categoryPretty = "MQTT Topics"});
    } else {
        item.topicValue = String(defaultTopic ? defaultTopic : "");
        item.jsonKeyPathValue = String(defaultJsonKeyPath ? defaultJsonKeyPath : "none");
    }

    receiveItems_.push_back(std::move(item));
    ensureReceiveSettingsRegistered_();
}

inline void MQTTManager::addMQTTTopicReceiveInt(const char* id,
                                                const char* label,
                                                const char* defaultTopic,
                                                int* target,
                                                const char* unit,
                                                const char* defaultJsonKeyPath,
                                                bool addToSettings)
{
    ReceiveItem item;
    item.id = id ? String(id) : String();
    item.label = label ? String(label) : item.id;
    item.type = ValueType::Int;
    item.unit = unit;
    item.precision = 0;
    item.target = target;
    item.runtimeOrder = nextReceiveRuntimeOrder_++;
    item.addToSettings = addToSettings;

    item.idKeyC = makeCString_(item.id);
    item.labelC = makeCString_(item.label);
    item.topicKeyC = makeCString_(String("MQTTRxT_") + item.id);
    item.topicNameC = makeCString_(item.label + String(" Topic"));
    item.jsonKeyKeyC = makeCString_(String("MQTTRxK_") + item.id);
    item.jsonKeyNameC = makeCString_(item.label + String(" JSON Key"));

    if (addToSettings) {
        item.topic = std::make_unique<Config<String>>(ConfigOptions<String>{
            .key = item.topicKeyC.get(),
            .name = item.topicNameC.get(),
            .category = "MQTT-Topics",
            .defaultValue = String(defaultTopic ? defaultTopic : ""),
            .sortOrder = nextReceiveSortOrder_++,
            .categoryPretty = "MQTT Topics"});

        item.jsonKeyPath = std::make_unique<Config<String>>(ConfigOptions<String>{
            .key = item.jsonKeyKeyC.get(),
            .name = item.jsonKeyNameC.get(),
            .category = "MQTT-Topics",
            .defaultValue = String(defaultJsonKeyPath ? defaultJsonKeyPath : "none"),
            .sortOrder = nextReceiveSortOrder_++,
            .categoryPretty = "MQTT Topics"});
    } else {
        item.topicValue = String(defaultTopic ? defaultTopic : "");
        item.jsonKeyPathValue = String(defaultJsonKeyPath ? defaultJsonKeyPath : "none");
    }

    receiveItems_.push_back(std::move(item));
    ensureReceiveSettingsRegistered_();
}

inline void MQTTManager::addMQTTTopicReceiveBool(const char* id,
                                                 const char* label,
                                                 const char* defaultTopic,
                                                 bool* target,
                                                 const char* defaultJsonKeyPath,
                                                 bool addToSettings)
{
    ReceiveItem item;
    item.id = id ? String(id) : String();
    item.label = label ? String(label) : item.id;
    item.type = ValueType::Bool;
    item.target = target;
    item.runtimeOrder = nextReceiveRuntimeOrder_++;
    item.addToSettings = addToSettings;

    item.idKeyC = makeCString_(item.id);
    item.labelC = makeCString_(item.label);
    item.topicKeyC = makeCString_(String("MQTTRxT_") + item.id);
    item.topicNameC = makeCString_(item.label + String(" Topic"));
    item.jsonKeyKeyC = makeCString_(String("MQTTRxK_") + item.id);
    item.jsonKeyNameC = makeCString_(item.label + String(" JSON Key"));

    if (addToSettings) {
        item.topic = std::make_unique<Config<String>>(ConfigOptions<String>{
            .key = item.topicKeyC.get(),
            .name = item.topicNameC.get(),
            .category = "MQTT-Topics",
            .defaultValue = String(defaultTopic ? defaultTopic : ""),
            .sortOrder = nextReceiveSortOrder_++,
            .categoryPretty = "MQTT Topics"});

        item.jsonKeyPath = std::make_unique<Config<String>>(ConfigOptions<String>{
            .key = item.jsonKeyKeyC.get(),
            .name = item.jsonKeyNameC.get(),
            .category = "MQTT-Topics",
            .defaultValue = String(defaultJsonKeyPath ? defaultJsonKeyPath : "none"),
            .sortOrder = nextReceiveSortOrder_++,
            .categoryPretty = "MQTT Topics"});
    } else {
        item.topicValue = String(defaultTopic ? defaultTopic : "");
        item.jsonKeyPathValue = String(defaultJsonKeyPath ? defaultJsonKeyPath : "none");
    }

    receiveItems_.push_back(std::move(item));
    ensureReceiveSettingsRegistered_();
}

inline void MQTTManager::addMQTTTopicReceiveString(const char* id,
                                                   const char* label,
                                                   const char* defaultTopic,
                                                   String* target,
                                                   const char* defaultJsonKeyPath,
                                                   bool addToSettings)
{
    ReceiveItem item;
    item.id = id ? String(id) : String();
    item.label = label ? String(label) : item.id;
    item.type = ValueType::String;
    item.target = target;
    item.runtimeOrder = nextReceiveRuntimeOrder_++;
    item.addToSettings = addToSettings;

    item.idKeyC = makeCString_(item.id);
    item.labelC = makeCString_(item.label);
    item.topicKeyC = makeCString_(String("MQTTRxT_") + item.id);
    item.topicNameC = makeCString_(item.label + String(" Topic"));
    item.jsonKeyKeyC = makeCString_(String("MQTTRxK_") + item.id);
    item.jsonKeyNameC = makeCString_(item.label + String(" JSON Key"));

    if (addToSettings) {
        item.topic = std::make_unique<Config<String>>(ConfigOptions<String>{
            .key = item.topicKeyC.get(),
            .name = item.topicNameC.get(),
            .category = "MQTT-Topics",
            .defaultValue = String(defaultTopic ? defaultTopic : ""),
            .sortOrder = nextReceiveSortOrder_++,
            .categoryPretty = "MQTT Topics"});

        item.jsonKeyPath = std::make_unique<Config<String>>(ConfigOptions<String>{
            .key = item.jsonKeyKeyC.get(),
            .name = item.jsonKeyNameC.get(),
            .category = "MQTT-Topics",
            .defaultValue = String(defaultJsonKeyPath ? defaultJsonKeyPath : "none"),
            .sortOrder = nextReceiveSortOrder_++,
            .categoryPretty = "MQTT Topics"});
    } else {
        item.topicValue = String(defaultTopic ? defaultTopic : "");
        item.jsonKeyPathValue = String(defaultJsonKeyPath ? defaultJsonKeyPath : "none");
    }

    receiveItems_.push_back(std::move(item));
    ensureReceiveSettingsRegistered_();
}

inline void MQTTManager::configureFromSettings_()
{
    mqttClient_.setServer(settings_.server.get().c_str(), static_cast<uint16_t>(settings_.port.get()));

    if (!settings_.enableMQTT.get()) {
        return;
    }

    if (settings_.server.get().isEmpty()) {
        disconnect();
        return;
    }

    // Ensure internal connect state is initialized.
    begin();
}

inline void MQTTManager::applySettingsCallbacks_()
{
    // Keep this idempotent: callbacks may be assigned multiple times.
    settings_.enableMQTT.setCallback([this](bool) {
        if (!settings_.enableMQTT.get()) {
            disconnect();
        }
    });

    auto reconfigure = [this](auto) { this->configureFromSettings_(); };
    settings_.server.setCallback(reconfigure);
    settings_.port.setCallback([this](int) { this->configureFromSettings_(); });
    settings_.username.setCallback(reconfigure);
    settings_.password.setCallback(reconfigure);
    settings_.clientId.setCallback([this](const String&) {
        this->configureFromSettings_();
        resetPublishSchedule_();
        lastSystemInfoPublishMs_ = 0;
        if (isConnected()) {
            publishSystemInfoNow(true);
        }
    });
    settings_.publishTopicBase.setCallback([this](const String&) {
        resetPublishSchedule_();
        lastSystemInfoPublishMs_ = 0;
        if (isConnected()) {
            publishSystemInfoNow(true);
        }
    });
    settings_.publishIntervalSec.setCallback([this](float) {
        resetPublishSchedule_();
    });
    settings_.listenIntervalMs.setCallback([this](int) {
        lastClientLoopMs_ = 0;
    });
}

inline void MQTTManager::maybeClientLoop_()
{
    const int listenMs = settings_.listenIntervalMs.get();
    if (listenMs <= 0) {
        mqttClient_.loop();
        return;
    }

    const unsigned long now = millis();
    if (now - lastClientLoopMs_ >= static_cast<unsigned long>(listenMs)) {
        lastClientLoopMs_ = now;
        mqttClient_.loop();
    }
}

inline void MQTTManager::maybePublishSendItems_()
{
    const float pubSec = settings_.publishIntervalSec.get();
    if (pubSec <= 0.0f) {
        // publish-on-change is implemented in send helpers (future); no periodic publish.
        return;
    }

    const unsigned long now = millis();
    const unsigned long intervalMs = static_cast<unsigned long>(pubSec * 1000.0f);
    if (intervalMs == 0) {
        return;
    }

    if (lastPublishMs_ == 0 || (now - lastPublishMs_ >= intervalMs)) {
        lastPublishMs_ = now;
        // Send helpers will be added next (not needed for current receive tests).
    }
}

inline void MQTTManager::maybePublishSystemInfo_()
{
    if (!isConnected()) {
        return;
    }

    const unsigned long now = millis();
    const unsigned long intervalMs = 60000;
    if (lastSystemInfoPublishMs_ == 0 || (now - lastSystemInfoPublishMs_ >= intervalMs)) {
        lastSystemInfoPublishMs_ = now;
        publishSystemInfoNow(true);
    }
}

inline void MQTTManager::resetPublishSchedule_()
{
    lastPublishMs_ = 0;
    for (auto& stamp : publishStamps_) {
        stamp.lastMs = 0;
    }
}

inline void MQTTManager::attemptConnection_()
{
    setState_(ConnectionState::Connecting);
    lastConnectionAttemptMs_ = millis();

    bool connected = false;
    if (settings_.username.get().isEmpty()) {
        connected = mqttClient_.connect(settings_.clientId.get().c_str());
    } else {
        connected = mqttClient_.connect(settings_.clientId.get().c_str(),
                                        settings_.username.get().c_str(),
                                        settings_.password.get().c_str());
    }

    if (connected) {
        handleConnection_();
    } else {
        currentRetry_++;
        setState_(ConnectionState::Disconnected);
    }
}

inline void MQTTManager::handleConnection_()
{
    setState_(ConnectionState::Connected);
    connectionStartMs_ = millis();
    currentRetry_ = 0;
    reconnectCount_++;
    lastSystemInfoPublishMs_ = 0;

    // Subscribe all receive topics.
    for (auto& item : receiveItems_) {
        const String topic = getReceiveTopic_(item);
        if (topic.length() > 0) {
            mqttClient_.subscribe(topic.c_str());
            item.lastSubscribedTopic = topic;
        } else {
            item.lastSubscribedTopic = String();
        }
    }

    if (onMqttConnect_) {
        onMqttConnect_();
    }
    if (onConnected_) {
        onConnected_();
    }
    onMQTTConnected();
}

inline void MQTTManager::handleDisconnection_()
{
    if (state_ == ConnectionState::Connected) {
        if (onMqttDisconnect_) {
            onMqttDisconnect_();
        }
        if (onDisconnected_) {
            onDisconnected_();
        }
        onMQTTDisconnected();
    }

    setState_(ConnectionState::Disconnected);
    currentRetry_ = 0;
}

inline void MQTTManager::setState_(ConnectionState newState)
{
    if (state_ == newState) {
        return;
    }
    state_ = newState;
    if (onStateChanged_) {
        onStateChanged_(state_);
    }
    ::cm::onMQTTStateChanged(static_cast<int>(state_));
}

inline void MQTTManager::handleIncomingMessage_(const char* topic, const byte* payload, unsigned int length)
{
    if (topic && topic[0]) {
        CM_LOG_VERBOSE("[MQTT][RX] %s", topic);
    }

    lastTopic_ = topic ? String(topic) : String();
    lastPayload_ = String(reinterpret_cast<const char*>(payload), length);
    lastMessageMs_ = millis();

    handleReceiveItems_(topic, payload, length);

    if (onNewMqttMessage_) {
        MqttMessageView view;
        view.topic = topic;
        view.payload = payload;
        view.length = length;
        onNewMqttMessage_(view);
    }
    if (topic && payload && length > 0) {
        ::cm::onNewMQTTMessage(topic, reinterpret_cast<const char*>(payload), length);
    }

    if (onMessage_) {
        // PubSubClient API uses non-const byte*
        onMessage_(const_cast<char*>(topic), const_cast<byte*>(payload), length);
    }
}

inline void MQTTManager::handleReceiveItems_(const char* topic, const byte* payload, unsigned int length)
{
    if (!topic) {
        return;
    }

    const String rawPayload(reinterpret_cast<const char*>(payload), length);

    for (auto& item : receiveItems_) {
        if (!item.target) {
            continue;
        }
        const String configuredTopic = getReceiveTopic_(item);
        if (configuredTopic.isEmpty()) {
            continue;
        }
        if (strcmp(topic, configuredTopic.c_str()) != 0) {
            continue;
        }

        String extracted;
        const String keyPath = getReceiveJsonKeyPath_(item);
        const bool hasJson = rawPayload.startsWith("{");
        bool ok = false;

        if (hasJson) {
            ok = tryExtractJsonValueAsString_(rawPayload, keyPath, extracted);
        } else {
            if (!isNoneKeyPath_(keyPath)) {
                ok = false;
            } else {
                extracted = rawPayload;
                extracted.trim();
                ok = true;
            }
        }

        if (!ok) {
            // Defensive defaults
            switch (item.type) {
            case ValueType::Float:
                *static_cast<float*>(item.target) = 0.0f;
                break;
            case ValueType::Int:
                *static_cast<int*>(item.target) = 0;
                break;
            case ValueType::Bool:
                *static_cast<bool*>(item.target) = false;
                break;
            case ValueType::String:
                *static_cast<String*>(item.target) = String();
                break;
            }
            continue;
        }

        switch (item.type) {
        case ValueType::Float: {
            float f = 0.0f;
            if (tryParseFloat_(extracted, f)) {
                *static_cast<float*>(item.target) = f;
            }
            break;
        }
        case ValueType::Int: {
            int v = 0;
            if (tryParseInt_(extracted, v)) {
                *static_cast<int*>(item.target) = v;
            }
            break;
        }
        case ValueType::Bool: {
            bool b = false;
            if (tryParseBool_(extracted, b)) {
                *static_cast<bool*>(item.target) = b;
            }
            break;
        }
        case ValueType::String:
            *static_cast<String*>(item.target) = extracted;
            break;
        }
    }
}

inline void MQTTManager::updateReceiveSubscription_(ReceiveItem& item, bool force)
{
    const String nextTopic = getReceiveTopic_(item);
    if (!force && nextTopic == item.lastSubscribedTopic) {
        return;
    }

    if (isConnected()) {
        if (item.lastSubscribedTopic.length() > 0 && item.lastSubscribedTopic != nextTopic) {
            mqttClient_.unsubscribe(item.lastSubscribedTopic.c_str());
        }
        if (nextTopic.length() > 0) {
            mqttClient_.subscribe(nextTopic.c_str());
        }
    }

    item.lastSubscribedTopic = nextTopic;
}

inline MQTTManager::ReceiveItem* MQTTManager::findReceiveItemById_(const char* id)
{
    if (!id || !id[0]) {
        return nullptr;
    }
    for (auto& item : receiveItems_) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}

inline String MQTTManager::getReceiveTopic_(const ReceiveItem& item) const
{
    if (item.topic) {
        return item.topic->get();
    }
    return item.topicValue;
}

inline String MQTTManager::getReceiveJsonKeyPath_(const ReceiveItem& item) const
{
    if (item.jsonKeyPath) {
        return item.jsonKeyPath->get();
    }
    return item.jsonKeyPathValue.length() ? item.jsonKeyPathValue : String("none");
}

inline bool MQTTManager::buildReceivePayload_(const ReceiveItem& item, String& outPayload) const
{
    if (!item.target) {
        return false;
    }

    switch (item.type) {
    case ValueType::Float: {
        const float value = *static_cast<float*>(item.target);
        outPayload = String(value, item.precision);
        return true;
    }
    case ValueType::Int: {
        const int value = *static_cast<int*>(item.target);
        outPayload = String(value);
        return true;
    }
    case ValueType::Bool: {
        const bool value = *static_cast<bool*>(item.target);
        outPayload = value ? "true" : "false";
        return true;
    }
    case ValueType::String: {
        const String value = *static_cast<String*>(item.target);
        outPayload = value;
        return true;
    }
    default:
        break;
    }
    return false;
}

inline bool MQTTManager::isNoneKeyPath_(const String& keyPath)
{
    if (keyPath.length() == 0) {
        return true;
    }
    return keyPath.equalsIgnoreCase("none");
}

inline bool MQTTManager::isLikelyNumberString_(const String& value)
{
    if (value.length() == 0) {
        return false;
    }

    bool hasDigit = false;
    for (size_t i = 0; i < value.length(); ++i) {
        const char ch = value.charAt(i);
        if ((ch >= '0' && ch <= '9')) {
            hasDigit = true;
            continue;
        }
        if (ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E') {
            continue;
        }
        return false;
    }
    return hasDigit;
}

inline bool MQTTManager::tryExtractJsonValueAsString_(const String& payload, const String& keyPath, String& outValue)
{
    if (isNoneKeyPath_(keyPath)) {
        return false;
    }

    DynamicJsonDocument doc(2048);
    const DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        return false;
    }

    JsonVariantConst current = doc.as<JsonVariantConst>();
    int start = 0;
    while (start < keyPath.length()) {
        const int dotPos = keyPath.indexOf('.', start);
        const String part = (dotPos < 0) ? keyPath.substring(start) : keyPath.substring(start, dotPos);
        if (part.length() == 0) {
            return false;
        }

        current = current[part.c_str()];
        if (current.isNull()) {
            return false;
        }

        if (dotPos < 0) {
            break;
        }
        start = dotPos + 1;
    }

    if (current.isNull()) {
        return false;
    }

    if (current.is<const char*>()) {
        outValue = String(current.as<const char*>());
        outValue.trim();
        return true;
    }
    if (current.is<int>() || current.is<long>()) {
        outValue = String(current.as<long>());
        return true;
    }
    if (current.is<float>() || current.is<double>()) {
        outValue = String(current.as<double>(), 6);
        outValue.trim();
        return true;
    }
    if (current.is<bool>()) {
        outValue = current.as<bool>() ? "true" : "false";
        return true;
    }

    // Fallback: serialize variant
    outValue = String();
    serializeJson(current, outValue);
    outValue.trim();
    return outValue.length() > 0;
}

inline bool MQTTManager::tryParseBool_(const String& value, bool& outBool)
{
    String s = value;
    s.trim();
    s.toLowerCase();

    if (s == "1" || s == "true" || s == "on" || s == "yes") {
        outBool = true;
        return true;
    }
    if (s == "0" || s == "false" || s == "off" || s == "no") {
        outBool = false;
        return true;
    }
    return false;
}

inline String MQTTManager::formatUptimeHuman_(uint32_t uptimeMs)
{
    unsigned long totalSeconds = uptimeMs / 1000UL;
    unsigned int years = static_cast<unsigned int>(totalSeconds / 31536000UL);
    totalSeconds %= 31536000UL;
    unsigned int months = static_cast<unsigned int>(totalSeconds / 2592000UL);
    totalSeconds %= 2592000UL;
    unsigned int days = static_cast<unsigned int>(totalSeconds / 86400UL);
    totalSeconds %= 86400UL;
    unsigned int hours = static_cast<unsigned int>(totalSeconds / 3600UL);
    totalSeconds %= 3600UL;
    unsigned int minutes = static_cast<unsigned int>(totalSeconds / 60UL);
    unsigned int seconds = static_cast<unsigned int>(totalSeconds % 60UL);

    String out;
    if (years > 0) out += String(years) + "y ";
    if (months > 0 || out.length()) out += String(months) + "mo ";
    if (days > 0 || out.length()) out += String(days) + "d ";
    if (hours > 0 || out.length()) out += String(hours) + "h ";
    if (minutes > 0 || out.length()) out += String(minutes) + "m ";
    out += String(seconds) + "s";
    out.trim();
    return out;
}

inline void MQTTManager::ensureReceiveSettingsRegistered_()
{
    if (!configManager_ || receiveItems_.empty()) {
        return;
    }

    // Register settings for newly added receive items.
    for (auto& item : receiveItems_) {
        registerReceiveItemSettings_(item);
    }
}

inline void MQTTManager::registerReceiveItemSettings_(ReceiveItem& item)
{
    if (!configManager_) {
        return;
    }
    if (!item.addToSettings || !item.topic || !item.jsonKeyPath) {
        return;
    }

    if (item.settingsAdded) {
        return;
    }

    configManager_->addSetting(item.topic.get());
    configManager_->addSetting(item.jsonKeyPath.get());
    const String itemId = item.id;
    item.topic->setCallback([this, itemId](const String&) {
        ReceiveItem* targetItem = findReceiveItemById_(itemId.c_str());
        if (targetItem) {
            updateReceiveSubscription_(*targetItem, true);
        }
    });
    item.settingsAdded = true;
}

inline void MQTTManager::registerReceiveItemRuntimeMeta_(ConfigManagerClass& configManager,
                                                        ReceiveItem& item,
                                                        const char* runtimeGroup,
                                                        int order,
                                                        const char* card)
{
    const char* key = item.idKeyC ? item.idKeyC.get() : item.id.c_str();
    RuntimeFieldMeta* existing = configManager.getRuntime().findRuntimeMeta(runtimeGroup, key);
    if (existing) {
        existing->label = item.labelC ? item.labelC.get() : item.label.c_str();
        existing->order = order;
        existing->unit = item.unit ? item.unit : "";
        existing->precision = item.precision;
        existing->isBool = (item.type == ValueType::Bool);
        existing->isString = (item.type == ValueType::String);
        if (card && card[0]) {
            existing->card = card;
        }
        return;
    }

    RuntimeFieldMeta meta;
    meta.group = runtimeGroup;
    meta.key = key;
    meta.label = item.labelC ? item.labelC.get() : item.label.c_str();
    meta.order = order;
    meta.unit = item.unit ? item.unit : "";
    meta.precision = item.precision;
    if (item.type == ValueType::Bool) {
        meta.isBool = true;
    }
    if (item.type == ValueType::String) {
        meta.isString = true;
    }
    if (card && card[0]) {
        meta.card = card;
    }
    configManager.getRuntime().addRuntimeMeta(meta);
}

inline MQTTManager::PublishStamp* MQTTManager::getPublishStamp_(const String& key)
{
    for (auto& stamp : publishStamps_) {
        if (stamp.key == key) {
            return &stamp;
        }
    }
    publishStamps_.push_back(PublishStamp{key, 0});
    return &publishStamps_.back();
}

inline bool MQTTManager::allowPublishNow_(const String& key, bool immediate) const
{
    if (immediate) {
        return true;
    }
    const float pubSec = settings_.publishIntervalSec.get();
    if (pubSec <= 0.0f) {
        return true;
    }
    const unsigned long intervalMs = static_cast<unsigned long>(pubSec * 1000.0f);
    if (intervalMs == 0) {
        return true;
    }
    const unsigned long now = millis();
    for (const auto& stamp : publishStamps_) {
        if (stamp.key == key) {
            return (stamp.lastMs == 0 || (now - stamp.lastMs >= intervalMs));
        }
    }
    return true;
}

inline void MQTTManager::markPublishedNow_(const String& key)
{
    const unsigned long now = millis();
    PublishStamp* stamp = getPublishStamp_(key);
    if (stamp) {
        stamp->lastMs = now;
    }
}

inline void MQTTManager::mqttCallbackTrampoline_(char* topic, byte* payload, unsigned int length)
{
    if (instanceForCallback_ != nullptr) {
        instanceForCallback_->handleIncomingMessage_(topic, payload, length);
    }
}

} // namespace cm
