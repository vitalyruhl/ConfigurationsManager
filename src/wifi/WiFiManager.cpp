#include "WiFiManager.h"
#include <ESP.h>

// Logging support with verbosity levels
#if CM_ENABLE_LOGGING
extern std::function<void(const char*)> ConfigManagerClass_logger;
#define WIFI_LOG(...) if(ConfigManagerClass_logger) { char buf[256]; snprintf(buf, sizeof(buf), __VA_ARGS__); ConfigManagerClass_logger(buf); }
#if CM_ENABLE_VERBOSE_LOGGING
#define WIFI_LOG_VERBOSE(...) WIFI_LOG(__VA_ARGS__)
#else
#define WIFI_LOG_VERBOSE(...)
#endif
#else
#define WIFI_LOG(...)
#define WIFI_LOG_VERBOSE(...)
#endif

ConfigManagerWiFi::ConfigManagerWiFi()
  : currentState(WIFI_STATE_DISCONNECTED)
  , autoRebootEnabled(false)
  , initialized(false)
  , lastGoodConnectionMillis(0)
  , lastReconnectAttempt(0)
  , reconnectInterval(10000)
  , autoRebootTimeoutMs(0)
  , useDHCP(true)
  , onConnectedCallback(nullptr)
  , onDisconnectedCallback(nullptr)
  , onAPModeCallback(nullptr)
  , smartRoamingEnabled(true)         // Default enabled
  , roamingThreshold(-75)             // Default -75 dBm
  , roamingCooldown(120000)           // Default 120 seconds in ms
  , roamingImprovement(10)            // Default 10 dBm improvement
  , lastRoamingAttempt(0)
{
}

void ConfigManagerWiFi::begin(unsigned long reconnectIntervalMs, unsigned long autoRebootTimeoutMin) {
  reconnectInterval = reconnectIntervalMs;
  autoRebootTimeoutMs = autoRebootTimeoutMin * 60000UL; // Convert minutes to milliseconds
  autoRebootEnabled = (autoRebootTimeoutMin > 0);

  // Initialize timing
  lastGoodConnectionMillis = millis();
  lastReconnectAttempt = 0;

  // Determine initial state
  if (WiFi.getMode() == WIFI_AP) {
    currentState = WIFI_STATE_AP_MODE;
    WIFI_LOG("[WiFi] Starting in AP mode");
  } else if (WiFi.status() == WL_CONNECTED) {
    currentState = WIFI_STATE_CONNECTED;
    lastGoodConnectionMillis = millis();
    WIFI_LOG_VERBOSE("[WiFi] Already connected to %s", WiFi.SSID().c_str());
  } else {
    currentState = WIFI_STATE_DISCONNECTED;
    WIFI_LOG_VERBOSE("[WiFi] Starting disconnected");
  }

  initialized = true;
}

void ConfigManagerWiFi::setCallbacks(WiFiConnectedCallback onConnected, WiFiDisconnectedCallback onDisconnected, WiFiAPModeCallback onAPMode) {
  onConnectedCallback = onConnected;
  onDisconnectedCallback = onDisconnected;
  onAPModeCallback = onAPMode;
}

void ConfigManagerWiFi::startConnection(const String& wifiSSID, const String& wifiPassword) {
  ssid = wifiSSID;
  password = wifiPassword;
  useDHCP = true;
  dns1 = IPAddress();
  dns2 = IPAddress();

  WIFI_LOG_VERBOSE("[WiFi] Starting DHCP connection to %s", ssid.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Disable WiFi sleep to prevent disconnections
  WiFi.setAutoReconnect(true); // Enable automatic reconnection
  WiFi.persistent(true); // Store WiFi configuration in flash
  WiFi.begin(ssid.c_str(), password.c_str());

  transitionToState(WIFI_STATE_CONNECTING);
  lastReconnectAttempt = millis();
}

void ConfigManagerWiFi::startConnection(const IPAddress& sIP, const IPAddress& gw, const IPAddress& sn, const String& wifiSSID, const String& wifiPassword, const IPAddress& primaryDNS, const IPAddress& secondaryDNS) {
  ssid = wifiSSID;
  password = wifiPassword;
  staticIP = sIP;
  gateway = gw;
  subnet = sn;
  dns1 = primaryDNS;
  dns2 = secondaryDNS;
  useDHCP = false;

  WIFI_LOG_VERBOSE("[WiFi] Starting static IP connection to %s (IP: %s, DNS1: %s, DNS2: %s)",
           ssid.c_str(),
           staticIP.toString().c_str(),
           (dns1 == IPAddress()) ? "0.0.0.0" : dns1.toString().c_str(),
           (dns2 == IPAddress()) ? "0.0.0.0" : dns2.toString().c_str());

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Disable WiFi sleep to prevent disconnections
  WiFi.setAutoReconnect(true); // Enable automatic reconnection
  WiFi.persistent(true); // Store WiFi configuration in flash
  applyStaticConfig();
  WiFi.begin(ssid.c_str(), password.c_str());

  transitionToState(WIFI_STATE_CONNECTING);
  lastReconnectAttempt = millis();
}

void ConfigManagerWiFi::applyStaticConfig() {
  IPAddress zero;
  bool hasPrimary = dns1 != zero;
  bool hasSecondary = dns2 != zero;

  if (hasPrimary && hasSecondary) {
    WiFi.config(staticIP, gateway, subnet, dns1, dns2);
  } else if (hasPrimary) {
    WiFi.config(staticIP, gateway, subnet, dns1);
  } else {
    WiFi.config(staticIP, gateway, subnet);
  }
}

void ConfigManagerWiFi::startAccessPoint(const String& apSSID, const String& apPassword) {
  WIFI_LOG_VERBOSE("[WiFi] Starting Access Point: %s", apSSID.c_str());

  WiFi.mode(WIFI_AP);
  if (apPassword.length() > 0) {
    WiFi.softAP(apSSID.c_str(), apPassword.c_str());
  } else {
    WiFi.softAP(apSSID.c_str());
  }

  transitionToState(WIFI_STATE_AP_MODE);
}

void ConfigManagerWiFi::update() {
  if (!initialized) return;

  WiFiManagerState previousState = currentState;

  // Determine current WiFi state
  if (WiFi.getMode() == WIFI_AP) {
    if (currentState != WIFI_STATE_AP_MODE) {
      transitionToState(WIFI_STATE_AP_MODE);
    }
  } else if (WiFi.status() == WL_CONNECTED) {
    if (currentState != WIFI_STATE_CONNECTED) {
      // Log detailed connection info when first connecting
      WIFI_LOG_VERBOSE("[WiFi] WiFi.status() = WL_CONNECTED, IP: %s, Gateway: %s, DNS: %s",
               WiFi.localIP().toString().c_str(),
               WiFi.gatewayIP().toString().c_str(),
               WiFi.dnsIP().toString().c_str());
      transitionToState(WIFI_STATE_CONNECTED);
    }
    // Update last good connection time
    lastGoodConnectionMillis = millis();
  } else {
    // WiFi is disconnected - log the actual status
    if (currentState == WIFI_STATE_CONNECTED) {
      WIFI_LOG("[WiFi] Connection lost! WiFi.status() = %d, transitioning to disconnected", WiFi.status());
      transitionToState(WIFI_STATE_DISCONNECTED);
    }

    // Handle reconnection attempts (non-blocking)
    handleReconnection();
  }

  // Check auto-reboot condition
  if (autoRebootEnabled && currentState != WIFI_STATE_AP_MODE) {
    checkAutoReboot();
  }

  // Check smart roaming when connected
  if (currentState == WIFI_STATE_CONNECTED) {
    checkSmartRoaming();
  }
}

void ConfigManagerWiFi::transitionToState(WiFiManagerState newState) {
  WiFiManagerState oldState = currentState;
  currentState = newState;

  // Log state transitions (verbose only)
  WIFI_LOG_VERBOSE("[WiFi] State: %s -> %s", getStatusString().c_str(), getStatusString().c_str());

  // Execute callbacks based on state transitions
  switch (newState) {
    case WIFI_STATE_CONNECTED:
      if (oldState != WIFI_STATE_CONNECTED) {
        WIFI_LOG("[WiFi] Connected! IP: %s", WiFi.localIP().toString().c_str());
        if (onConnectedCallback) {
          onConnectedCallback();
        }
      }
      break;

    case WIFI_STATE_DISCONNECTED:
    case WIFI_STATE_RECONNECTING:
      if (oldState == WIFI_STATE_CONNECTED) {
        WIFI_LOG_VERBOSE("[WiFi] Disconnected from %s", ssid.c_str());
        if (onDisconnectedCallback) {
          onDisconnectedCallback();
        }
      }
      break;

    case WIFI_STATE_AP_MODE:
      if (oldState != WIFI_STATE_AP_MODE) {
        WIFI_LOG_VERBOSE("[WiFi] Access Point mode active");
        if (onAPModeCallback) {
          onAPModeCallback();
        }
      }
      break;

    case WIFI_STATE_CONNECTING:
      WIFI_LOG_VERBOSE("[WiFi] Connecting to %s...", ssid.c_str());
      break;

    default:
      break;
  }
}

void ConfigManagerWiFi::handleReconnection() {
  if (WiFi.getMode() == WIFI_AP) return; // Don't reconnect in AP mode

  unsigned long now = millis();

  // Non-blocking reconnection attempt
  if (now - lastReconnectAttempt >= reconnectInterval) {
    lastReconnectAttempt = now;

    if (currentState != WIFI_STATE_RECONNECTING) {
      transitionToState(WIFI_STATE_RECONNECTING);
    }

    // Attempt non-blocking reconnection
    WIFI_LOG_VERBOSE("[WiFi] Attempting reconnection... Current WiFi.status() = %d", WiFi.status());
    WiFi.setSleep(false); // Ensure WiFi sleep is disabled on reconnection attempts
    WiFi.setAutoReconnect(true); // Enable automatic reconnection
    if (useDHCP) {
      WiFi.begin(ssid.c_str(), password.c_str());
    } else {
      applyStaticConfig();
      WiFi.begin(ssid.c_str(), password.c_str());
    }
  }
}

void ConfigManagerWiFi::checkAutoReboot() {
  if (!autoRebootEnabled || autoRebootTimeoutMs == 0) return;

  unsigned long now = millis();
  unsigned long timeSinceLastConnection = now - lastGoodConnectionMillis;

  if (timeSinceLastConnection >= autoRebootTimeoutMs) {
    // Time for auto-reboot
    WIFI_LOG("[WiFi] Auto-reboot triggered after %lu ms without connection", timeSinceLastConnection);
    ESP.restart();
  }
}

// State queries
WiFiManagerState ConfigManagerWiFi::getState() const {
  return currentState;
}

bool ConfigManagerWiFi::isConnected() const {
  return currentState == WIFI_STATE_CONNECTED;
}

bool ConfigManagerWiFi::isInAPMode() const {
  return currentState == WIFI_STATE_AP_MODE;
}

bool ConfigManagerWiFi::isConnecting() const {
  return currentState == WIFI_STATE_CONNECTING || currentState == WIFI_STATE_RECONNECTING;
}

unsigned long ConfigManagerWiFi::getLastConnectionTime() const {
  return lastGoodConnectionMillis;
}

unsigned long ConfigManagerWiFi::getTimeSinceLastConnection() const {
  return millis() - lastGoodConnectionMillis;
}

// Control functions
void ConfigManagerWiFi::enableAutoReboot(bool enable) {
  autoRebootEnabled = enable;
}

void ConfigManagerWiFi::setAutoRebootTimeout(unsigned long timeoutMinutes) {
  autoRebootTimeoutMs = timeoutMinutes * 60000UL;
  autoRebootEnabled = (timeoutMinutes > 0);
}

void ConfigManagerWiFi::setReconnectInterval(unsigned long intervalMs) {
  reconnectInterval = intervalMs;
}

void ConfigManagerWiFi::forceReconnect() {
  lastReconnectAttempt = 0; // Reset timer to trigger immediate reconnect
}

void ConfigManagerWiFi::reconnect() {
  WIFI_LOG("[WiFi] Manual reconnect requested");
  WiFi.disconnect();
  forceReconnect();
}

void ConfigManagerWiFi::disconnect() {
  WIFI_LOG("[WiFi] Manual disconnect requested");
  WiFi.disconnect();
  transitionToState(WIFI_STATE_DISCONNECTED);
}

void ConfigManagerWiFi::reset() {
  currentState = WIFI_STATE_DISCONNECTED;
  lastGoodConnectionMillis = millis();
  lastReconnectAttempt = 0;
}

// Status information
String ConfigManagerWiFi::getStatusString() const {
  switch (currentState) {
    case WIFI_STATE_CONNECTED:
      return "Connected";
    case WIFI_STATE_CONNECTING:
      return "Connecting";
    case WIFI_STATE_DISCONNECTED:
      return "Disconnected";
    case WIFI_STATE_AP_MODE:
      return "AP Mode";
    case WIFI_STATE_RECONNECTING:
      return "Reconnecting";
    default:
      return "Unknown";
  }
}

float ConfigManagerWiFi::getConnectionUptime() const {
  if (currentState == WIFI_STATE_CONNECTED) {
    return (millis() - lastGoodConnectionMillis) / 1000.0f;
  }
  return 0.0f;
}

IPAddress ConfigManagerWiFi::getLocalIP() const {
  return WiFi.localIP();
}

int ConfigManagerWiFi::getRSSI() const {
  return WiFi.RSSI();
}

// Smart WiFi Roaming implementation
void ConfigManagerWiFi::enableSmartRoaming(bool enable) {
  smartRoamingEnabled = enable;
  WIFI_LOG_VERBOSE("[WiFi] Smart Roaming %s", enable ? "enabled" : "disabled");
}

void ConfigManagerWiFi::setRoamingThreshold(int thresholdDbm) {
  roamingThreshold = thresholdDbm;
  WIFI_LOG_VERBOSE("[WiFi] Roaming threshold set to %d dBm", thresholdDbm);
}

void ConfigManagerWiFi::setRoamingCooldown(unsigned long cooldownSeconds) {
  roamingCooldown = cooldownSeconds * 1000; // Convert to milliseconds
  WIFI_LOG_VERBOSE("[WiFi] Roaming cooldown set to %lu seconds", cooldownSeconds);
}

void ConfigManagerWiFi::setRoamingImprovement(int improvementDbm) {
  roamingImprovement = improvementDbm;
  WIFI_LOG_VERBOSE("[WiFi] Roaming improvement threshold set to %d dBm", improvementDbm);
}

bool ConfigManagerWiFi::isSmartRoamingEnabled() const {
  return smartRoamingEnabled;
}

void ConfigManagerWiFi::checkSmartRoaming() {
  if (!smartRoamingEnabled || ssid.isEmpty()) {
    return;
  }

  unsigned long currentTime = millis();
  
  // Check cooldown period
  if (currentTime - lastRoamingAttempt < roamingCooldown) {
    return;
  }

  int currentRSSI = WiFi.RSSI();
  
  // Only check if signal is below threshold
  if (currentRSSI >= roamingThreshold) {
    return;
  }

  WIFI_LOG_VERBOSE("[WiFi] Current RSSI (%d dBm) below threshold (%d dBm), scanning for better APs...", 
           currentRSSI, roamingThreshold);

  // Scan for networks
  int networkCount = WiFi.scanNetworks();
  if (networkCount <= 0) {
    WIFI_LOG_VERBOSE("[WiFi] No networks found during roaming scan");
    return;
  }

  int bestRSSI = currentRSSI;
  int bestNetworkIndex = -1;

  // Find the best network with our SSID
  for (int i = 0; i < networkCount; i++) {
    if (WiFi.SSID(i) == ssid) {
      int networkRSSI = WiFi.RSSI(i);
      
      // Check if this network is significantly better
      if (networkRSSI > bestRSSI + roamingImprovement) {
        bestRSSI = networkRSSI;
        bestNetworkIndex = i;
      }
    }
  }

  if (bestNetworkIndex != -1) {
    String bestBSSID = WiFi.BSSIDstr(bestNetworkIndex);
    String currentBSSID = WiFi.BSSIDstr();
    
    // Don't roam to the same AP
    if (bestBSSID != currentBSSID) {
      WIFI_LOG_VERBOSE("[WiFi] Found better AP: %s (RSSI: %d dBm, improvement: %d dBm)", 
               bestBSSID.c_str(), bestRSSI, bestRSSI - currentRSSI);
      
      // Disconnect and reconnect to trigger roaming
      WiFi.disconnect();
      delay(500);
      
      if (useDHCP) {
        WiFi.begin(ssid.c_str(), password.c_str());
      } else {
        WiFi.config(staticIP, gateway, subnet, dns1, dns2);
        WiFi.begin(ssid.c_str(), password.c_str());
      }
      
      lastRoamingAttempt = currentTime;
      WIFI_LOG_VERBOSE("[WiFi] Initiated roaming to better access point");
    }
  } else {
    WIFI_LOG_VERBOSE("[WiFi] No better AP found (current: %d dBm)", currentRSSI);
  }

  // Clean up scan results
  WiFi.scanDelete();
}