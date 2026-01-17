#include "mqtt_manager.h"

#include <ArduinoJson.h>

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
    , _powerTargetWatts(nullptr)
{
    _instance = this;
    _mqttClient.setCallback(_mqttCallback);
}

MQTTManager::~MQTTManager() {
    disconnect();
    _instance = nullptr;
}

void MQTTManager::configurePowerUsage(const String& topic, const String& jsonKeyPath, int* targetWatts)
{
    _powerTopic = topic;
    _powerJsonKeyPath = jsonKeyPath;
    _powerTargetWatts = targetWatts;
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

    return _mqttClient.publish(topic, payload, retained);
}

bool MQTTManager::publish(const char* topic, const String& payload, bool retained) {
    return publish(topic, payload.c_str(), retained);
}

bool MQTTManager::subscribe(const char* topic, uint8_t qos) {
    if (!isConnected()) {
        return false;
    }

    return _mqttClient.subscribe(topic, qos);
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

bool MQTTManager::_isNoneKeyPath(const String& keyPath)
{
    if (keyPath.length() == 0)
        return true;
    return keyPath.equalsIgnoreCase("none");
}

bool MQTTManager::_isLikelyNumberString(const String& value)
{
    if (value.length() == 0)
        return false;

    bool hasDigit = false;
    for (size_t i = 0; i < value.length(); ++i)
    {
        const char ch = value.charAt(i);
        if ((ch >= '0' && ch <= '9'))
        {
            hasDigit = true;
            continue;
        }
        if (ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E')
            continue;
        return false;
    }

    return hasDigit;
}

bool MQTTManager::_tryParseWattsFromJson(const String& payload, const String& keyPath, int& outWatts)
{
    if (_isNoneKeyPath(keyPath))
        return false;

    DynamicJsonDocument doc(1024);
    const DeserializationError err = deserializeJson(doc, payload);
    if (err)
        return false;

    auto tryReadVariant = [&outWatts](JsonVariantConst v) -> bool {
        if (v.isNull())
            return false;

        if (v.is<int>() || v.is<long>())
        {
            outWatts = v.as<long>();
            return true;
        }

        if (v.is<float>() || v.is<double>())
        {
            outWatts = static_cast<int>(v.as<double>());
            return true;
        }

        if (v.is<const char*>())
        {
            String s(v.as<const char*>());
            s.trim();
            if (!_isLikelyNumberString(s))
                return false;
            outWatts = static_cast<int>(s.toFloat());
            return true;
        }

        return false;
    };

    JsonVariantConst current = doc.as<JsonVariantConst>();
    int start = 0;
    while (start < keyPath.length())
    {
        const int dotPos = keyPath.indexOf('.', start);
        const String part = (dotPos < 0) ? keyPath.substring(start) : keyPath.substring(start, dotPos);
        if (part.length() == 0)
            return false;

        current = current[part.c_str()];
        if (current.isNull())
            return false;

        if (dotPos < 0)
            break;
        start = dotPos + 1;
    }

    return tryReadVariant(current);
}

bool MQTTManager::_tryParseWattsFromPayload(const String& payload, const String& keyPath, int& outWatts)
{
    String s = payload;
    s.trim();

    if (s.equalsIgnoreCase("null") ||
        s.equalsIgnoreCase("undefined") ||
        s.equalsIgnoreCase("NaN") ||
        s.equalsIgnoreCase("Infinity") ||
        s.equalsIgnoreCase("-Infinity"))
    {
        return false;
    }

    if (s.startsWith("{"))
        return _tryParseWattsFromJson(s, keyPath, outWatts);

    if (!_isNoneKeyPath(keyPath))
        return false;

    if (!_isLikelyNumberString(s))
        return false;

    outWatts = static_cast<int>(s.toFloat());
    return true;
}

void MQTTManager::_handlePowerUsageMessage(const char* topic, const byte* payload, unsigned int length)
{
    if (_powerTargetWatts == nullptr)
        return;
    if (_powerTopic.isEmpty())
        return;
    if (strcmp(topic, _powerTopic.c_str()) != 0)
        return;

    String message(reinterpret_cast<const char*>(payload), length);
    message.trim();

    int watts = 0;
    if (_tryParseWattsFromPayload(message, _powerJsonKeyPath, watts))
    {
        *_powerTargetWatts = watts;
    }
    else
    {
        *_powerTargetWatts = 0;
    }
}

void MQTTManager::_mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        _instance->_handlePowerUsageMessage(topic, payload, length);
    }

    if (_instance && _instance->_onMessage) {
        _instance->_onMessage(topic, payload, length);
    }
}
