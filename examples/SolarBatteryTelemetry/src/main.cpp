#include <Arduino.h>
#include <ConfigManager.h>
#include "runtime/AppRuntimeMetadata.h"
#include <bluetooth/BluetoothManager.h>
#include <core/CoreSettings.h>
#include <core/CoreWiFiServices.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include "diagnostics/HeapFailureDiagnostics.h"
#include "diagnostics/PanicSummaryCapture.h"
#include <WiFi.h>
#include "settings.h"
#include "smoke/SmokePolicy.h"
#include "smoke/IsolatedTestProfile.h"
#include "smoke/PanicOnceProbe.h"
#include "telemetry/MqttService.h"
#include "victron/VictronReadout.h"
#include "bms/BmsReadout.h"
#include "helpers/HelperModule.h"
#include "logging/LoggingManager.h"

static cm::BluetoothManager bluetooth;
static telemetry::MqttService telemetryMqtt;
static victron::Readout victronReadout(VICTRON_READOUT_MAC,VICTRON_READOUT_KEY);
static bms::Readout bmsReadout(BMS_READOUT_MAC);
static auto& core=cm::CoreSettings::instance();
static auto& wifiSettings=core.wifi;
static auto& systemSettings=core.system;
static cm::CoreWiFiServices wifiServices;
static bool appReady=false;
#if SBT_TEST_PANIC_ONCE
static bool panicProbeSetupComplete=false;
#endif
static bool bleWorkerReady=false; // Written before web-server startup, then immutable.
static void updateStatusLED();
static void setupLogging();
#if CM_ENABLE_VERBOSE_LOGGING || SBT_ENABLE_HEAP_DIAGNOSTICS
static void printHealth(const char* stage);
#endif
static void logNetworkIpInfo(const char *context);
static void applyAccessPointMacPriority();
static bool isValidMacAddress(const String &value);

static bool applyIsolatedTestSettings() {
    if constexpr(!sbt::testprofile::isolated) return true;
    for(const auto& key : {std::pair<const char*,const char*>{"BMS","BmsReadMac"},
                          {"Victron","VictronReadMac"},{"Victron","VictronReadKey"}}) {
        auto* setting=ConfigManager.findSettingByKeyHint(key.first,key.second);
        if(!setting||setting->getType()!=SettingType::STRING)return false;
        static_cast<Config<String>*>(setting)->set("");
    }
    auto* gate=ConfigManager.findSettingByKeyHint("BMS","BmsControl");
    if(!gate||gate->getType()!=SettingType::BOOL)return false;
    static_cast<Config<bool>*>(gate)->set(false);
    bluetooth.settings().enabled.set(true);
    return true;
}

// Built-in LED pulse helper for WiFi status patterns.
static cm::helpers::PulseOutput buildinLED(LED_BUILTIN, cm::helpers::PulseOutput::ActiveLevel::ActiveHigh);

static cm::LoggingManager &lmg = cm::LoggingManager::instance();
using LL = cm::LoggingManager::Level;

static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;

struct WiFiRoamingSettings
{
    Config<String> preferredApMac{ConfigOptions<String>{.key = "WiFiMacPrio", .name = "Preferred AP MAC", .category = "WiFi", .defaultValue = "", .sortOrder = 90}};

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&preferredApMac);
    }
};
WiFiRoamingSettings wifiRoamingSettings;

static bool attachSystemSettings() {
    // Reuse CM's original object. Provision only a genuinely missing NVS value.
    Preferences prefs;
    if (!prefs.begin("ConfigManager", false)) return false;
    const bool initialized=sbt::initializeSettingIfMissing(prefs, systemSettings.otaPassword, String(OTA_PASSWORD));
    prefs.end();
    if (!initialized) return false;
    ConfigManager.setCategoryLayoutOverride(cm::CoreCategories::System,cm::CoreCategories::System,
        cm::CoreCategories::System,"System Settings",20);
    ConfigManager.addSettingsPage(cm::CoreCategories::System,20);
    ConfigManager.addSettingsGroup(cm::CoreCategories::System,cm::CoreCategories::System,"System Settings",20);
    return ConfigManager.addSetting(&systemSettings.allowOTA) &&
        ConfigManager.addSetting(&systemSettings.otaPassword);
}

static void setupNetworkDefaults() {
#if SMOKE_LOCAL_SECRETS
    if(smoke::applyNetworkDefaults(true,wifiSettings.wifiSsid.get().length(),wifiSettings.wifiPassword.get().length(),
                                  String(MY_WIFI_SSID).length(),String(MY_WIFI_PASSWORD).length())) {
        wifiSettings.wifiSsid.set(MY_WIFI_SSID);
        wifiSettings.wifiPassword.set(MY_WIFI_PASSWORD);
        wifiSettings.staticIp.set(MY_WIFI_IP);
        wifiSettings.useDhcp.set(MY_USE_DHCP);
        wifiSettings.gateway.set(MY_GATEWAY_IP);
        wifiSettings.subnet.set(MY_SUBNET_MASK);
        wifiSettings.dnsPrimary.set(MY_DNS_IP);
        ConfigManager.saveAll();
    }
#endif
}

static bool attachSmokeRuntime() {
    ConfigManager.addLivePage("Connectivity",40);
    ConfigManager.addLiveCard("Connectivity","Connectivity",40);
    ConfigManager.addLiveGroup("Connectivity","Connectivity","Bluetooth",40);
    ConfigManager.getRuntime().addRuntimeProvider("smoke",[](JsonObject& data) {
        data["scope"]="WiFi + Victron and BMS readout";
        data["workerReady"]=bleWorkerReady;
        data["resetReason"]=static_cast<int>(esp_reset_reason());
    },5);
    return sbt::metadata::registerSmokeFields(ConfigManager.getRuntime());
}

void setup() {
    telemetryMqtt.capturePanicSummary(sbt::diagnostics::captureStoredPanicOnce());
    setupLogging();
    SBT_INSTALL_HEAP_FAILURE_DIAGNOSTICS();
    ConfigManager.setAppName(sbt::testprofile::isolated?"SolarBatteryTelemetry-Test":APP_NAME);
    ConfigManager.setAppTitle(sbt::testprofile::isolated?"SolarBatteryTelemetry-Test":APP_NAME);
    ConfigManager.setVersion(APP_VERSION);
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
    // Keep the live dashboard focused on collector sources; serial health remains available.
    core.attachWiFi(ConfigManager,"Wi-Fi","Wi-Fi Settings",10);
    if(!attachSystemSettings()){lmg.logTag(LL::Error, "SETUP", "System settings initialization failed");return;}
    core.attachNtp(ConfigManager);
    bluetooth.attach(ConfigManager);
    if(!attachSmokeRuntime()){lmg.logTag(LL::Error, "SETUP", "Runtime fields unavailable");return;}
    if(!victronReadout.attach(ConfigManager,bluetooth)){lmg.logTag(LL::Error, "SETUP", "Victron settings initialization failed");return;}
    if(!telemetryMqtt.attach(ConfigManager,MY_MQTT_BROKER_IP,MY_MQTT_BROKER_PORT,MY_MQTT_USERNAME,MY_MQTT_PASSWORD,SMOKE_LOCAL_SECRETS)){
        lmg.logTag(LL::Error, "SETUP", "MQTT settings initialization failed");return;
    }
    if(!bmsReadout.attach(ConfigManager,bluetooth)){lmg.logTag(LL::Error, "SETUP", "BMS settings initialization failed");return;}
    telemetryMqtt.bindBms(bmsReadout);
    bluetooth.setEventHandler([](const cm::BleEvent& event){victronReadout.onEvent(event);bmsReadout.onEvent(event);});
    // CM polls this from its main loop; only the BLE owner may deinitialize the radio.
    ConfigManager.getOTAManager().setRebootPreparation([]() {
        bluetooth.requestShutdownForRestart();
        return bluetooth.isShutdownForRestartComplete();
    });
    ConfigManager.getOTAManager().setLifecycleObserver([](const OtaLifecycleEvent& event) {
        telemetryMqtt.pauseForOta(event.stage==OtaStage::Starting||event.stage==OtaStage::Succeeded);
        if(event.stage==OtaStage::Starting || event.stage==OtaStage::Succeeded)
            bluetooth.requestPause(cm::BlePauseReason::Ota);
        else
            bluetooth.requestResume(cm::BlePauseReason::Ota);
    });
    ConfigManager.loadAll();
    if(!applyIsolatedTestSettings()){lmg.logTag(LL::Error,"SETUP","Test isolation failed");return;}
    telemetryMqtt.afterLoad();
    victronReadout.afterLoad();
    bmsReadout.afterLoad();
#if SBT_CONTROLLER_SCAN_TEST
    // [MOCKED!] Passive filtering only: no real peer or GATT connection is configured.
    cm::BleAddress scanFixture{};
    std::memcpy(scanFixture.text,"02:00:00:00:00:01",sizeof(scanFixture.text));
    if(!bluetooth.configureAdvertisementPeer(scanFixture)){
        Serial.println("[E] [MOCKED!] Controller scan fixture rejected");return;
    }
    Serial.println("[I] [MOCKED!] Controller scan fixture enabled; no GATT peer");
#endif
    Preferences readoutPrefs;
    if(!readoutPrefs.begin("ConfigManager",false)){lmg.logTag(LL::Error, "SETUP", "Readout initialization failed");return;}
    const bool readoutReady=sbt::enableReadoutBluetoothOnce(readoutPrefs,bluetooth.settings().enabled,victronReadout.configured()||bmsReadout.configured());
    readoutPrefs.end();
    if(!readoutReady){lmg.logTag(LL::Error, "SETUP", "Readout initialization failed");return;}
    setupNetworkDefaults();
    if constexpr(sbt::testprofile::isolated){
        wifiSettings.useDhcp.set(true);
        Serial.println("[I] [MOCKED!] SBTTest/COM8; peers empty; BMS control blocked; DHCP");
    }

    // SmartSolar uses passive advertisements; JBD uses serialized telemetry and explicitly enabled MOS transactions.
    // Do not initialize NimBLE separately; the CM worker owns its first radio operation.
    bleWorkerReady=bluetooth.begin();
    if(!bleWorkerReady)lmg.logTag(LL::Error, "SETUP", "BLE worker unavailable; WiFi/UI/OTA remain available");
    ConfigManager.startWebServer();
    applyAccessPointMacPriority();
    lmg.logTag(LL::Info, "SETUP", "Completed successfully. Starting main loop...");
#if SBT_TEST_PANIC_ONCE
    panicProbeSetupComplete=true;
#endif
}

void loop() {
    // if(!appReady){delay(100);return;}
    ConfigManager.getWiFiManager().update();
    ConfigManager.handleClient();

    const uint32_t now=millis();
    victronReadout.loop(now);
    bmsReadout.loop(now);
    bluetooth.loop(now);
#if SBT_CONTROLLER_SCAN_TEST
    // [MOCKED!] Read host status only; CM's worker remains the sole radio owner.
    static uint32_t scanSampleAt=0,scanReportAt=0,scanSamples=0,scanActive=0,scanStarts=0;
    static bool previousScan=false;
    if(uint32_t(now-scanSampleAt)>=100){
        scanSampleAt=now;
        const bool active=NimBLEDevice::isInitialized()&&ble_gap_disc_active();
        ++scanSamples;if(active)++scanActive;if(active&&!previousScan)++scanStarts;
        previousScan=active;
    }
    if(uint32_t(now-scanReportAt)>=30000){
        scanReportAt=now;
        Serial.printf("[I] [MOCKED!] BT_SCAN samples=%lu active=%lu starts=%lu\n",
            static_cast<unsigned long>(scanSamples),static_cast<unsigned long>(scanActive),
            static_cast<unsigned long>(scanStarts));
    }
#endif

    // BLE dispatch may stamp a newer sample than the loop timestamp above.
    // Copy each snapshot before capturing its health timestamp, including rollover.
    const auto sample=victronReadout.snapshot();
    telemetryMqtt.updateVictron(sample.values,sample.available,sample.reason,sample.receivedMs,millis());

    const auto battery=bmsReadout.snapshot();
    telemetryMqtt.updateBms(battery,bmsReadout.reason(),millis());
    telemetryMqtt.loop(millis());

    updateStatusLED();
    cm::helpers::PulseOutput::loopAll();
    lmg.loop();


#if CM_ENABLE_VERBOSE_LOGGING || SBT_ENABLE_HEAP_DIAGNOSTICS
    static uint32_t lastHealthPrint = 0;
    if (uint32_t(now - lastHealthPrint) >= 1000) {
        lastHealthPrint = now;
        printHealth("loop-alive");
    }
#endif


#if SBT_TEST_PANIC_ONCE
    sbt::testprofile::tickPanicOnceProbe(now,panicProbeSetupComplete&&bleWorkerReady&&telemetryMqtt.connected());
#endif
    delay(1);
}

void onWiFiConnected()
{
    wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
    logNetworkIpInfo("onWiFiConnected");
    lmg.logTag(LL::Debug, "WiFi", "Station Mode: http://%s", WiFi.localIP().toString().c_str());
}

void onWiFiDisconnected()
{
    wifiServices.onDisconnected();
    lmg.logTag(LL::Warn, "WiFi", "Disconnected");
}

void onWiFiAPMode()
{
    wifiServices.onAPMode();
    logNetworkIpInfo("onWiFiAPMode");
    lmg.logTag(LL::Debug, "WiFi", "AP Mode: http://%s", WiFi.softAPIP().toString().c_str());
}


static void updateStatusLED()
{
    static int lastMode = -1; // 1=AP, 2=CONNECTED, 3=CONNECTING

    const bool connected = ConfigManager.getWiFiManager().isConnected();
    const bool apMode = ConfigManager.getWiFiManager().isInAPMode();

    const int mode = apMode ? 1 : (connected ? 2 : 3);
    if (mode == lastMode)
    {
        return;
    }
    lastMode = mode;

    switch (mode)
    {
    case 1: // AP mode: fast blink
        buildinLED.setPulseRepeat(/*count*/ 1, /*periodMs*/ 200, /*gapMs*/ 0);
        break;
    case 2: // Connected: heartbeat
        buildinLED.setPulseRepeat(/*count*/ 1, /*periodMs*/ 100, /*gapMs*/ 1500);
        break;
    case 3: // Connecting/disconnected: double blink
        buildinLED.setPulseRepeat(/*count*/ 3, /*periodMs*/ 200, /*gapMs*/ 600);
        break;
    }

}


static void setupLogging()
{
    Serial.begin(115200);

    auto serialOut = std::make_unique<cm::LoggingManager::SerialOutput>(Serial);
    // serialOut->setLevel(LL::Info);
    serialOut->setLevel(LL::Debug);
    lmg.addOutput(std::move(serialOut));

    // Keep normal UART diagnostics without debug or trace output.
    lmg.setGlobalLevel(LL::Debug);
    lmg.attachToConfigManager(LL::Info, LL::Debug, "");

#if !CM_DISABLE_GUI_LOGGING
    auto guiOut = std::make_unique<cm::LoggingManager::GuiOutput>(ConfigManager, 20);
    guiOut->setLevel(LL::Error);
    lmg.addOutput(std::move(guiOut));
#endif
}

#if CM_ENABLE_VERBOSE_LOGGING || SBT_ENABLE_HEAP_DIAGNOSTICS
// Keep recovery diagnostics independent of HTTP/JSON and omit all credentials.
static void printHealth(const char* stage) {
    multi_heap_info_t info{};
    heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
    lmg.logTag(LL::Debug, "Health", "%s ms=%lu heap=%u min=%u largest=%u internal=%u wifi=%d rssi=%d\n",
        stage, static_cast<unsigned long>(millis()), static_cast<unsigned>(info.total_free_bytes),
        static_cast<unsigned>(info.minimum_free_bytes), static_cast<unsigned>(info.largest_free_block),
        static_cast<unsigned>(ESP.getFreeHeap()), static_cast<int>(WiFi.status()),
        WiFi.isConnected() ? WiFi.RSSI() : 0);
}
#endif

static void logNetworkIpInfo(const char *context)
{
    const WiFiMode_t mode = WiFi.getMode();
    const bool apActive = (mode == WIFI_AP) || (mode == WIFI_AP_STA);
    const bool staConnected = (WiFi.status() == WL_CONNECTED);

    if (apActive)
    {
        const IPAddress apIp = WiFi.softAPIP();
        lmg.logTag(LL::Debug, "WiFi", "%s: AP IP: %s", context, apIp.toString().c_str());
        lmg.logTag(LL::Debug, "WiFi", "%s: AP SSID: %s", context, WiFi.softAPSSID().c_str());
    }

    if (staConnected)
    {
        const IPAddress staIp = WiFi.localIP();
        lmg.logTag(LL::Debug, "WiFi", "%s: STA IP: %s", context, staIp.toString().c_str());
    }
}

static void applyAccessPointMacPriority()
{
    String preferredMac = wifiRoamingSettings.preferredApMac.get();
    preferredMac.trim();

#if defined(WIFI_FILTER_MAC_PRIORITY)
    if (preferredMac.isEmpty())
    {
        preferredMac = WIFI_FILTER_MAC_PRIORITY;
    }
#endif

    if (preferredMac.isEmpty())
    {
        lmg.logTag(LL::Debug, "WiFi", "AP MAC priority not configured");
        return;
    }

    if (!isValidMacAddress(preferredMac))
    {
        lmg.logTag(LL::Warn, "WiFi", "AP MAC priority invalid: %s", preferredMac.c_str());
        return;
    }

    ConfigManager.setAccessPointMacPriority(preferredMac.c_str());
    lmg.logTag(LL::Info, "WiFi", "AP MAC priority set: %s", preferredMac.c_str());
}

static bool isValidMacAddress(const String &value)
{
    if (value.length() != 17)
    {
        return false;
    }

    for (int i = 0; i < value.length(); ++i)
    {
        const char c = value.charAt(i);
        if ((i + 1) % 3 == 0)
        {
            if (c != ':')
            {
                return false;
            }
            continue;
        }

        const bool isDigit = (c >= '0' && c <= '9');
        const bool isUpperHex = (c >= 'A' && c <= 'F');
        const bool isLowerHex = (c >= 'a' && c <= 'f');
        if (!isDigit && !isUpperHex && !isLowerHex)
        {
            return false;
        }
    }

    return true;
}
