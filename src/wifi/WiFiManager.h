#pragma once

#include <WiFi.h>
#include <functional>
#include "../ConfigManagerConfig.h"

// WiFi connection states
enum WiFiManagerState {
  WIFI_STATE_DISCONNECTED,
  WIFI_STATE_CONNECTING,
  WIFI_STATE_CONNECTED,
  WIFI_STATE_AP_MODE,
  WIFI_STATE_RECONNECTING
};

// Callback function types
typedef std::function<void()> WiFiConnectedCallback;
typedef std::function<void()> WiFiDisconnectedCallback;
typedef std::function<void()> WiFiAPModeCallback;

class ConfigManagerWiFi {
private:
  // State management
  WiFiManagerState currentState;
  bool autoRebootEnabled;
  bool initialized;
  
  // Timing variables
  unsigned long lastGoodConnectionMillis;
  unsigned long lastReconnectAttempt;
  unsigned long reconnectInterval;
  unsigned long autoRebootTimeoutMs;
  
  // Connection parameters
  String ssid;
  String password;
  IPAddress staticIP;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns1;
  IPAddress dns2;
  bool useDHCP;
  
  // Callback functions
  WiFiConnectedCallback onConnectedCallback;
  WiFiDisconnectedCallback onDisconnectedCallback;
  WiFiAPModeCallback onAPModeCallback;
  
  // Private methods
  void transitionToState(WiFiManagerState newState);
  void handleReconnection();
  void checkAutoReboot();
  void log(const char* format, ...) const;
  void applyStaticConfig();
  
public:
  // Constructor
  ConfigManagerWiFi();
  
  // Initialization
  void begin(unsigned long reconnectIntervalMs = 10000, unsigned long autoRebootTimeoutMin = 0);
  void setCallbacks(WiFiConnectedCallback onConnected, WiFiDisconnectedCallback onDisconnected, WiFiAPModeCallback onAPMode = nullptr);
  
  // Connection management - NON-BLOCKING
  void startConnection(const String& ssid, const String& password);
  void startConnection(const IPAddress& staticIP, const IPAddress& gateway, const IPAddress& subnet, const String& ssid, const String& password, const IPAddress& dns1 = IPAddress(), const IPAddress& dns2 = IPAddress());
  void startAccessPoint(const String& apSSID, const String& apPassword = "");
  
  // Main update function (call in loop) - NON-BLOCKING
  void update();
  
  // State queries
  WiFiManagerState getState() const;
  bool isConnected() const;
  bool isInAPMode() const;
  bool isConnecting() const;
  unsigned long getLastConnectionTime() const;
  unsigned long getTimeSinceLastConnection() const;
  
  // Control functions
  void enableAutoReboot(bool enable);
  void setAutoRebootTimeout(unsigned long timeoutMinutes);
  void setReconnectInterval(unsigned long intervalMs);
  void forceReconnect();
  void reconnect();
  void disconnect();
  void reset();
  
  // Status information
  String getStatusString() const;
  float getConnectionUptime() const; // in seconds
  IPAddress getLocalIP() const;
  int getRSSI() const;
  
  // Compatibility methods for ConfigManager
  bool getStatus() const { return isConnected(); }
};