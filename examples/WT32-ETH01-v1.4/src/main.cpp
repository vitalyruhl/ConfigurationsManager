#include <Arduino.h>
#include <ETH.h>

#include "ConfigManager.h"
#include "core/CoreSettings.h"

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#endif

#define VERSION CONFIGMANAGER_VERSION

#ifndef APP_NAME
#define APP_NAME "CM-WT32-ETH01"
#endif

#ifndef SETTINGS_PASSWORD
#define SETTINGS_PASSWORD ""
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD "ota"
#endif

#ifndef MY_ETHERNET_IP
#define MY_ETHERNET_IP "192.168.2.127"
#endif

#ifndef MY_GATEWAY_IP
#define MY_GATEWAY_IP "192.168.2.1"
#endif

#ifndef MY_SUBNET_MASK
#define MY_SUBNET_MASK "255.255.255.0"
#endif

#ifndef MY_DNS_IP
#define MY_DNS_IP MY_GATEWAY_IP
#endif

namespace {
volatile bool ethernetHasIp = false;
bool networkServicesStarted = false;

cm::CoreSettings& coreSettings = cm::CoreSettings::instance();
cm::CoreSystemSettings& systemSettings = coreSettings.system;
cm::CoreNtpSettings& ntpSettings = coreSettings.ntp;

Config<String> ethernetIp{ConfigOptions<String>{.key = "EthIP", .name = "IP Address", .category = "Ethernet", .defaultValue = String(""), .showInWeb = true, .sortOrder = 1}};
Config<String> ethernetSubnet{ConfigOptions<String>{.key = "EthSubnet", .name = "Subnet Mask", .category = "Ethernet", .defaultValue = String(""), .showInWeb = true, .sortOrder = 2}};
Config<String> ethernetGateway{ConfigOptions<String>{.key = "EthGateway", .name = "Gateway", .category = "Ethernet", .defaultValue = String(""), .showInWeb = true, .sortOrder = 3}};
Config<String> ethernetDns{ConfigOptions<String>{.key = "EthDNS", .name = "Primary DNS", .category = "Ethernet", .defaultValue = String(""), .showInWeb = true, .sortOrder = 4}};
Config<String> settingsPassword{ConfigOptions<String>{.key = "SettingsPass", .name = "Settings Password", .category = "System", .defaultValue = String(""), .showInWeb = true, .isPassword = true, .sortOrder = 2}};

void registerSettings() {
  ConfigManager.setCategoryLayoutOverride("Ethernet", "Network", "Network", "Ethernet Settings", 10);
  ConfigManager.addSettingsPage("Network", 10);
  ConfigManager.addSettingsGroup("Network", "Network", "Ethernet Settings", 10);

  ConfigManager.addSetting(&ethernetIp);
  ConfigManager.addSetting(&ethernetSubnet);
  ConfigManager.addSetting(&ethernetGateway);
  ConfigManager.addSetting(&ethernetDns);
  ConfigManager.addSetting(&settingsPassword);

  coreSettings.attachSystem(ConfigManager);
  coreSettings.attachNtp(ConfigManager);
}

void setupDefaults() {
  if (ethernetIp.get().isEmpty()) {
    ethernetIp.set(MY_ETHERNET_IP);
    ethernetSubnet.set(MY_SUBNET_MASK);
    ethernetGateway.set(MY_GATEWAY_IP);
    ethernetDns.set(MY_DNS_IP);
    settingsPassword.set(SETTINGS_PASSWORD);
#if CM_ENABLE_OTA
    systemSettings.otaPassword.set(OTA_PASSWORD);
#endif
#ifdef MY_NTP_SERVER_1
    ntpSettings.server1.set(MY_NTP_SERVER_1);
#endif
#ifdef MY_NTP_SERVER_2
    ntpSettings.server2.set(MY_NTP_SERVER_2);
#endif
#ifdef MY_NTP_TIMEZONE
    ntpSettings.tz.set(MY_NTP_TIMEZONE);
#endif
    ConfigManager.saveAll();
  }

  ConfigManager.setSettingsPassword(settingsPassword.get());
  settingsPassword.setCallback([](const String& password) { ConfigManager.setSettingsPassword(password); });
}

bool loadEthernetConfig(IPAddress& localIp, IPAddress& gateway, IPAddress& subnet, IPAddress& dnsPrimary) {
  const bool valid = localIp.fromString(ethernetIp.get()) &&
                     gateway.fromString(ethernetGateway.get()) &&
                     subnet.fromString(ethernetSubnet.get()) &&
                     dnsPrimary.fromString(ethernetDns.get());
  if (!valid) {
    Serial.println("[E] Invalid Ethernet network settings");
  }
  return valid;
}

void setupNtp() {
  configTzTime(ntpSettings.tz.get().c_str(),
               ntpSettings.server1.get().c_str(),
               ntpSettings.server2.get().c_str());
}

void onNetworkEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname(APP_NAME);
      Serial.println("[I] Ethernet started");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("[I] Ethernet link connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      ethernetHasIp = true;
      Serial.printf("[I] Ethernet ready: http://%s\n", ETH.localIP().toString().c_str());
      break;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    case ARDUINO_EVENT_ETH_LOST_IP:
      ethernetHasIp = false;
      Serial.println("[W] Ethernet lost IP");
      break;
#endif
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      ethernetHasIp = false;
      Serial.println("[W] Ethernet disconnected");
      break;
    case ARDUINO_EVENT_ETH_STOP:
      ethernetHasIp = false;
      Serial.println("[W] Ethernet stopped");
      break;
    default:
      break;
  }
}
} // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  ConfigManagerClass::setLogger([](const char* message) {
    Serial.print("[CM] ");
    Serial.println(message);
  });

  ConfigManager.setAppName(APP_NAME);
  ConfigManager.setAppTitle(APP_NAME);
  ConfigManager.setVersion(VERSION);
  ConfigManager.enableBuiltinSystemProvider();
  registerSettings();
  ConfigManager.loadAll();
  setupDefaults();

  WiFi.onEvent(onNetworkEvent);

  if (!ETH.begin()) {
    Serial.println("[E] Ethernet PHY init failed");
    return;
  }

  IPAddress localIp;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dnsPrimary;
  if (!loadEthernetConfig(localIp, gateway, subnet, dnsPrimary)) {
    return;
  }

  if (!ETH.config(localIp, gateway, subnet, dnsPrimary)) {
    Serial.println("[E] Ethernet static IP config failed");
  }
}

void loop() {
  if (ethernetHasIp && !networkServicesStarted) {
    ConfigManager.startWebServerOnNetwork();
#if CM_ENABLE_OTA
    ConfigManager.setupOTA(APP_NAME, systemSettings.otaPassword.get());
#endif
    setupNtp();
    networkServicesStarted = true;
  }

  ConfigManager.handleClient();
  delay(1);
}
