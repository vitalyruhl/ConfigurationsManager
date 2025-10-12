# MQTTManager Usage Guide

The `MQTTManager` class provides a robust, non-blocking MQTT client implementation for ESP32 projects. It features automatic reconnection, callback support, and comprehensive error handling.

## Features

- **Non-blocking operations** - Uses state machine for connection management
- **Automatic reconnection** - Configurable retry attempts and intervals
- **Callback system** - Events for connection, disconnection, and messages
- **Robust error handling** - Detailed connection states and timeouts
- **Statistics tracking** - Uptime, reconnect counters, and connection metrics
- **Reusable design** - Can be used across multiple projects
- **Memory efficient** - No global state variables or blocking operations

## Basic Usage

### 1. Include the Header

```cpp
#include "helpers/mqtt_manager.h"
```

### 2. Create Instance

```cpp
MQTTManager mqttManager;  // Global instance
```

### 3. Configure Settings

```cpp
void setup() {
    // Basic configuration
    mqttManager.setServer("192.168.1.100", 1883);
    mqttManager.setCredentials("username", "password");
    mqttManager.setClientId("ESP32_Device_001");
    
    // Optional settings
    mqttManager.setMaxRetries(10);           // Default: 10
    mqttManager.setRetryInterval(5000);      // Default: 5000ms
    mqttManager.setKeepAlive(60);            // Default: 60s
}
```

### 4. Set Callbacks

```cpp
void setup() {
    // Connection successful callback
    mqttManager.onConnected([]() {
        Serial.println("MQTT connected!");
        
        // Subscribe to topics
        mqttManager.subscribe("device/commands");
        mqttManager.subscribe("device/settings");
        
        // Publish initial status
        mqttManager.publish("device/status", "online", true);
    });
    
    // Connection lost callback
    mqttManager.onDisconnected([]() {
        Serial.println("MQTT disconnected!");
    });
    
    // Message received callback
    mqttManager.onMessage([](char* topic, byte* payload, unsigned int length) {
        String message = String((char*)payload, length);
        Serial.printf("Received [%s]: %s\n", topic, message.c_str());
        
        // Handle different topics
        if (strcmp(topic, "device/commands") == 0) {
            handleCommand(message);
        }
    });
}
```

### 5. Start and Loop

```cpp
void setup() {
    // ... configuration and callbacks ...
    
    // Start MQTT manager
    mqttManager.begin();
}

void loop() {
    // Process MQTT operations (must be called regularly)
    mqttManager.loop();
    
    // Your other code here
}
```

## Advanced Usage

### Publishing Messages

```cpp
// Simple publish
mqttManager.publish("sensor/temperature", "23.5");

// Retained message
mqttManager.publish("device/config", "{\"mode\":\"auto\"}", true);

// Check if published successfully
if (mqttManager.publish("sensor/data", jsonString)) {
    Serial.println("Published successfully");
} else {
    Serial.println("Publish failed - not connected");
}
```

### Subscribing to Topics

```cpp
// Basic subscription
mqttManager.subscribe("device/commands");

// With QoS level
mqttManager.subscribe("important/topic", 1);

// Unsubscribe
mqttManager.unsubscribe("old/topic");
```

### Connection Status

```cpp
void loop() {
    mqttManager.loop();
    
    // Check connection status
    if (mqttManager.isConnected()) {
        // Safe to publish/subscribe
        mqttManager.publish("heartbeat", String(millis()));
    }
    
    // Get detailed state
    switch (mqttManager.getState()) {
        case MQTTManager::ConnectionState::DISCONNECTED:
            // Attempting to connect
            break;
        case MQTTManager::ConnectionState::CONNECTING:
            // Connection in progress
            break;
        case MQTTManager::ConnectionState::CONNECTED:
            // Ready to use
            break;
        case MQTTManager::ConnectionState::FAILED:
            // All retries exhausted
            break;
    }
}
```

### Statistics and Monitoring

```cpp
void printMqttStats() {
    Serial.printf("MQTT Uptime: %lu ms\n", mqttManager.getUptime());
    Serial.printf("Reconnect Count: %u\n", mqttManager.getReconnectCount());
    Serial.printf("Current Retry: %u\n", mqttManager.getCurrentRetry());
    Serial.printf("Last Attempt: %lu ms ago\n", 
                  millis() - mqttManager.getLastConnectionAttempt());
}
```

## Configuration Options

### Connection Parameters

```cpp
// Server configuration
mqttManager.setServer("mqtt.example.com", 8883);  // Custom port
mqttManager.setCredentials("user", "pass");        // Authentication
mqttManager.setClientId("unique_device_id");       // Client identifier

// Connection behavior
mqttManager.setKeepAlive(30);           // Heartbeat interval (seconds)
mqttManager.setMaxRetries(5);           // Maximum retry attempts
mqttManager.setRetryInterval(10000);    // Wait between retries (ms)
```

### Automatic Client ID

If no client ID is set, the manager automatically generates one based on the MAC address:

```cpp
// Auto-generated: "ESP32_AABBCCDDEEFF"
mqttManager.begin();  // No setClientId() called
```

## Integration Examples

### With Configuration Manager

```cpp
// In setup()
mqttManager.setServer(
    mqttSettings.server.get().c_str(), 
    mqttSettings.port.get()
);
mqttManager.setCredentials(
    mqttSettings.username.get().c_str(),
    mqttSettings.password.get().c_str()
);

// Callback integration
mqttManager.onConnected([]() {
    // Publish device configuration
    publishDeviceConfig();
    
    // Subscribe to configuration topics
    mqttManager.subscribe(mqttSettings.commandTopic.get().c_str());
});
```

### With Sensor Data

```cpp
Ticker sensorTicker;

void setup() {
    // ... MQTT setup ...
    
    // Publish sensor data every 30 seconds
    sensorTicker.attach(30, publishSensorData);
}

void publishSensorData() {
    if (mqttManager.isConnected()) {
        float temp = readTemperature();
        float humidity = readHumidity();
        
        mqttManager.publish("sensors/temperature", String(temp));
        mqttManager.publish("sensors/humidity", String(humidity));
    }
}
```

### Error Handling

```cpp
void handleMqttErrors() {
    static unsigned long lastCheck = 0;
    
    if (millis() - lastCheck > 60000) {  // Check every minute
        lastCheck = millis();
        
        if (mqttManager.getState() == MQTTManager::ConnectionState::FAILED) {
            Serial.println("MQTT connection failed after all retries");
            
            // Maybe restart or switch to offline mode
            if (mqttManager.getReconnectCount() > 10) {
                Serial.println("Too many failures, restarting device...");
                ESP.restart();
            }
        }
    }
}
```

## Best Practices

### 1. Regular Loop Calls
```cpp
void loop() {
    mqttManager.loop();  // Call this frequently!
    
    // Keep other operations short to avoid blocking
    delay(10);  // Small delay is OK
}
```

### 2. Message Handling
```cpp
mqttManager.onMessage([](char* topic, byte* payload, unsigned int length) {
    // Create proper string with correct length
    String message = String((char*)payload, length);
    message.trim();  // Remove whitespace
    
    // Avoid long operations in callback
    if (message.length() > 0) {
        handleMessageAsync(String(topic), message);
    }
});
```

### 3. Connection Management
```cpp
// Don't try to publish immediately after setup
mqttManager.onConnected([]() {
    // Wait a bit before publishing
    delay(100);
    mqttManager.publish("device/status", "ready");
});
```

### 4. Memory Management
```cpp
// Use const char* when possible
const char* TOPIC_TEMPERATURE = "sensors/temp";
mqttManager.publish(TOPIC_TEMPERATURE, tempString);

// Avoid String concatenation in callbacks
```

## Troubleshooting

### Common Issues

1. **No connection**: Check server IP, port, and credentials
2. **Frequent disconnects**: Increase keepAlive time or check network stability
3. **Publish fails**: Verify connection status before publishing
4. **Memory issues**: Avoid long strings and frequent allocations

### Debug Information

```cpp
void printMqttDebug() {
    Serial.printf("MQTT State: %d\n", (int)mqttManager.getState());
    Serial.printf("Connected: %s\n", mqttManager.isConnected() ? "Yes" : "No");
    Serial.printf("Retry Count: %u/%u\n", 
                  mqttManager.getCurrentRetry(), 
                  /* max retries from your config */);
}
```

## Dependencies

- `WiFi.h` - ESP32 WiFi library
- `PubSubClient.h` - MQTT client library
- `functional` - For callback support

## Thread Safety

The MQTTManager is **not thread-safe**. Always call methods from the same thread (typically the main loop thread).

## Memory Usage

- **RAM**: ~200 bytes for instance + connection buffers
- **Flash**: ~8KB compiled code
- **Stack**: Minimal during operation

## Version Compatibility

- **ESP32**: Arduino Core 2.0.0+
- **PubSubClient**: 2.8.0+
- **Arduino IDE**: 1.8.0+
- **PlatformIO**: Compatible