#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <functional>

class MQTTManager {
public:
    enum class ConnectionState {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        FAILED
    };

    using ConnectedCallback = std::function<void()>;
    using DisconnectedCallback = std::function<void()>;
    using MessageCallback = std::function<void(char* topic, byte* payload, unsigned int length)>;

    MQTTManager();
    ~MQTTManager();

    void setServer(const char* server, uint16_t port = 1883);
    void setCredentials(const char* username, const char* password);
    void setClientId(const char* clientId);
    void setKeepAlive(uint16_t keepAlive = 60);
    void setMaxRetries(uint8_t maxRetries = 10);
    void setRetryInterval(unsigned long retryInterval = 5000);
    void setBufferSize(uint16_t size = 256);

    void onConnected(ConnectedCallback callback);
    void onDisconnected(DisconnectedCallback callback);
    void onMessage(MessageCallback callback);

    bool begin();
    void loop();
    void disconnect();

    bool isConnected() const;
    ConnectionState getState() const;
    uint8_t getCurrentRetry() const;
    unsigned long getLastConnectionAttempt() const;

    bool publish(const char* topic, const char* payload, bool retained = false);
    bool publish(const char* topic, const String& payload, bool retained = false);

    bool subscribe(const char* topic, uint8_t qos = 0);
    bool unsubscribe(const char* topic);

    unsigned long getUptime() const;
    uint32_t getReconnectCount() const;

    // Optional convenience: parse a power value from JSON payloads using a configurable key path.
    // If keyPath is "none" or empty, only plain numeric payloads are accepted.
    void configurePowerUsage(const String& topic, const String& jsonKeyPath, int* targetWatts);

private:
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;

    String _server;
    uint16_t _port;
    String _username;
    String _password;
    String _clientId;
    uint16_t _keepAlive;
    uint8_t _maxRetries;
    unsigned long _retryInterval;

    ConnectionState _state;
    uint8_t _currentRetry;
    unsigned long _lastConnectionAttempt;
    unsigned long _connectionStartTime;
    uint32_t _reconnectCount;

    ConnectedCallback _onConnected;
    DisconnectedCallback _onDisconnected;
    MessageCallback _onMessage;

    String _powerTopic;
    String _powerJsonKeyPath;
    int* _powerTargetWatts;

    void _attemptConnection();
    void _handleConnection();
    void _handleDisconnection();
    void _setState(ConnectionState newState);
    void _handlePowerUsageMessage(const char* topic, const byte* payload, unsigned int length);

    static bool _isNoneKeyPath(const String& keyPath);
    static bool _isLikelyNumberString(const String& value);
    static bool _tryParseWattsFromJson(const String& payload, const String& keyPath, int& outWatts);
    static bool _tryParseWattsFromPayload(const String& payload, const String& keyPath, int& outWatts);

    static void _mqttCallback(char* topic, byte* payload, unsigned int length);
    static MQTTManager* _instance;
};

#endif // MQTT_MANAGER_H
