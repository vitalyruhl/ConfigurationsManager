#pragma once

#include <Arduino.h>
#include <cstdint>
#include <functional>

#include "ConfigManager.h" // for CM_LOG

// Optional module: requires explicit include by the consumer.
// Dependency note: This header requires PubSubClient to be available in the build.
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

    using ConnectedCallback = std::function<void()>;
    using DisconnectedCallback = std::function<void()>;
    using MessageCallback = std::function<void(char* topic, byte* payload, unsigned int length)>;

    MQTTManager()
        : mqttClient_(wifiClient_)
        , port_(1883)
        , keepAliveSec_(60)
        , maxRetries_(10)
        , retryIntervalMs_(5000)
        , state_(ConnectionState::Disconnected)
        , currentRetry_(0)
        , lastConnectionAttemptMs_(0)
        , connectionStartMs_(0)
        , reconnectCount_(0)
        , powerTargetWatts_(nullptr)
    {
        if (instanceForCallback_ != nullptr && instanceForCallback_ != this) {
            CM_LOG("[MQTTManager][WARNING] Multiple instances detected; callbacks will target the last created instance");
        }
        instanceForCallback_ = this;
        mqttClient_.setCallback(&MQTTManager::mqttCallbackTrampoline_);
    }

    ~MQTTManager()
    {
        disconnect();
        if (instanceForCallback_ == this) {
            instanceForCallback_ = nullptr;
        }
    }

    void setServer(const char* server, uint16_t port = 1883)
    {
        server_ = server ? String(server) : String();
        port_ = port;
        mqttClient_.setServer(server_.c_str(), port_);
    }

    void setCredentials(const char* username, const char* password)
    {
        username_ = username ? String(username) : String();
        password_ = password ? String(password) : String();
    }

    void setClientId(const char* clientId)
    {
        clientId_ = clientId ? String(clientId) : String();
    }

    void setKeepAlive(uint16_t keepAliveSec = 60)
    {
        keepAliveSec_ = keepAliveSec;
        mqttClient_.setKeepAlive(keepAliveSec_);
    }

    void setMaxRetries(uint8_t maxRetries = 10)
    {
        maxRetries_ = maxRetries;
    }

    void setRetryInterval(unsigned long retryIntervalMs = 5000)
    {
        retryIntervalMs_ = retryIntervalMs;
    }

    void setBufferSize(uint16_t size = 256)
    {
        mqttClient_.setBufferSize(size);
    }

    void onConnected(ConnectedCallback callback) { onConnected_ = std::move(callback); }
    void onDisconnected(DisconnectedCallback callback) { onDisconnected_ = std::move(callback); }
    void onMessage(MessageCallback callback) { onMessage_ = std::move(callback); }

    // Optional convenience helper: watch a single topic and parse a power value (watts).
    // - If jsonKeyPath is empty or "none", payload must be a plain number string.
    // - If payload is JSON and jsonKeyPath is set (example: "sensor.power"), the value is extracted.
    // - If parsing fails: target is set to 0.
    void configurePowerUsage(const String& topic, const String& jsonKeyPath, int* targetWatts)
    {
        powerTopic_ = topic;
        powerJsonKeyPath_ = jsonKeyPath;
        powerTargetWatts_ = targetWatts;
    }

    bool begin()
    {
        if (server_.isEmpty()) {
            CM_LOG("[MQTTManager][ERROR] begin: server is not set");
            return false;
        }

        if (clientId_.isEmpty()) {
            clientId_ = "ESP32_" + String(WiFi.macAddress());
            clientId_.replace(":", "");
        }

        setState_(ConnectionState::Disconnected);
        currentRetry_ = 0;
        lastConnectionAttemptMs_ = 0;
        return true;
    }

    // Alias (matches other modules style)
    void update() { loop(); }

    void loop()
    {
        if (!WiFi.isConnected()) {
            if (state_ == ConnectionState::Connected) {
                handleDisconnection_();
            }
            return;
        }

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
            // Connection attempt timeout
            if (millis() - lastConnectionAttemptMs_ >= 5000) {
                currentRetry_++;
                setState_(ConnectionState::Disconnected);
            }
            break;

        case ConnectionState::Connected:
            if (!mqttClient_.connected()) {
                handleDisconnection_();
            } else {
                mqttClient_.loop();
            }
            break;

        case ConnectionState::Failed:
            // Reset retry counter after a longer delay
            if (millis() - lastConnectionAttemptMs_ >= 30000) {
                currentRetry_ = 0;
                setState_(ConnectionState::Disconnected);
            }
            break;
        }
    }

    void disconnect()
    {
        if (mqttClient_.connected()) {
            mqttClient_.disconnect();
        }
        setState_(ConnectionState::Disconnected);
        currentRetry_ = 0;
    }

    bool isConnected() const
    {
        return state_ == ConnectionState::Connected && const_cast<PubSubClient&>(mqttClient_).connected();
    }

    ConnectionState getState() const { return state_; }
    uint8_t getCurrentRetry() const { return currentRetry_; }
    unsigned long getLastConnectionAttempt() const { return lastConnectionAttemptMs_; }

    bool publish(const char* topic, const char* payload, bool retained = false)
    {
        if (!isConnected()) {
            return false;
        }
        return mqttClient_.publish(topic, payload, retained);
    }

    bool publish(const char* topic, const String& payload, bool retained = false)
    {
        return publish(topic, payload.c_str(), retained);
    }

    bool subscribe(const char* topic, uint8_t qos = 0)
    {
        if (!isConnected()) {
            return false;
        }
        return mqttClient_.subscribe(topic, qos);
    }

    bool unsubscribe(const char* topic)
    {
        if (!isConnected()) {
            return false;
        }
        return mqttClient_.unsubscribe(topic);
    }

    unsigned long getUptime() const
    {
        if (state_ == ConnectionState::Connected && connectionStartMs_ > 0) {
            return millis() - connectionStartMs_;
        }
        return 0;
    }

    uint32_t getReconnectCount() const { return reconnectCount_; }

private:
    WiFiClient wifiClient_;
    PubSubClient mqttClient_;

    String server_;
    uint16_t port_;
    String username_;
    String password_;
    String clientId_;
    uint16_t keepAliveSec_;
    uint8_t maxRetries_;
    unsigned long retryIntervalMs_;

    ConnectionState state_;
    uint8_t currentRetry_;
    unsigned long lastConnectionAttemptMs_;
    unsigned long connectionStartMs_;
    uint32_t reconnectCount_;

    ConnectedCallback onConnected_;
    DisconnectedCallback onDisconnected_;
    MessageCallback onMessage_;

    String powerTopic_;
    String powerJsonKeyPath_;
    int* powerTargetWatts_;

    void attemptConnection_()
    {
        setState_(ConnectionState::Connecting);
        lastConnectionAttemptMs_ = millis();

        bool connected = false;
        if (username_.isEmpty()) {
            connected = mqttClient_.connect(clientId_.c_str());
        } else {
            connected = mqttClient_.connect(clientId_.c_str(), username_.c_str(), password_.c_str());
        }

        if (connected) {
            handleConnection_();
        } else {
            currentRetry_++;
            setState_(ConnectionState::Disconnected);
        }
    }

    void handleConnection_()
    {
        setState_(ConnectionState::Connected);
        connectionStartMs_ = millis();
        currentRetry_ = 0;
        reconnectCount_++;

        if (onConnected_) {
            onConnected_();
        }
    }

    void handleDisconnection_()
    {
        if (state_ == ConnectionState::Connected) {
            if (onDisconnected_) {
                onDisconnected_();
            }
        }

        setState_(ConnectionState::Disconnected);
        currentRetry_ = 0;
    }

    void setState_(ConnectionState newState)
    {
        if (state_ != newState) {
            state_ = newState;
        }
    }

    static bool isNoneKeyPath_(const String& keyPath)
    {
        if (keyPath.length() == 0) {
            return true;
        }
        return keyPath.equalsIgnoreCase("none");
    }

    static bool isLikelyNumberString_(const String& value)
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

    static bool tryParseWattsFromJson_(const String& payload, const String& keyPath, int& outWatts)
    {
        if (isNoneKeyPath_(keyPath)) {
            return false;
        }

        DynamicJsonDocument doc(1024);
        const DeserializationError err = deserializeJson(doc, payload);
        if (err) {
            return false;
        }

        auto tryReadVariant = [&outWatts](JsonVariantConst v) -> bool {
            if (v.isNull()) {
                return false;
            }

            if (v.is<int>() || v.is<long>()) {
                outWatts = v.as<long>();
                return true;
            }

            if (v.is<float>() || v.is<double>()) {
                outWatts = static_cast<int>(v.as<double>());
                return true;
            }

            if (v.is<const char*>()) {
                String s(v.as<const char*>());
                s.trim();
                if (!isLikelyNumberString_(s)) {
                    return false;
                }
                outWatts = static_cast<int>(s.toFloat());
                return true;
            }

            return false;
        };

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

        return tryReadVariant(current);
    }

    static bool tryParseWattsFromPayload_(const String& payload, const String& keyPath, int& outWatts)
    {
        String s = payload;
        s.trim();

        if (s.equalsIgnoreCase("null") ||
            s.equalsIgnoreCase("undefined") ||
            s.equalsIgnoreCase("NaN") ||
            s.equalsIgnoreCase("Infinity") ||
            s.equalsIgnoreCase("-Infinity")) {
            return false;
        }

        if (s.startsWith("{")) {
            return tryParseWattsFromJson_(s, keyPath, outWatts);
        }

        if (!isNoneKeyPath_(keyPath)) {
            return false;
        }

        if (!isLikelyNumberString_(s)) {
            return false;
        }

        outWatts = static_cast<int>(s.toFloat());
        return true;
    }

    void handlePowerUsageMessage_(const char* topic, const byte* payload, unsigned int length)
    {
        if (powerTargetWatts_ == nullptr) {
            return;
        }
        if (powerTopic_.isEmpty()) {
            return;
        }
        if (strcmp(topic, powerTopic_.c_str()) != 0) {
            return;
        }

        String message(reinterpret_cast<const char*>(payload), length);
        message.trim();

        int watts = 0;
        if (tryParseWattsFromPayload_(message, powerJsonKeyPath_, watts)) {
            *powerTargetWatts_ = watts;
        } else {
            *powerTargetWatts_ = 0;
        }
    }

    static void mqttCallbackTrampoline_(char* topic, byte* payload, unsigned int length)
    {
        if (instanceForCallback_ != nullptr) {
            instanceForCallback_->handlePowerUsageMessage_(topic, payload, length);

            if (instanceForCallback_->onMessage_) {
                instanceForCallback_->onMessage_(topic, payload, length);
            }
        }
    }

    inline static MQTTManager* instanceForCallback_ = nullptr;
};

} // namespace cm
