#pragma once

#include <Arduino.h>
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
    // Adds a runtime provider and runtime metadata.
    void addToGUI(ConfigManagerClass& configManager,
                  const char* runtimeGroup = "mqtt",
                  int providerOrder = 2,
                  int baseOrder = 10);

    // Individual meta helpers (requested):
    void addLastTopicToGUI(ConfigManagerClass& configManager,
                           const char* runtimeGroup = "mqtt",
                           int order = 20,
                           const char* label = "Last Topic");

    void addLastPayloadToGUI(ConfigManagerClass& configManager,
                             const char* runtimeGroup = "mqtt",
                             int order = 21,
                             const char* label = "Last Payload");

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
    // Topic: <publishTopicBase>/System-Info
    String getSystemInfoTopic() const;
    SystemInfo collectSystemInfo() const;
    bool publishSystemInfo(const SystemInfo& info, bool retained = false);
    bool publishSystemInfoNow(bool retained = false);

    bool publish(const char* topic, const char* payload, bool retained = false);
    bool publish(const char* topic, const String& payload, bool retained = false);
    bool subscribe(const char* topic, uint8_t qos = 0);
    bool unsubscribe(const char* topic);

    // Topic receive helpers: creates Settings entries (topic + optional json key path), stores the parsed value,
    // and includes the value in addToGUI().
    // - If jsonKeyPath is empty or "none": payload is interpreted as plain value.
    // - If payload is JSON and jsonKeyPath is set (example: "E320.Power_in"): value is extracted.
    void addMQTTTopicReceiveFloat(const char* id,
                                 const char* label,
                                 const char* defaultTopic,
                                 float* target,
                                 const char* unit = nullptr,
                                 int precision = 2,
                                 const char* defaultJsonKeyPath = "none");

    void addMQTTTopicReceiveInt(const char* id,
                               const char* label,
                               const char* defaultTopic,
                               int* target,
                               const char* unit = nullptr,
                               const char* defaultJsonKeyPath = "none");

    void addMQTTTopicReceiveBool(const char* id,
                                const char* label,
                                const char* defaultTopic,
                                bool* target,
                                const char* defaultJsonKeyPath = "none");

    void addMQTTTopicReceiveString(const char* id,
                                  const char* label,
                                  const char* defaultTopic,
                                  String* target,
                                  const char* defaultJsonKeyPath = "none");

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
    bool guiRegistered_ = false;

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
    void maybeClientLoop_();

    void attemptConnection_();
    void handleConnection_();
    void handleDisconnection_();
    void setState_(ConnectionState newState);

    void handleIncomingMessage_(const char* topic, const byte* payload, unsigned int length);
    void handleReceiveItems_(const char* topic, const byte* payload, unsigned int length);

    static bool isNoneKeyPath_(const String& keyPath);
    static bool isLikelyNumberString_(const String& value);
    static bool tryExtractJsonValueAsString_(const String& payload, const String& keyPath, String& outValue);
    static bool tryParseBool_(const String& value, bool& outBool);

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
                                        const char* runtimeGroup,
                                        int baseOrder);

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
          .defaultValue = String(""),
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
    (void)configManager;

    if (!configManager_) {
        configManager_ = &configManager;
    }

    if (!guiRegistered_) {
        // Provider contains baseline + all registered receive values.
        configManager.getRuntime().addRuntimeProvider(runtimeGroup, [this](JsonObject& data) {
            data["enabled"] = settings_.enableMQTT.get();
            data["wifi"] = WiFi.isConnected();
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

        // Baseline meta
        RuntimeFieldMeta enabledMeta;
        enabledMeta.group = runtimeGroup;
        enabledMeta.key = "enabled";
        enabledMeta.label = "MQTT Enabled";
        enabledMeta.order = baseOrder + 0;
        enabledMeta.isBool = true;
        configManager.getRuntime().addRuntimeMeta(enabledMeta);

        RuntimeFieldMeta wifiMeta;
        wifiMeta.group = runtimeGroup;
        wifiMeta.key = "wifi";
        wifiMeta.label = "WiFi Connected";
        wifiMeta.order = baseOrder + 1;
        wifiMeta.isBool = true;
        configManager.getRuntime().addRuntimeMeta(wifiMeta);

        RuntimeFieldMeta stateMeta;
        stateMeta.group = runtimeGroup;
        stateMeta.key = "state";
        stateMeta.label = "State (0=disc,1=conn,2=ok,3=fail)";
        stateMeta.order = baseOrder + 2;
        configManager.getRuntime().addRuntimeMeta(stateMeta);

        RuntimeFieldMeta reconnectsMeta;
        reconnectsMeta.group = runtimeGroup;
        reconnectsMeta.key = "reconnects";
        reconnectsMeta.label = "Reconnect Count";
        reconnectsMeta.order = baseOrder + 3;
        configManager.getRuntime().addRuntimeMeta(reconnectsMeta);

        RuntimeFieldMeta retryMeta;
        retryMeta.group = runtimeGroup;
        retryMeta.key = "retry";
        retryMeta.label = "Current Retry";
        retryMeta.order = baseOrder + 4;
        configManager.getRuntime().addRuntimeMeta(retryMeta);

        RuntimeFieldMeta uptimeMeta;
        uptimeMeta.group = runtimeGroup;
        uptimeMeta.key = "uptimeMs";
        uptimeMeta.label = "MQTT Uptime";
        uptimeMeta.unit = "ms";
        uptimeMeta.order = baseOrder + 5;
        configManager.getRuntime().addRuntimeMeta(uptimeMeta);

        addLastTopicToGUI(configManager, runtimeGroup, baseOrder + 10);
        addLastPayloadToGUI(configManager, runtimeGroup, baseOrder + 11);

        RuntimeFieldMeta lastAgeMeta;
        lastAgeMeta.group = runtimeGroup;
        lastAgeMeta.key = "lastMsgAgeMs";
        lastAgeMeta.label = "Last Message Age";
        lastAgeMeta.unit = "ms";
        lastAgeMeta.order = baseOrder + 12;
        configManager.getRuntime().addRuntimeMeta(lastAgeMeta);

        registerReceiveItemRuntimeMeta_(configManager, runtimeGroup, baseOrder);

        guiRegistered_ = true;
    }
}

inline void MQTTManager::addLastTopicToGUI(ConfigManagerClass& configManager,
                                           const char* runtimeGroup,
                                           int order,
                                           const char* label)
{
    RuntimeFieldMeta lastTopicMeta;
    lastTopicMeta.group = runtimeGroup;
    lastTopicMeta.key = "lastTopic";
    lastTopicMeta.label = label;
    lastTopicMeta.order = order;
    configManager.getRuntime().addRuntimeMeta(lastTopicMeta);
}

inline void MQTTManager::addLastPayloadToGUI(ConfigManagerClass& configManager,
                                             const char* runtimeGroup,
                                             int order,
                                             const char* label)
{
    RuntimeFieldMeta lastPayloadMeta;
    lastPayloadMeta.group = runtimeGroup;
    lastPayloadMeta.key = "lastPayload";
    lastPayloadMeta.label = label;
    lastPayloadMeta.order = order;
    configManager.getRuntime().addRuntimeMeta(lastPayloadMeta);
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
    return mqttClient_.publish(topic, payload, retained);
}

inline bool MQTTManager::publish(const char* topic, const String& payload, bool retained)
{
    return publish(topic, payload.c_str(), retained);
}

inline String MQTTManager::getSystemInfoTopic() const
{
    String base = settings_.publishTopicBase.get();
    base.trim();
    if (base.isEmpty()) {
        base = settings_.clientId.get();
        base.trim();
    }
    if (base.isEmpty()) {
        return String();
    }
    while (base.endsWith("/")) {
        base.remove(base.length() - 1);
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
    const String topic = getSystemInfoTopic();
    if (topic.isEmpty()) {
        return false;
    }

    StaticJsonDocument<768> doc;
    doc["uptimeMs"] = info.uptimeMs;
    doc["freeHeap"] = info.freeHeap;
    if (info.minFreeHeap > 0) doc["minFreeHeap"] = info.minFreeHeap;
    if (info.maxAllocHeap > 0) doc["maxAllocHeap"] = info.maxAllocHeap;

    JsonObject chip = doc.createNestedObject("chip");
    chip["model"] = info.chipModel;
    chip["revision"] = info.chipRevision;
    chip["cpuMHz"] = info.cpuFreqMHz;
    chip["sdk"] = info.sdkVersion;
    chip["flashSize"] = info.flashSizeBytes;

    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["hostname"] = info.hostname;
    wifi["ssid"] = info.ssid;
    wifi["rssi"] = info.rssi;
    wifi["ip"] = info.ip;
    wifi["mac"] = info.mac;

    String payload;
    serializeJson(doc, payload);

    // PubSubClient has a fixed internal buffer (default ~256 bytes). System-Info JSON can be bigger.
    // Grow the buffer on demand to avoid publish failures.
    const size_t required = payload.length() + 1; // include null terminator
    if (required > 0) {
        const size_t desired = required + 64; // headroom for topic + overhead
        const size_t capped = desired > 2048 ? 2048 : desired;
        mqttClient_.setBufferSize(static_cast<uint16_t>(capped));
    }
    return publish(topic.c_str(), payload, retained);
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
                                                  const char* defaultJsonKeyPath)
{
    ReceiveItem item;
    item.id = id ? String(id) : String();
    item.label = label ? String(label) : item.id;
    item.type = ValueType::Float;
    item.unit = unit;
    item.precision = precision;
    item.target = target;
    item.runtimeOrder = nextReceiveRuntimeOrder_++;

    item.idKeyC = makeCString_(item.id);
    item.labelC = makeCString_(item.label);
    item.topicKeyC = makeCString_(String("MQTTRxT_") + item.id);
    item.topicNameC = makeCString_(item.label + String(" Topic"));
    item.jsonKeyKeyC = makeCString_(String("MQTTRxK_") + item.id);
    item.jsonKeyNameC = makeCString_(item.label + String(" JSON Key"));

    item.topic = std::make_unique<Config<String>>(ConfigOptions<String>{
        .key = item.topicKeyC.get(),
        .name = item.topicNameC.get(),
        .category = "MQTT",
        .defaultValue = String(defaultTopic ? defaultTopic : ""),
        .sortOrder = nextReceiveSortOrder_++});

    item.jsonKeyPath = std::make_unique<Config<String>>(ConfigOptions<String>{
        .key = item.jsonKeyKeyC.get(),
        .name = item.jsonKeyNameC.get(),
        .category = "MQTT",
        .defaultValue = String(defaultJsonKeyPath ? defaultJsonKeyPath : "none"),
        .sortOrder = nextReceiveSortOrder_++});

    receiveItems_.push_back(std::move(item));
    ensureReceiveSettingsRegistered_();
}

inline void MQTTManager::addMQTTTopicReceiveInt(const char* id,
                                                const char* label,
                                                const char* defaultTopic,
                                                int* target,
                                                const char* unit,
                                                const char* defaultJsonKeyPath)
{
    ReceiveItem item;
    item.id = id ? String(id) : String();
    item.label = label ? String(label) : item.id;
    item.type = ValueType::Int;
    item.unit = unit;
    item.precision = 0;
    item.target = target;
    item.runtimeOrder = nextReceiveRuntimeOrder_++;

    item.idKeyC = makeCString_(item.id);
    item.labelC = makeCString_(item.label);
    item.topicKeyC = makeCString_(String("MQTTRxT_") + item.id);
    item.topicNameC = makeCString_(item.label + String(" Topic"));
    item.jsonKeyKeyC = makeCString_(String("MQTTRxK_") + item.id);
    item.jsonKeyNameC = makeCString_(item.label + String(" JSON Key"));

    item.topic = std::make_unique<Config<String>>(ConfigOptions<String>{
        .key = item.topicKeyC.get(),
        .name = item.topicNameC.get(),
        .category = "MQTT",
        .defaultValue = String(defaultTopic ? defaultTopic : ""),
        .sortOrder = nextReceiveSortOrder_++});

    item.jsonKeyPath = std::make_unique<Config<String>>(ConfigOptions<String>{
        .key = item.jsonKeyKeyC.get(),
        .name = item.jsonKeyNameC.get(),
        .category = "MQTT",
        .defaultValue = String(defaultJsonKeyPath ? defaultJsonKeyPath : "none"),
        .sortOrder = nextReceiveSortOrder_++});

    receiveItems_.push_back(std::move(item));
    ensureReceiveSettingsRegistered_();
}

inline void MQTTManager::addMQTTTopicReceiveBool(const char* id,
                                                 const char* label,
                                                 const char* defaultTopic,
                                                 bool* target,
                                                 const char* defaultJsonKeyPath)
{
    ReceiveItem item;
    item.id = id ? String(id) : String();
    item.label = label ? String(label) : item.id;
    item.type = ValueType::Bool;
    item.target = target;
    item.runtimeOrder = nextReceiveRuntimeOrder_++;

    item.idKeyC = makeCString_(item.id);
    item.labelC = makeCString_(item.label);
    item.topicKeyC = makeCString_(String("MQTTRxT_") + item.id);
    item.topicNameC = makeCString_(item.label + String(" Topic"));
    item.jsonKeyKeyC = makeCString_(String("MQTTRxK_") + item.id);
    item.jsonKeyNameC = makeCString_(item.label + String(" JSON Key"));

    item.topic = std::make_unique<Config<String>>(ConfigOptions<String>{
        .key = item.topicKeyC.get(),
        .name = item.topicNameC.get(),
        .category = "MQTT",
        .defaultValue = String(defaultTopic ? defaultTopic : ""),
        .sortOrder = nextReceiveSortOrder_++});

    item.jsonKeyPath = std::make_unique<Config<String>>(ConfigOptions<String>{
        .key = item.jsonKeyKeyC.get(),
        .name = item.jsonKeyNameC.get(),
        .category = "MQTT",
        .defaultValue = String(defaultJsonKeyPath ? defaultJsonKeyPath : "none"),
        .sortOrder = nextReceiveSortOrder_++});

    receiveItems_.push_back(std::move(item));
    ensureReceiveSettingsRegistered_();
}

inline void MQTTManager::addMQTTTopicReceiveString(const char* id,
                                                   const char* label,
                                                   const char* defaultTopic,
                                                   String* target,
                                                   const char* defaultJsonKeyPath)
{
    ReceiveItem item;
    item.id = id ? String(id) : String();
    item.label = label ? String(label) : item.id;
    item.type = ValueType::String;
    item.target = target;
    item.runtimeOrder = nextReceiveRuntimeOrder_++;

    item.idKeyC = makeCString_(item.id);
    item.labelC = makeCString_(item.label);
    item.topicKeyC = makeCString_(String("MQTTRxT_") + item.id);
    item.topicNameC = makeCString_(item.label + String(" Topic"));
    item.jsonKeyKeyC = makeCString_(String("MQTTRxK_") + item.id);
    item.jsonKeyNameC = makeCString_(item.label + String(" JSON Key"));

    item.topic = std::make_unique<Config<String>>(ConfigOptions<String>{
        .key = item.topicKeyC.get(),
        .name = item.topicNameC.get(),
        .category = "MQTT",
        .defaultValue = String(defaultTopic ? defaultTopic : ""),
        .sortOrder = nextReceiveSortOrder_++});

    item.jsonKeyPath = std::make_unique<Config<String>>(ConfigOptions<String>{
        .key = item.jsonKeyKeyC.get(),
        .name = item.jsonKeyNameC.get(),
        .category = "MQTT",
        .defaultValue = String(defaultJsonKeyPath ? defaultJsonKeyPath : "none"),
        .sortOrder = nextReceiveSortOrder_++});

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
    settings_.clientId.setCallback(reconfigure);
    settings_.publishIntervalSec.setCallback([this](float) {
        // Reset scheduling on change.
        lastPublishMs_ = 0;
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

    // Subscribe all receive topics.
    for (auto& item : receiveItems_) {
        if (item.topic && item.topic->get().length() > 0) {
            mqttClient_.subscribe(item.topic->get().c_str());
        }
    }

    if (onMqttConnect_) {
        onMqttConnect_();
    }
    if (onConnected_) {
        onConnected_();
    }
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
}

inline void MQTTManager::handleIncomingMessage_(const char* topic, const byte* payload, unsigned int length)
{
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
        if (!item.topic || !item.target) {
            continue;
        }
        const String configuredTopic = item.topic->get();
        if (configuredTopic.isEmpty()) {
            continue;
        }
        if (strcmp(topic, configuredTopic.c_str()) != 0) {
            continue;
        }

        String extracted;
        const String keyPath = item.jsonKeyPath ? item.jsonKeyPath->get() : String("none");
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
    if (!item.topic || !item.jsonKeyPath) {
        return;
    }

    if (item.settingsAdded) {
        return;
    }

    configManager_->addSetting(item.topic.get());
    configManager_->addSetting(item.jsonKeyPath.get());
    item.settingsAdded = true;
}

inline void MQTTManager::registerReceiveItemRuntimeMeta_(ConfigManagerClass& configManager,
                                                        const char* runtimeGroup,
                                                        int baseOrder)
{
    for (auto& item : receiveItems_) {
        RuntimeFieldMeta meta;
        meta.group = runtimeGroup;
        meta.key = item.idKeyC ? item.idKeyC.get() : item.id.c_str();
        meta.label = item.labelC ? item.labelC.get() : item.label.c_str();
        meta.order = baseOrder + item.runtimeOrder;
        meta.unit = item.unit;
        meta.precision = item.precision;
        if (item.type == ValueType::Bool) {
            meta.isBool = true;
        }
        configManager.getRuntime().addRuntimeMeta(meta);
    }
}

inline void MQTTManager::mqttCallbackTrampoline_(char* topic, byte* payload, unsigned int length)
{
    if (instanceForCallback_ != nullptr) {
        instanceForCallback_->handleIncomingMessage_(topic, payload, length);
    }
}

} // namespace cm
