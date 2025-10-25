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

    // Callback function types
    using ConnectedCallback = std::function<void()>;
    using DisconnectedCallback = std::function<void()>;
    using MessageCallback = std::function<void(char* topic, byte* payload, unsigned int length)>;

    MQTTManager();
    ~MQTTManager();

    // Configuration
    void setServer(const char* server, uint16_t port = 1883);
    void setCredentials(const char* username, const char* password);
    void setClientId(const char* clientId);
    void setKeepAlive(uint16_t keepAlive = 60);
    void setMaxRetries(uint8_t maxRetries = 10);
    void setRetryInterval(unsigned long retryInterval = 5000);

    // Callbacks
    void onConnected(ConnectedCallback callback);
    void onDisconnected(DisconnectedCallback callback);
    void onMessage(MessageCallback callback);

    // Connection management
    bool begin();
    void loop();
    void disconnect();

    // Status
    bool isConnected() const;
    ConnectionState getState() const;
    uint8_t getCurrentRetry() const;
    unsigned long getLastConnectionAttempt() const;

    // Publishing
    bool publish(const char* topic, const char* payload, bool retained = false);
    bool publish(const char* topic, const String& payload, bool retained = false);

    // Subscribing
    bool subscribe(const char* topic, uint8_t qos = 0);
    bool unsubscribe(const char* topic);

    // Statistics
    unsigned long getUptime() const;
    uint32_t getReconnectCount() const;

private:
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;

    // Configuration
    String _server;
    uint16_t _port;
    String _username;
    String _password;
    String _clientId;
    uint16_t _keepAlive;
    uint8_t _maxRetries;
    unsigned long _retryInterval;

    // State management
    ConnectionState _state;
    uint8_t _currentRetry;
    unsigned long _lastConnectionAttempt;
    unsigned long _connectionStartTime;
    uint32_t _reconnectCount;

    // Callbacks
    ConnectedCallback _onConnected;
    DisconnectedCallback _onDisconnected;
    MessageCallback _onMessage;

    // Internal methods
    void _attemptConnection();
    void _handleConnection();
    void _handleDisconnection();
    void _setState(ConnectionState newState);
    static void _mqttCallback(char* topic, byte* payload, unsigned int length);

    // Static instance for callback
    static MQTTManager* _instance;
};

#endif // MQTT_MANAGER_H