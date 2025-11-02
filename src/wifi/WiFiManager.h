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

  // Smart WiFi Roaming
  bool smartRoamingEnabled;
  int roamingThreshold;        // dBm threshold for roaming trigger
  unsigned long roamingCooldown; // Minimum time between roaming attempts (ms)
  int roamingImprovement;      // Minimum signal improvement required (dBm)
  unsigned long lastRoamingAttempt;

  // MAC Address Filtering and Priority
  bool macFilterEnabled;       // If true, only connect to specific MAC
  bool macPriorityEnabled;     // If true, prefer specific MAC but allow fallback
  String filterMac;            // MAC address for filtering/priority
  String priorityMac;          // MAC address for priority (same as filterMac for simplicity)

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
  void checkSmartRoaming();  // Smart roaming check method
  String findBestBSSID();    // Find best BSSID considering MAC filter/priority

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

  // Smart WiFi Roaming configuration
  void enableSmartRoaming(bool enable = true);
  void setRoamingThreshold(int thresholdDbm = -75);
  void setRoamingCooldown(unsigned long cooldownSeconds = 120);
  void setRoamingImprovement(int improvementDbm = 10);
  bool isSmartRoamingEnabled() const;

  // MAC Address Filtering and Priority
  void setWifiAPMacFilter(const String& macAddress);     // Only connect to this MAC
  void setWifiAPMacPriority(const String& macAddress);   // Prefer this MAC, fallback to others
  void clearMacFilter();                                  // Remove MAC filtering
  void clearMacPriority();                               // Remove MAC priority
  bool isMacFilterEnabled() const;
  bool isMacPriorityEnabled() const;
  String getFilterMac() const;
  String getPriorityMac() const;

  // Compatibility methods for ConfigManager
  bool getStatus() const { return isConnected(); }
};