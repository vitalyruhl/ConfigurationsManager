#include "mqtt_manager.h"

// Static instance for callback
MQTTManager* MQTTManager::_instance = nullptr;

MQTTManager::MQTTManager()
    : _mqttClient(_wifiClient)
    , _port(1883)
    , _keepAlive(60)
    , _maxRetries(10)
    , _retryInterval(5000)
    , _state(ConnectionState::DISCONNECTED)
    , _currentRetry(0)
    , _lastConnectionAttempt(0)
    , _connectionStartTime(0)
    , _reconnectCount(0)
{
    _instance = this;
    _mqttClient.setCallback(_mqttCallback);
}

MQTTManager::~MQTTManager() {
    disconnect();
    _instance = nullptr;
}

void MQTTManager::setServer(const char* server, uint16_t port) {
    _server = server;
    _port = port;
    _mqttClient.setServer(server, port);
}

void MQTTManager::setCredentials(const char* username, const char* password) {
    _username = username;
    _password = password;
}

void MQTTManager::setClientId(const char* clientId) {
    _clientId = clientId;
}

void MQTTManager::setKeepAlive(uint16_t keepAlive) {
    _keepAlive = keepAlive;
    _mqttClient.setKeepAlive(keepAlive);
}

void MQTTManager::setBufferSize(uint16_t size) {
    _mqttClient.setBufferSize(size);
}

void MQTTManager::setMaxRetries(uint8_t maxRetries) {
    _maxRetries = maxRetries;
}

void MQTTManager::setRetryInterval(unsigned long retryInterval) {
    _retryInterval = retryInterval;
}

void MQTTManager::onConnected(ConnectedCallback callback) {
    _onConnected = callback;
}

void MQTTManager::onDisconnected(DisconnectedCallback callback) {
    _onDisconnected = callback;
}

void MQTTManager::onMessage(MessageCallback callback) {
    _onMessage = callback;
}

bool MQTTManager::begin() {
    if (_server.isEmpty()) {
        return false;
    }

    if (_clientId.isEmpty()) {
        _clientId = "ESP32_" + String(WiFi.macAddress());
        _clientId.replace(":", "");
    }

    _setState(ConnectionState::DISCONNECTED);
    _currentRetry = 0;
    _lastConnectionAttempt = 0;

    return true;
}

void MQTTManager::loop() {
    if (!WiFi.isConnected()) {
        if (_state == ConnectionState::CONNECTED) {
            _handleDisconnection();
        }
        return;
    }

    switch (_state) {
        case ConnectionState::DISCONNECTED:
            if (_currentRetry < _maxRetries) {
                if (millis() - _lastConnectionAttempt >= _retryInterval) {
                    _attemptConnection();
                }
            } else {
                _setState(ConnectionState::FAILED);
            }
            break;

        case ConnectionState::CONNECTING:
            // Check if connection attempt timed out (5 seconds)
            if (millis() - _lastConnectionAttempt >= 5000) {
                _currentRetry++;
                _setState(ConnectionState::DISCONNECTED);
            }
            break;

        case ConnectionState::CONNECTED:
            if (!_mqttClient.connected()) {
                _handleDisconnection();
            } else {
                _mqttClient.loop();
            }
            break;

        case ConnectionState::FAILED:
            // Reset retry counter after a longer delay
            if (millis() - _lastConnectionAttempt >= 30000) {
                _currentRetry = 0;
                _setState(ConnectionState::DISCONNECTED);
            }
            break;
    }
}

void MQTTManager::disconnect() {
    if (_mqttClient.connected()) {
        _mqttClient.disconnect();
    }
    _setState(ConnectionState::DISCONNECTED);
    _currentRetry = 0;
}

bool MQTTManager::isConnected() const {
    return _state == ConnectionState::CONNECTED && const_cast<PubSubClient&>(_mqttClient).connected();
}

MQTTManager::ConnectionState MQTTManager::getState() const {
    return _state;
}

uint8_t MQTTManager::getCurrentRetry() const {
    return _currentRetry;
}

unsigned long MQTTManager::getLastConnectionAttempt() const {
    return _lastConnectionAttempt;
}

bool MQTTManager::publish(const char* topic, const char* payload, bool retained) {
    if (!isConnected()) {
        return false;
    }

    bool success = _mqttClient.publish(topic, payload, retained);
    return success;
}

bool MQTTManager::publish(const char* topic, const String& payload, bool retained) {
    return publish(topic, payload.c_str(), retained);
}

bool MQTTManager::subscribe(const char* topic, uint8_t qos) {
    if (!isConnected()) {
        return false;
    }

    bool success = _mqttClient.subscribe(topic, qos);
    return success;
}

bool MQTTManager::unsubscribe(const char* topic) {
    if (!isConnected()) {
        return false;
    }

    return _mqttClient.unsubscribe(topic);
}

unsigned long MQTTManager::getUptime() const {
    if (_state == ConnectionState::CONNECTED && _connectionStartTime > 0) {
        return millis() - _connectionStartTime;
    }
    return 0;
}

uint32_t MQTTManager::getReconnectCount() const {
    return _reconnectCount;
}

void MQTTManager::_attemptConnection() {
    _setState(ConnectionState::CONNECTING);
    _lastConnectionAttempt = millis();

    bool connected = false;

    if (_username.isEmpty()) {
        connected = _mqttClient.connect(_clientId.c_str());
    } else {
        connected = _mqttClient.connect(_clientId.c_str(), _username.c_str(), _password.c_str());
    }

    if (connected) {
        _handleConnection();
    } else {
        _currentRetry++;
        _setState(ConnectionState::DISCONNECTED);
    }
}

void MQTTManager::_handleConnection() {
    _setState(ConnectionState::CONNECTED);
    _connectionStartTime = millis();
    _currentRetry = 0;
    _reconnectCount++;

    if (_onConnected) {
        _onConnected();
    }
}

void MQTTManager::_handleDisconnection() {
    if (_state == ConnectionState::CONNECTED) {
        if (_onDisconnected) {
            _onDisconnected();
        }
    }

    _setState(ConnectionState::DISCONNECTED);
    _currentRetry = 0;
}

void MQTTManager::_setState(ConnectionState newState) {
    if (_state != newState) {
        _state = newState;
    }
}

void MQTTManager::_mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance && _instance->_onMessage) {
        _instance->_onMessage(topic, payload, length);
    }
}