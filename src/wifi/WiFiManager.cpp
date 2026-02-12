#include "WiFiManager.h"
#include "../ConfigManager.h"
#include <ESP.h>
#include <esp_system.h>
#include <esp_wifi.h>

// Logging support with verbosity levels
#define WIFI_LOG(...) CM_LOG("[WiFi] " __VA_ARGS__)
#define WIFI_LOG_VERBOSE(...) CM_LOG_VERBOSE("[WiFi] " __VA_ARGS__)

namespace
{
constexpr uint32_t kRestartMarkerMagic = 0x434D5752; // "CMWR"
constexpr uint32_t kRestartCauseWiFiAutoReboot = 1;

RTC_DATA_ATTR uint32_t g_restartMarkerMagic = 0;
RTC_DATA_ATTR uint32_t g_restartMarkerCause = 0;

const char *resetReasonToStr(const esp_reset_reason_t reason)
{
  switch (reason)
  {
  case ESP_RST_POWERON:
    return "POWERON";
  case ESP_RST_EXT:
    return "EXTERNAL";
  case ESP_RST_SW:
    return "SW";
  case ESP_RST_PANIC:
    return "PANIC";
  case ESP_RST_INT_WDT:
    return "INT_WDT";
  case ESP_RST_TASK_WDT:
    return "TASK_WDT";
  case ESP_RST_WDT:
    return "WDT";
  case ESP_RST_DEEPSLEEP:
    return "DEEPSLEEP";
  case ESP_RST_BROWNOUT:
    return "BROWNOUT";
  case ESP_RST_SDIO:
    return "SDIO";
  default:
    return "UNKNOWN";
  }
}

void markRestartCause(const uint32_t cause)
{
  g_restartMarkerMagic = kRestartMarkerMagic;
  g_restartMarkerCause = cause;
}

void logAndClearRestartMarker()
{
  if (g_restartMarkerMagic == kRestartMarkerMagic)
  {
    if (g_restartMarkerCause == kRestartCauseWiFiAutoReboot)
    {
      WIFI_LOG("[INFO] Previous restart marker: WiFi auto-reboot");
    }
    else
    {
      WIFI_LOG("[INFO] Previous restart marker: cause=%lu", static_cast<unsigned long>(g_restartMarkerCause));
    }
  }

  g_restartMarkerMagic = 0;
  g_restartMarkerCause = 0;
}
} // namespace

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
  , lastNoSsidScanMillis(0)
  , noSsidScanStartMillis(0)
  , roamingReconnectPending(false)
  , roamingReconnectAtMs(0)
  , stackResetInProgress(false)
  , connectAfterStackReset(false)
  , stackResetStep(0)
  , stackResetStepAtMs(0)
{
}

void ConfigManagerWiFi::begin(unsigned long reconnectIntervalMs, unsigned long autoRebootTimeoutMin) {
  const esp_reset_reason_t rr = esp_reset_reason();
  WIFI_LOG_VERBOSE("Reset reason: %s (%d)", resetReasonToStr(rr), static_cast<int>(rr));
  logAndClearRestartMarker();

  reconnectInterval = reconnectIntervalMs;
  autoRebootTimeoutMs = autoRebootTimeoutMin * 60000UL; // Convert minutes to milliseconds
  autoRebootEnabled = (autoRebootTimeoutMin > 0);

  WIFI_LOG_VERBOSE("Config: reconnectInterval=%lu ms, autoReboot=%s (%lu min)",
                   reconnectInterval,
                   autoRebootEnabled ? "enabled" : "disabled",
                   autoRebootTimeoutMin);

  // Initialize timing
  lastGoodConnectionMillis = millis();
  lastReconnectAttempt = 0;

  // Determine initial state
  if (WiFi.getMode() == WIFI_AP) {
    currentState = WIFI_STATE_AP_MODE;
    WIFI_LOG("Starting in AP mode");
  } else if (WiFi.status() == WL_CONNECTED) {
    currentState = WIFI_STATE_CONNECTED;
    lastGoodConnectionMillis = millis();
    WIFI_LOG_VERBOSE("Already connected to %s", WiFi.SSID().c_str());
  } else {
    currentState = WIFI_STATE_DISCONNECTED;
    WIFI_LOG_VERBOSE("Starting disconnected");
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

  WIFI_LOG_VERBOSE("Starting DHCP connection to %s", ssid.c_str());

  // Start phased connection attempts
  roamingReconnectPending = false;
  connectAfterStackReset = false;
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

  WIFI_LOG_VERBOSE("Starting static IP connection to %s (IP: %s, DNS1: %s, DNS2: %s)",
           ssid.c_str(),
           staticIP.toString().c_str(),
           (dns1 == IPAddress()) ? "0.0.0.0" : dns1.toString().c_str(),
           (dns2 == IPAddress()) ? "0.0.0.0" : dns2.toString().c_str());

  // Start phased connection attempts
  roamingReconnectPending = false;
  connectAfterStackReset = false;
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
  WIFI_LOG_VERBOSE("Starting Access Point: %s", apSSID.c_str());

  WiFi.mode(WIFI_AP);
  if (apPassword.length() > 0) {
    WiFi.softAP(apSSID.c_str(), apPassword.c_str());
  } else {
    WiFi.softAP(apSSID.c_str());
  }

  transitionToState(WIFI_STATE_AP_MODE);
}

void ConfigManagerWiFi::beginWiFiConnection_() {
  if (!useDHCP) {
    applyStaticConfig();
  }

  // BSSID selection if configured
  String targetBSSID = findBestBSSID();
  if (!targetBSSID.isEmpty()) {
    WIFI_LOG("Using specific BSSID: %s", targetBSSID.c_str());
    uint8_t bssid[6];
    unsigned int tmp[6];
    int matched = sscanf(targetBSSID.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x",
                         &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]);
    if (matched == 6) {
      for (int i = 0; i < 6; ++i) bssid[i] = static_cast<uint8_t>(tmp[i] & 0xFF);
      WiFi.begin(ssid.c_str(), password.c_str(), 0, bssid);
    } else {
      WIFI_LOG("Invalid BSSID format '%s', falling back to auto BSSID", targetBSSID.c_str());
      WiFi.begin(ssid.c_str(), password.c_str());
    }
  } else {
    WiFi.begin(ssid.c_str(), password.c_str());
  }

  // Advance state and counters
  transitionToState(WIFI_STATE_CONNECTING);
  lastReconnectAttempt = millis();
  if (connectAttempts < 250) {
    connectAttempts++;
  }
}

void ConfigManagerWiFi::startStackReset_() {
  if (stackResetInProgress) {
    return;
  }

  WIFI_LOG("Performing complete WiFi stack reset for connectivity fix...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  stackResetInProgress = true;
  stackResetStep = 1;
  stackResetStepAtMs = millis() + 500UL;
}

bool ConfigManagerWiFi::advanceStackReset_() {
  if (!stackResetInProgress) {
    return false;
  }

  const unsigned long now = millis();
  if (static_cast<long>(now - stackResetStepAtMs) < 0) {
    return false;
  }

  switch (stackResetStep) {
    case 1:
      esp_wifi_stop();
      esp_wifi_deinit();
      stackResetStep = 2;
      stackResetStepAtMs = now + 200UL;
      return false;
    case 2: {
      wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
      esp_wifi_init(&cfg);
      stackResetStep = 3;
      stackResetStepAtMs = now + 200UL;
      return false;
    }
    case 3:
      WiFi.mode(WIFI_STA);
      stackResetStep = 4;
      stackResetStepAtMs = now + 100UL;
      return false;
    case 4:
      // Set additional WiFi parameters for stability
      WiFi.setSleep(false);       // Disable WiFi sleep
      WiFi.setAutoReconnect(true);  // Enable auto-reconnect
      WiFi.persistent(true);      // Store WiFi configuration in flash
      WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Set specific power level
      stackResetInProgress = false;
      stackResetStep = 0;
      stackResetStepAtMs = 0;
      WIFI_LOG("WiFi stack reset complete - WiFi.mode() = %d, WiFi.status() = %d",
               WiFi.getMode(), WiFi.status());
      return true;
    default:
      stackResetInProgress = false;
      stackResetStep = 0;
      stackResetStepAtMs = 0;
      return false;
  }
}

void ConfigManagerWiFi::processPendingRoamingReconnect_() {
  if (!roamingReconnectPending) {
    return;
  }
  if (stackResetInProgress) {
    return;
  }

  const unsigned long now = millis();
  if (static_cast<long>(now - roamingReconnectAtMs) < 0) {
    return;
  }

  roamingReconnectPending = false;
  if (!useDHCP) {
    applyStaticConfig();
  }
  WiFi.begin(ssid.c_str(), password.c_str());
  transitionToState(WIFI_STATE_CONNECTING);
  lastReconnectAttempt = now;
  WIFI_LOG_VERBOSE("Deferred roaming reconnect started");
}

void ConfigManagerWiFi::update() {
  if (!initialized) return;

  const bool resetCompleted = advanceStackReset_();
  if (resetCompleted && connectAfterStackReset) {
    connectAfterStackReset = false;
    beginWiFiConnection_();
  }

  processPendingRoamingReconnect_();

  // Debug: Log current status every few calls
  // static int debugCounter = 0;
  // debugCounter++;
  // if (debugCounter % 50 == 0) { // Every 50 calls (~every 0.5 second for more frequent debugging)
  //   WIFI_LOG("DEBUG Update - Current State: %s, WiFi.status(): %d (%s)", 
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
      WIFI_LOG_VERBOSE("WiFi.status() = WL_CONNECTED, IP: %s, Gateway: %s, DNS: %s",
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
      WIFI_LOG_VERBOSE("Transient status while connected: %d (%s)",
                       wifiStatus, getWiFiStatusString(wifiStatus).c_str());
      lastGoodConnectionMillis = millis();
    } else {
      if (currentState == WIFI_STATE_CONNECTED) {
        WIFI_LOG("Connection lost! WiFi.status() = %d", wifiStatus);
        transitionToState(WIFI_STATE_DISCONNECTED);
      } else if (currentState == WIFI_STATE_CONNECTING) {
        // Still trying to connect, log status periodically
        static unsigned long lastStatusLog = 0;
        if (millis() - lastStatusLog > 5000) { // Log every 5 seconds
          WIFI_LOG_VERBOSE("Still connecting... WiFi.status() = %d (%s)",
                           wifiStatus, getWiFiStatusString(wifiStatus).c_str());
          lastStatusLog = millis();

          if (wifiStatus == WL_NO_SSID_AVAIL) {
            logNoSsidAvailScan_();
          }
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
  WIFI_LOG_VERBOSE("State: %s -> %s", oldStateStr.c_str(), newStateStr.c_str());

  // Execute callbacks based on state transitions
  switch (newState) {
    case WIFI_STATE_CONNECTED:
      if (oldState != WIFI_STATE_CONNECTED) {
        WIFI_LOG_VERBOSE("Connected! IP: %s", WiFi.localIP().toString().c_str());
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
        WIFI_LOG_VERBOSE("Disconnected from %s", ssid.c_str());
        if (onDisconnectedCallback) {
          onDisconnectedCallback();
        }
      }
      break;

    case WIFI_STATE_AP_MODE:
      if (oldState != WIFI_STATE_AP_MODE) {
        WIFI_LOG_VERBOSE("Access Point mode active");
        if (onAPModeCallback) {
          onAPModeCallback();
        }
      }
      break;

    case WIFI_STATE_CONNECTING:
      WIFI_LOG_VERBOSE("Connecting to %s...", ssid.c_str());
      break;

    default:
      break;
  }
}

void ConfigManagerWiFi::handleReconnection() {
  if (WiFi.getMode() == WIFI_AP) return; // Don't reconnect in AP mode
  if (stackResetInProgress || roamingReconnectPending) return;

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
    WIFI_LOG("Auto-reboot triggered after %lu ms without connection", timeSinceLastConnection);
    markRestartCause(kRestartCauseWiFiAutoReboot);
    delay(50); // allow log to flush
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
  WIFI_LOG_VERBOSE("Auto-reboot timeout set to %lu min (%lu ms)",
                   timeoutMinutes,
                   autoRebootTimeoutMs);
}

void ConfigManagerWiFi::setReconnectInterval(unsigned long intervalMs) {
  reconnectInterval = intervalMs;
}

void ConfigManagerWiFi::forceReconnect() {
  lastReconnectAttempt = 0; // Reset timer to trigger immediate reconnect
}
void ConfigManagerWiFi::reconnect() {
  WIFI_LOG("Manual reconnect requested");
  WiFi.disconnect();
  forceReconnect();
}

void ConfigManagerWiFi::disconnect() {
  WIFI_LOG("Manual disconnect requested");
  roamingReconnectPending = false;
  connectAfterStackReset = false;
  WiFi.disconnect();
  transitionToState(WIFI_STATE_DISCONNECTED);
}

void ConfigManagerWiFi::reset() {
  currentState = WIFI_STATE_DISCONNECTED;
  lastGoodConnectionMillis = millis();
  lastReconnectAttempt = 0;
  roamingReconnectPending = false;
  connectAfterStackReset = false;
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
//  - Attempt 3+ (phase >=2): keep retrying and periodically reset the WiFi stack
//    (restart only via auto-reboot timeout to avoid reboot loops on weak networks)
void ConfigManagerWiFi::attemptConnect() {
  if (stackResetInProgress) {
    WIFI_LOG_VERBOSE("Stack reset in progress, deferring connect attempt");
    return;
  }

  roamingReconnectPending = false;
  const uint8_t phase = connectAttempts;
  const unsigned long now = millis();
  const unsigned long timeSinceLastGood = now - lastGoodConnectionMillis;
  const bool autoRebootDue = (autoRebootEnabled && autoRebootTimeoutMs > 0 && timeSinceLastGood >= autoRebootTimeoutMs);

  if (phase == 0) {
    WIFI_LOG_VERBOSE("Attempt 1: normal connect (no stack reset)");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
  } else if (phase == 1) {
    WIFI_LOG("Attempt 2: performing WiFi stack reset, then reconnect");
    connectAfterStackReset = true;
    startStackReset_();
    return;
  } else {
    // Do not restart immediately; gate restarts by the configured auto-reboot timeout.
    // This prevents short reboot loops when an SSID is temporarily unavailable or signal is weak.
    const unsigned long timeoutMin = autoRebootTimeoutMs / 60000UL;
    WIFI_LOG_VERBOSE("Attempt %u: retrying (sinceLastGood=%lu ms, autoReboot=%s, timeout=%lu min)",
                     static_cast<unsigned int>(phase) + 1U,
                     timeSinceLastGood,
                     autoRebootEnabled ? "enabled" : "disabled",
                     timeoutMin);

    if (autoRebootDue) {
      WIFI_LOG("Auto-reboot triggered after %lu ms without connection (attempt=%u)",
               timeSinceLastGood,
               static_cast<unsigned int>(phase) + 1U);
      markRestartCause(kRestartCauseWiFiAutoReboot);
      delay(50); // allow log to flush
      ESP.restart();
      return;
    }

    // Periodically reset the WiFi stack to recover from stuck states without rebooting the MCU.
    // With a 10s reconnect interval, every 6th retry is roughly once per minute.
    const bool periodicReset = ((phase % 6U) == 0U);
    if (periodicReset) {
      WIFI_LOG("Retry attempt %u: performing WiFi stack reset (no reboot)",
               static_cast<unsigned int>(phase) + 1U);
      connectAfterStackReset = true;
      startStackReset_();
      return;
    } else {
      // Ensure WiFi is in a sane STA config before calling begin() again.
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(false);
      WiFi.setAutoReconnect(true);
      WiFi.persistent(true);
    }
  }
  beginWiFiConnection_();
}

// Smart WiFi Roaming implementation
void ConfigManagerWiFi::enableSmartRoaming(bool enable) {
  smartRoamingEnabled = enable;
  WIFI_LOG_VERBOSE("Smart Roaming %s", enable ? "enabled" : "disabled");
}

void ConfigManagerWiFi::setRoamingThreshold(int thresholdDbm) {
  roamingThreshold = thresholdDbm;
  WIFI_LOG_VERBOSE("Roaming threshold set to %d dBm", thresholdDbm);
}

void ConfigManagerWiFi::setRoamingCooldown(unsigned long cooldownSeconds) {
  roamingCooldown = cooldownSeconds * 1000; // Convert to milliseconds
  WIFI_LOG_VERBOSE("Roaming cooldown set to %lu seconds", cooldownSeconds);
}

void ConfigManagerWiFi::setRoamingImprovement(int improvementDbm) {
  roamingImprovement = improvementDbm;
  WIFI_LOG_VERBOSE("Roaming improvement threshold set to %d dBm", improvementDbm);
}

bool ConfigManagerWiFi::isSmartRoamingEnabled() const {
  return smartRoamingEnabled;
}

void ConfigManagerWiFi::checkSmartRoaming() {
  if (!smartRoamingEnabled || ssid.isEmpty()) {
    return;
  }
  if (roamingReconnectPending) {
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

  WIFI_LOG_VERBOSE("Current RSSI (%d dBm) below threshold (%d dBm), scanning for better APs...", 
           currentRSSI, roamingThreshold);

  // Scan for networks
  int networkCount = WiFi.scanNetworks();
  if (networkCount <= 0) {
    WIFI_LOG_VERBOSE("No networks found during roaming scan");
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
      WIFI_LOG_VERBOSE("Found better AP: %s (RSSI: %d dBm, improvement: %d dBm)", 
               bestBSSID.c_str(), bestRSSI, bestRSSI - currentRSSI);
      
      // Disconnect now and reconnect after a short delay without blocking loop().
      WiFi.disconnect();
      roamingReconnectPending = true;
      roamingReconnectAtMs = currentTime + 500UL;
      lastRoamingAttempt = currentTime;
      WIFI_LOG_VERBOSE("Scheduled roaming reconnect in 500 ms");
    }
  } else {
    WIFI_LOG_VERBOSE("No better AP found (current: %d dBm)", currentRSSI);
  }

  // Clean up scan results
  WiFi.scanDelete();
}

// MAC Address Filtering and Priority implementation
void ConfigManagerWiFi::setAccessPointMacFilter(const String& macAddress) {
  filterMac = macAddress;
  macFilterEnabled = true;
  macPriorityEnabled = false; // Filter mode disables priority mode
  WIFI_LOG("MAC Filter enabled for: %s", macAddress.c_str());
}

void ConfigManagerWiFi::setAccessPointMacPriority(const String& macAddress) {
  priorityMac = macAddress;
  macPriorityEnabled = true;
  macFilterEnabled = false; // Priority mode disables filter mode
  WIFI_LOG("MAC Priority enabled for: %s", macAddress.c_str());
}

void ConfigManagerWiFi::clearMacFilter() {
  macFilterEnabled = false;
  filterMac = "";
  WIFI_LOG("MAC Filter disabled");
}

void ConfigManagerWiFi::clearMacPriority() {
  macPriorityEnabled = false;
  priorityMac = "";
  WIFI_LOG("MAC Priority disabled");
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
    WIFI_LOG("No SSID set, skipping BSSID selection");
    return "";
  }

  // If no MAC filtering/priority is enabled, let WiFi auto-connect
  if (!macFilterEnabled && !macPriorityEnabled) {
    WIFI_LOG_VERBOSE("No MAC filter/priority enabled, using auto-connect");
    return "";
  }

  WIFI_LOG_VERBOSE("Scanning for networks to apply MAC filter/priority...");
  
  // Clear any previous scan results first
  WiFi.scanDelete();
  
  int networkCount = WiFi.scanNetworks(false, false, false, 300); // Reduced scan time
  
  if (networkCount <= 0) {
    WIFI_LOG("No networks found during scan (count: %d), falling back to auto-connect", networkCount);
    WiFi.scanDelete(); // Ensure cleanup even on failure
    return "";
  }

  WIFI_LOG_VERBOSE("Found %d networks during scan", networkCount);

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
      
      WIFI_LOG_VERBOSE("Found matching network: SSID=%s, BSSID=%s, RSSI=%d", 
                       networkSSID.c_str(), networkBSSID.c_str(), networkRSSI);

      // MAC Filter mode: only connect to specific MAC
      if (macFilterEnabled) {
        if (networkBSSID.equalsIgnoreCase(filterMac)) {
          if (networkRSSI > bestRSSI) {
            bestBSSID = networkBSSID;
            bestRSSI = networkRSSI;
            WIFI_LOG("Filter match found: %s (RSSI: %d dBm)", networkBSSID.c_str(), networkRSSI);
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
          WIFI_LOG("Found priority AP: %s (RSSI: %d dBm)", networkBSSID.c_str(), networkRSSI);
          break; // Stop searching once priority AP is found
        } else if (!priorityFound) {
          // Fallback option: use best available if no priority found yet
          if (networkRSSI > bestRSSI) {
            bestBSSID = networkBSSID;
            bestRSSI = networkRSSI;
            WIFI_LOG_VERBOSE("Fallback candidate: %s (RSSI: %d dBm)", networkBSSID.c_str(), networkRSSI);
          }
        }
      }
    }
  }

  if (matchingNetworks == 0) {
    constexpr int maxShown = 10;
    String list;
    list.reserve(256);
    int shown = 0;
    for (int i = 0; i < networkCount && shown < maxShown; i++) {
      String s = WiFi.SSID(i);
      if (s.isEmpty()) {
        s = "<hidden>";
      }
      if (shown > 0) {
        list += ", ";
      }
      list += s;
      shown++;
    }

    WIFI_LOG("[WARNING] SSID '%s' not found during MAC scan. Nearby SSIDs: %d networks found, showing %d: %s",
             ssid.c_str(),
             networkCount,
             shown,
             list.c_str());
  }

  // Clean up scan results
  WiFi.scanDelete();

  WIFI_LOG("Scan complete: %d matching networks found", matchingNetworks);

  if (!bestBSSID.isEmpty()) {
    WIFI_LOG_VERBOSE("Selected BSSID: %s (RSSI: %d dBm)", bestBSSID.c_str(), bestRSSI);
    // If priority was configured but not found, make that explicit even when we have a fallback
    if (macPriorityEnabled && !priorityFound) {
      WIFI_LOG("MAC Priority target %s not found; using best available AP %s (RSSI: %d dBm)",
               priorityMac.c_str(), bestBSSID.c_str(), bestRSSI);
    }
  } else if (macFilterEnabled) {
    WIFI_LOG("MAC Filter enabled but target AP %s not found", filterMac.c_str());
  } else if (macPriorityEnabled) {
    WIFI_LOG("MAC Priority enabled but target AP %s not found, will use auto-connect", priorityMac.c_str());
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

void ConfigManagerWiFi::performStackReset() {
  if (!stackResetInProgress) {
    startStackReset_();
  }

  // Compatibility path: keep method synchronous for direct/manual callers.
  const unsigned long timeoutMs = 3000UL;
  const unsigned long startedAt = millis();
  while (stackResetInProgress) {
    advanceStackReset_();
    if (millis() - startedAt > timeoutMs) {
      WIFI_LOG("[WARNING] WiFi stack reset timeout after %lu ms", timeoutMs);
      stackResetInProgress = false;
      stackResetStep = 0;
      stackResetStepAtMs = 0;
      break;
    }
    delay(1);
  }
}

void ConfigManagerWiFi::logNoSsidAvailScan_() {
  const unsigned long now = millis();
  if (ssid.isEmpty()) {
    WIFI_LOG("[WARNING] WL_NO_SSID_AVAIL but SSID is empty");
    return;
  }

  constexpr unsigned long throttleMs = 15000UL; // avoid frequent scan starts/log spam
  constexpr unsigned long maxScanAgeMs = 30000UL;

  const int scanState = WiFi.scanComplete();
  if (scanState == WIFI_SCAN_RUNNING) {
    // Scan is already running (likely started by us).
    if (noSsidScanStartMillis != 0 && (now - noSsidScanStartMillis) > maxScanAgeMs) {
      WIFI_LOG_VERBOSE("SSID '%s' still not found; scan is still running (%lu ms)",
                       ssid.c_str(),
                       now - noSsidScanStartMillis);
    }
    return;
  }

  if (scanState >= 0) {
    const int networkCount = scanState;
    int matches = 0;
    for (int i = 0; i < networkCount; i++) {
      if (WiFi.SSID(i) == ssid) {
        matches++;
      }
    }

    constexpr int maxShown = 10;
    String list;
    list.reserve(256);
    int shown = 0;
    for (int i = 0; i < networkCount && shown < maxShown; i++) {
      String s = WiFi.SSID(i);
      if (s.isEmpty()) {
        s = "<hidden>";
      }
      if (shown > 0) {
        list += ", ";
      }
      list += s;
      shown++;
    }

    WIFI_LOG("[WARNING] Nearby SSIDs: %d networks found, matches for '%s': %d, showing %d: %s",
             networkCount, ssid.c_str(), matches, shown, list.c_str());
    WiFi.scanDelete();
    noSsidScanStartMillis = 0;
    return;
  }

  // scanComplete() errors:
  // - WIFI_SCAN_FAILED (-2): scan failed
  // - WIFI_SCAN_RUNNING (-1): handled above
  if (scanState == WIFI_SCAN_FAILED) {
    WIFI_LOG("[WARNING] SSID '%s' not found; WiFi scan failed (WIFI_SCAN_FAILED)", ssid.c_str());
    WiFi.scanDelete();
    noSsidScanStartMillis = 0;
    // Fall through to possibly start a new scan (throttled).
  }

  if (lastNoSsidScanMillis != 0 && (now - lastNoSsidScanMillis) < throttleMs) {
    return;
  }
  lastNoSsidScanMillis = now;

  WIFI_LOG("[WARNING] SSID '%s' not found (WL_NO_SSID_AVAIL). Starting async scan for nearby networks...",
           ssid.c_str());
  WiFi.scanDelete();
  noSsidScanStartMillis = now;
  (void)WiFi.scanNetworks(true /*async*/, true /*showHidden*/);
}
