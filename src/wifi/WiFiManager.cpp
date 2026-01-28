#include "WiFiManager.h"
#include "../ConfigManager.h"
#include <ESP.h>
#include <esp_wifi.h>

// Logging support with verbosity levels
#define WIFI_LOG(...) CM_LOG(__VA_ARGS__)
#define WIFI_LOG_VERBOSE(...) CM_LOG_VERBOSE(__VA_ARGS__)

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
  , macFilterEnabled(false)           // MAC filtering disabled by default
  , macPriorityEnabled(false)         // MAC priority disabled by default
  , filterMac("")                     // No filter MAC by default
  , priorityMac("")                   // No priority MAC by default
  , connectAttempts(0)
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

  // Start phased connection attempts
  connectAttempts = 0;
  attemptConnect();
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

  // Start phased connection attempts
  connectAttempts = 0;
  attemptConnect();
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

  // Debug: Log current status every few calls
  // static int debugCounter = 0;
  // debugCounter++;
  // if (debugCounter % 50 == 0) { // Every 50 calls (~every 0.5 second for more frequent debugging)
  //   WIFI_LOG("[WiFi] DEBUG Update - Current State: %s, WiFi.status(): %d (%s)", 
  //          getStatusString().c_str(), 
  //          WiFi.status(), 
  //          getWiFiStatusString(WiFi.status()).c_str());
  // }

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
    // WiFi is not reporting WL_CONNECTED - log the actual status
    const int wifiStatus = WiFi.status();

    // The ESP32 WiFi stack can temporarily report WL_SCAN_COMPLETED (and sometimes WL_IDLE_STATUS)
    // even while the link is still up (e.g. during scans/roaming). If we were connected before,
    // treat this as a transient state to avoid false disconnect transitions and reconnect storms.
    if (currentState == WIFI_STATE_CONNECTED && (wifiStatus == WL_SCAN_COMPLETED || wifiStatus == WL_IDLE_STATUS)) {
      WIFI_LOG_VERBOSE("[WiFi] Transient status while connected: %d (%s)",
                       wifiStatus, getWiFiStatusString(wifiStatus).c_str());
      lastGoodConnectionMillis = millis();
    } else {
      if (currentState == WIFI_STATE_CONNECTED) {
        WIFI_LOG("[WiFi] Connection lost! WiFi.status() = %d", wifiStatus);
        transitionToState(WIFI_STATE_DISCONNECTED);
      } else if (currentState == WIFI_STATE_CONNECTING) {
        // Still trying to connect, log status periodically
        static unsigned long lastStatusLog = 0;
        if (millis() - lastStatusLog > 5000) { // Log every 5 seconds
          WIFI_LOG_VERBOSE("[WiFi] Still connecting... WiFi.status() = %d (%s)",
                           wifiStatus, getWiFiStatusString(wifiStatus).c_str());
          lastStatusLog = millis();
        }
      }

      // Handle reconnection attempts (non-blocking)
      handleReconnection();
    }
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
  
  // Create string representations
  String oldStateStr, newStateStr;
  switch (oldState) {
    case WIFI_STATE_CONNECTED: oldStateStr = "Connected"; break;
    case WIFI_STATE_CONNECTING: oldStateStr = "Connecting"; break;
    case WIFI_STATE_DISCONNECTED: oldStateStr = "Disconnected"; break;
    case WIFI_STATE_AP_MODE: oldStateStr = "AP Mode"; break;
    case WIFI_STATE_RECONNECTING: oldStateStr = "Reconnecting"; break;
    default: oldStateStr = "Unknown"; break;
  }
  
  switch (newState) {
    case WIFI_STATE_CONNECTED: newStateStr = "Connected"; break;
    case WIFI_STATE_CONNECTING: newStateStr = "Connecting"; break;
    case WIFI_STATE_DISCONNECTED: newStateStr = "Disconnected"; break;
    case WIFI_STATE_AP_MODE: newStateStr = "AP Mode"; break;
    case WIFI_STATE_RECONNECTING: newStateStr = "Reconnecting"; break;
    default: newStateStr = "Unknown"; break;
  }
  
  currentState = newState;

  // Log state transitions (with correct old and new state)
  WIFI_LOG_VERBOSE("[WiFi] State: %s -> %s", oldStateStr.c_str(), newStateStr.c_str());

  // Execute callbacks based on state transitions
  switch (newState) {
    case WIFI_STATE_CONNECTED:
      if (oldState != WIFI_STATE_CONNECTED) {
        WIFI_LOG_VERBOSE("[WiFi] Connected! IP: %s", WiFi.localIP().toString().c_str());
        // Reset connection attempt counter on success
        connectAttempts = 0;
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

  // Avoid reconnect storms while the WiFi stack is already busy.
  // Some ESP32 Arduino stacks report WL_IDLE_STATUS during an ongoing connect and
  // WL_SCAN_COMPLETED transiently during scans.
  const int wifiStatus = WiFi.status();
  if (wifiStatus == WL_IDLE_STATUS || wifiStatus == WL_SCAN_COMPLETED) {
    return;
  }

  unsigned long now = millis();

  // Non-blocking reconnection attempt
  if (now - lastReconnectAttempt >= reconnectInterval) {
    lastReconnectAttempt = now;
    if (currentState != WIFI_STATE_RECONNECTING) {
      transitionToState(WIFI_STATE_RECONNECTING);
    }
    attemptConnect();
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

// Attempt connection with phased strategy:
//  - Attempt 1 (phase 0): normal connect (no stack reset)
//  - Attempt 2 (phase 1): perform WiFi stack reset, then connect
//  - Attempt 3+ (phase >=2): restart ESP
void ConfigManagerWiFi::attemptConnect() {
  uint8_t phase = connectAttempts;

  if (phase == 0) {
    WIFI_LOG_VERBOSE("[WiFi] Attempt 1: normal connect (no stack reset)");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    if (!useDHCP) {
      applyStaticConfig();
    }
  } else if (phase == 1) {
    WIFI_LOG("[WiFi] Attempt 2: performing WiFi stack reset, then reconnect");
    performWiFiStackReset();
    if (!useDHCP) {
      applyStaticConfig();
    }
  } else {
    WIFI_LOG("[WiFi] Attempt %d: restarting ESP due to repeated connection failures", phase + 1);
    ESP.restart();
    return; // restart should not return
  }

  // BSSID selection if configured
  String targetBSSID = findBestBSSID();
  if (!targetBSSID.isEmpty()) {
    WIFI_LOG("[WiFi] Using specific BSSID: %s", targetBSSID.c_str());
    uint8_t bssid[6];
    unsigned int tmp[6];
    int matched = sscanf(targetBSSID.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x",
                         &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]);
    if (matched == 6) {
      for (int i = 0; i < 6; ++i) bssid[i] = static_cast<uint8_t>(tmp[i] & 0xFF);
      WiFi.begin(ssid.c_str(), password.c_str(), 0, bssid);
    } else {
      WIFI_LOG("[WiFi] Invalid BSSID format '%s', falling back to auto BSSID", targetBSSID.c_str());
      WiFi.begin(ssid.c_str(), password.c_str());
    }
  } else {
    WiFi.begin(ssid.c_str(), password.c_str());
  }

  // Advance state and counters
  transitionToState(WIFI_STATE_CONNECTING);
  lastReconnectAttempt = millis();
  connectAttempts++;
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

  // Only check roaming if we're currently connected
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  unsigned long currentTime = millis();
  
  // Check cooldown period (skip if this is the first roaming attempt)
  if (lastRoamingAttempt > 0 && currentTime - lastRoamingAttempt < roamingCooldown) {
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
  String preferredBSSID = "";

  // Find the best network with our SSID, considering MAC filtering/priority
  for (int i = 0; i < networkCount; i++) {
    if (WiFi.SSID(i) == ssid) {
      int networkRSSI = WiFi.RSSI(i);
      String networkBSSID = WiFi.BSSIDstr(i);
      
      // Apply MAC filtering if enabled
      if (macFilterEnabled) {
        if (networkBSSID.equalsIgnoreCase(filterMac)) {
          // Only consider this AP if it matches the filter
          if (networkRSSI > bestRSSI + roamingImprovement) {
            bestRSSI = networkRSSI;
            bestNetworkIndex = i;
          }
        }
        // Skip all other APs when filter is enabled
        continue;
      }
      
      // Apply MAC priority if enabled
      if (macPriorityEnabled && networkBSSID.equalsIgnoreCase(priorityMac)) {
        // Prefer priority MAC even with smaller improvement requirement
        if (networkRSSI > bestRSSI + (roamingImprovement / 2)) {
          bestRSSI = networkRSSI;
          bestNetworkIndex = i;
          preferredBSSID = networkBSSID;
        }
      } else {
        // Check if this network is significantly better
        if (networkRSSI > bestRSSI + roamingImprovement) {
          // Only use if no priority MAC was found or this is much better
          if (preferredBSSID.isEmpty() || networkRSSI > bestRSSI + roamingImprovement) {
            bestRSSI = networkRSSI;
            bestNetworkIndex = i;
          }
        }
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

// MAC Address Filtering and Priority implementation
void ConfigManagerWiFi::setWifiAPMacFilter(const String& macAddress) {
  filterMac = macAddress;
  macFilterEnabled = true;
  macPriorityEnabled = false; // Filter mode disables priority mode
  WIFI_LOG("[WiFi] MAC Filter enabled for: %s", macAddress.c_str());
}

void ConfigManagerWiFi::setWifiAPMacPriority(const String& macAddress) {
  priorityMac = macAddress;
  macPriorityEnabled = true;
  macFilterEnabled = false; // Priority mode disables filter mode
  WIFI_LOG("[WiFi] MAC Priority enabled for: %s", macAddress.c_str());
}

void ConfigManagerWiFi::clearMacFilter() {
  macFilterEnabled = false;
  filterMac = "";
  WIFI_LOG("[WiFi] MAC Filter disabled");
}

void ConfigManagerWiFi::clearMacPriority() {
  macPriorityEnabled = false;
  priorityMac = "";
  WIFI_LOG("[WiFi] MAC Priority disabled");
}

bool ConfigManagerWiFi::isMacFilterEnabled() const {
  return macFilterEnabled;
}

bool ConfigManagerWiFi::isMacPriorityEnabled() const {
  return macPriorityEnabled;
}

String ConfigManagerWiFi::getFilterMac() const {
  return filterMac;
}

String ConfigManagerWiFi::getPriorityMac() const {
  return priorityMac;
}

// Helper method to find the best BSSID considering MAC filter/priority
String ConfigManagerWiFi::findBestBSSID() {
  if (ssid.isEmpty()) {
    WIFI_LOG("[WiFi] No SSID set, skipping BSSID selection");
    return "";
  }

  // If no MAC filtering/priority is enabled, let WiFi auto-connect
  if (!macFilterEnabled && !macPriorityEnabled) {
    WIFI_LOG_VERBOSE("[WiFi] No MAC filter/priority enabled, using auto-connect");
    return "";
  }

  WIFI_LOG_VERBOSE("[WiFi] Scanning for networks to apply MAC filter/priority...");
  
  // Clear any previous scan results first
  WiFi.scanDelete();
  
  int networkCount = WiFi.scanNetworks(false, false, false, 300); // Reduced scan time
  
  if (networkCount <= 0) {
    WIFI_LOG("[WiFi] No networks found during scan (count: %d), falling back to auto-connect", networkCount);
    WiFi.scanDelete(); // Ensure cleanup even on failure
    return "";
  }

  WIFI_LOG_VERBOSE("[WiFi] Found %d networks during scan", networkCount);

  String bestBSSID = "";
  int bestRSSI = -100; // Very weak signal as starting point
  bool priorityFound = false;
  int matchingNetworks = 0;

  for (int i = 0; i < networkCount; i++) {
    String networkSSID = WiFi.SSID(i);
    if (networkSSID == ssid) {
      matchingNetworks++;
      String networkBSSID = WiFi.BSSIDstr(i);
      int networkRSSI = WiFi.RSSI(i);
      
      WIFI_LOG_VERBOSE("[WiFi] Found matching network: SSID=%s, BSSID=%s, RSSI=%d", 
                       networkSSID.c_str(), networkBSSID.c_str(), networkRSSI);

      // MAC Filter mode: only connect to specific MAC
      if (macFilterEnabled) {
        if (networkBSSID.equalsIgnoreCase(filterMac)) {
          if (networkRSSI > bestRSSI) {
            bestBSSID = networkBSSID;
            bestRSSI = networkRSSI;
            WIFI_LOG("[WiFi] Filter match found: %s (RSSI: %d dBm)", networkBSSID.c_str(), networkRSSI);
          }
        }
        continue; // Skip all other APs when filter is enabled
      }

      // MAC Priority mode: prefer specific MAC, allow fallback
      if (macPriorityEnabled) {
        if (networkBSSID.equalsIgnoreCase(priorityMac)) {
          // Always prefer the priority MAC if found
          bestBSSID = networkBSSID;
          bestRSSI = networkRSSI;
          priorityFound = true;
          WIFI_LOG("[WiFi] Found priority AP: %s (RSSI: %d dBm)", networkBSSID.c_str(), networkRSSI);
          break; // Stop searching once priority AP is found
        } else if (!priorityFound) {
          // Fallback option: use best available if no priority found yet
          if (networkRSSI > bestRSSI) {
            bestBSSID = networkBSSID;
            bestRSSI = networkRSSI;
            WIFI_LOG_VERBOSE("[WiFi] Fallback candidate: %s (RSSI: %d dBm)", networkBSSID.c_str(), networkRSSI);
          }
        }
      }
    }
  }

  // Clean up scan results
  WiFi.scanDelete();

  WIFI_LOG("[WiFi] Scan complete: %d matching networks found", matchingNetworks);

  if (!bestBSSID.isEmpty()) {
    WIFI_LOG_VERBOSE("[WiFi] Selected BSSID: %s (RSSI: %d dBm)", bestBSSID.c_str(), bestRSSI);
    // If priority was configured but not found, make that explicit even when we have a fallback
    if (macPriorityEnabled && !priorityFound) {
      WIFI_LOG("[WiFi] MAC Priority target %s not found; using best available AP %s (RSSI: %d dBm)",
               priorityMac.c_str(), bestBSSID.c_str(), bestRSSI);
    }
  } else if (macFilterEnabled) {
    WIFI_LOG("[WiFi] MAC Filter enabled but target AP %s not found", filterMac.c_str());
  } else if (macPriorityEnabled) {
    WIFI_LOG("[WiFi] MAC Priority enabled but target AP %s not found, will use auto-connect", priorityMac.c_str());
  }

  return bestBSSID;
}

String ConfigManagerWiFi::getWiFiStatusString(int status) const {
  switch (status) {
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
    case WL_CONNECTED: return "WL_CONNECTED";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED: return "WL_DISCONNECTED";
    default: return "UNKNOWN_STATUS";
  }
}

void ConfigManagerWiFi::performWiFiStackReset() {
  WIFI_LOG("[WiFi] Performing complete WiFi stack reset for connectivity fix...");
  
  // 1. Complete WiFi shutdown
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(500);  // Longer delay for complete shutdown
  
  // 2. Reset WiFi stack (ESP32 specific)
  esp_wifi_stop();
  esp_wifi_deinit();
  delay(200);
  
  // 3. Reinitialize WiFi stack
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  delay(200);
  
  // 4. Set WiFi mode and configuration
  WiFi.mode(WIFI_STA);
  delay(100);
  
  // Set additional WiFi parameters for stability
  WiFi.setSleep(false);       // Disable WiFi sleep
  WiFi.setAutoReconnect(true);  // Enable auto-reconnect
  WiFi.persistent(true);      // Store WiFi configuration in flash
  WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Set specific power level
  
  WIFI_LOG("[WiFi] WiFi stack reset complete - WiFi.mode() = %d, WiFi.status() = %d", 
         WiFi.getMode(), WiFi.status());
}
