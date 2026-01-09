#include <Arduino.h>

#include <BME280_I2C.h>
#include <Ticker.h>
#include <WiFi.h>

#include "ConfigManager.h"

#include "secret/wifiSecret.h"

#ifndef BME280_ADDRESS
#define BME280_ADDRESS 0x76
#endif

#define VERSION CONFIGMANAGER_VERSION
#define APP_NAME "CM-BME280-Demo"

// I2C pins for the BME280 sensor
#define I2C_SDA 21
#define I2C_SCL 22

static float computeDewPoint(float temperatureC, float relHumidityPct);
static inline ConfigManagerRuntime &CRM() { return ConfigManager.getRuntime(); }

extern ConfigManagerClass ConfigManager;

static BME280_I2C bme280;
static Ticker temperatureTicker;

static float temperature = 0.0f;
static float dewPoint = 0.0f;
static float humidity = 0.0f;
static float pressure = 0.0f;

struct SystemSettings
{
    Config<bool> allowOTA;
    Config<String> otaPassword;
    Config<String> version;

    SystemSettings()
        : allowOTA(ConfigOptions<bool>{
              .key = "OTAEn",
              .name = "Allow OTA Updates",
              .category = "System",
              .defaultValue = true,
              .callback = [](bool newValue) {
                  Serial.printf("[MAIN] OTA setting changed to: %s\n", newValue ? "enabled" : "disabled");
                  ConfigManager.getOTAManager().enable(newValue);
              },
          }),
          otaPassword(ConfigOptions<String>{
              .key = "OTAPass",
              .name = "OTA Password",
              .category = "System",
              .defaultValue = String(OTA_PASSWORD),
              .showInWeb = true,
              .isPassword = true,
          }),
          version(ConfigOptions<String>{
              .key = "P_Version",
              .name = "Program Version",
              .category = "System",
              .defaultValue = String(VERSION),
          })
    {
    }

    void init()
    {
        ConfigManager.addSetting(&allowOTA);
        ConfigManager.addSetting(&otaPassword);
        ConfigManager.addSetting(&version);
    }
};

static SystemSettings systemSettings;

struct WiFiSettings
{
    Config<String> wifiSsid;
    Config<String> wifiPassword;
    Config<bool> useDhcp;
    Config<String> staticIp;
    Config<String> gateway;
    Config<String> subnet;
    Config<String> dnsPrimary;
    Config<String> dnsSecondary;

    WiFiSettings()
        : wifiSsid(ConfigOptions<String>{.key = "WiFiSSID", .name = "WiFi SSID", .category = "WiFi", .defaultValue = "", .showInWeb = true, .sortOrder = 1}),
          wifiPassword(ConfigOptions<String>{.key = "WiFiPassword", .name = "WiFi Password", .category = "WiFi", .defaultValue = "secretpass", .showInWeb = true, .isPassword = true, .sortOrder = 2}),
          useDhcp(ConfigOptions<bool>{.key = "WiFiUseDHCP", .name = "Use DHCP", .category = "WiFi", .defaultValue = true, .showInWeb = true, .sortOrder = 3}),
          staticIp(ConfigOptions<String>{.key = "WiFiStaticIP", .name = "Static IP", .category = "WiFi", .defaultValue = "192.168.0.100", .sortOrder = 4, .showIf = [this]() { return !useDhcp.get(); }}),
          gateway(ConfigOptions<String>{.key = "WiFiGateway", .name = "Gateway", .category = "WiFi", .defaultValue = "192.168.0.1", .sortOrder = 5, .showIf = [this]() { return !useDhcp.get(); }}),
          subnet(ConfigOptions<String>{.key = "WiFiSubnet", .name = "Subnet Mask", .category = "WiFi", .defaultValue = "255.255.255.0", .sortOrder = 6, .showIf = [this]() { return !useDhcp.get(); }}),
          dnsPrimary(ConfigOptions<String>{.key = "WiFiDNS1", .name = "Primary DNS", .category = "WiFi", .defaultValue = "192.168.0.1", .sortOrder = 7, .showIf = [this]() { return !useDhcp.get(); }}),
          dnsSecondary(ConfigOptions<String>{.key = "WiFiDNS2", .name = "Secondary DNS", .category = "WiFi", .defaultValue = "8.8.8.8", .sortOrder = 8, .showIf = [this]() { return !useDhcp.get(); }})
    {
    }

    void init()
    {
        ConfigManager.addSetting(&wifiSsid);
        ConfigManager.addSetting(&wifiPassword);
        ConfigManager.addSetting(&useDhcp);
        ConfigManager.addSetting(&staticIp);
        ConfigManager.addSetting(&gateway);
        ConfigManager.addSetting(&subnet);
        ConfigManager.addSetting(&dnsPrimary);
        ConfigManager.addSetting(&dnsSecondary);
    }
};

static WiFiSettings wifiSettings;

struct TempSettings
{
    Config<float> tempCorrection;
    Config<float> humidityCorrection;
    Config<int> seaLevelPressure;
    Config<int> readIntervalSec;

    TempSettings()
        : tempCorrection(ConfigOptions<float>{.key = "TCO", .name = "Temperature Correction", .category = "Temp", .defaultValue = 0.0f}),
          humidityCorrection(ConfigOptions<float>{.key = "HYO", .name = "Humidity Correction", .category = "Temp", .defaultValue = 0.0f}),
          seaLevelPressure(ConfigOptions<int>{.key = "SLP", .name = "Sea Level Pressure", .category = "Temp", .defaultValue = 1013}),
          readIntervalSec(ConfigOptions<int>{.key = "ReadTemp", .name = "Read Temp/Humidity every (s)", .category = "Temp", .defaultValue = 30})
    {
    }

    void init()
    {
        ConfigManager.addSetting(&tempCorrection);
        ConfigManager.addSetting(&humidityCorrection);
        ConfigManager.addSetting(&seaLevelPressure);
        ConfigManager.addSetting(&readIntervalSec);
    }
};

static TempSettings tempSettings;

static void setupRuntimeUI()
{
    CRM().addRuntimeProvider("sensors", [](JsonObject &data) {
        data["temp"] = temperature;
        data["hum"] = humidity;
        data["dew"] = dewPoint;
        data["pressure"] = pressure;
    });

    RuntimeFieldMeta tempMeta;
    tempMeta.group = "sensors";
    tempMeta.key = "temp";
    tempMeta.label = "Temperature";
    tempMeta.unit = "C";
    tempMeta.precision = 1;
    tempMeta.order = 10;
    CRM().addRuntimeMeta(tempMeta);

    RuntimeFieldMeta humMeta;
    humMeta.group = "sensors";
    humMeta.key = "hum";
    humMeta.label = "Humidity";
    humMeta.unit = "%";
    humMeta.precision = 1;
    humMeta.order = 11;
    CRM().addRuntimeMeta(humMeta);

    RuntimeFieldMeta dewMeta;
    dewMeta.group = "sensors";
    dewMeta.key = "dew";
    dewMeta.label = "Dewpoint";
    dewMeta.unit = "C";
    dewMeta.precision = 1;
    dewMeta.order = 12;
    CRM().addRuntimeMeta(dewMeta);

    RuntimeFieldMeta pressureMeta;
    pressureMeta.group = "sensors";
    pressureMeta.key = "pressure";
    pressureMeta.label = "Pressure";
    pressureMeta.unit = "hPa";
    pressureMeta.precision = 1;
    pressureMeta.order = 13;
    CRM().addRuntimeMeta(pressureMeta);
}

static bool startWebServer()
{
    Serial.println("[MAIN] Starting web server...");

    if (WiFi.getMode() == WIFI_AP)
    {
        return false;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        if (wifiSettings.useDhcp.get())
        {
            Serial.println("[MAIN] DHCP enabled");
            ConfigManager.startWebServer(wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());
        }
        else
        {
            Serial.println("[MAIN] DHCP disabled - using static IP");
            IPAddress staticIP, gateway, subnet, dns1, dns2;
            staticIP.fromString(wifiSettings.staticIp.get());
            gateway.fromString(wifiSettings.gateway.get());
            subnet.fromString(wifiSettings.subnet.get());

            const String dnsPrimaryStr = wifiSettings.dnsPrimary.get();
            const String dnsSecondaryStr = wifiSettings.dnsSecondary.get();
            if (!dnsPrimaryStr.isEmpty())
            {
                dns1.fromString(dnsPrimaryStr);
            }
            if (!dnsSecondaryStr.isEmpty())
            {
                dns2.fromString(dnsSecondaryStr);
            }

            ConfigManager.startWebServer(staticIP, gateway, subnet, wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get(), dns1, dns2);
        }
    }

    return true;
}

static void readBme280()
{
    bme280.setSeaLevelPressure(tempSettings.seaLevelPressure.get());
    bme280.read();

    temperature = bme280.data.temperature + tempSettings.tempCorrection.get();
    humidity = bme280.data.humidity + tempSettings.humidityCorrection.get();
    pressure = bme280.data.pressure;
    dewPoint = computeDewPoint(temperature, humidity);
}

static void setupTemperatureMeasuring()
{
    Serial.println("[TEMP] Initializing BME280 sensor...");

    bme280.setAddress(BME280_ADDRESS, I2C_SDA, I2C_SCL);

    Serial.println("[TEMP] Starting BME280.begin()...");
    const bool ok = bme280.begin(
        bme280.BME280_STANDBY_0_5,
        bme280.BME280_FILTER_OFF,
        bme280.BME280_SPI3_DISABLE,
        bme280.BME280_OVERSAMPLING_1,
        bme280.BME280_OVERSAMPLING_1,
        bme280.BME280_OVERSAMPLING_1,
        bme280.BME280_MODE_NORMAL);

    if (!ok)
    {
        Serial.println("[TEMP] BME280 not initialized - continuing without temperature sensor");
        return;
    }

    Serial.println("[TEMP] BME280 ready! Starting temperature ticker...");
    int interval = tempSettings.readIntervalSec.get();
    if (interval < 2)
    {
        interval = 2;
    }
    temperatureTicker.attach(static_cast<float>(interval), readBme280);
    readBme280();
}

static float computeDewPoint(float temperatureC, float relHumidityPct)
{
    if (isnan(temperatureC) || isnan(relHumidityPct))
    {
        return NAN;
    }
    if (relHumidityPct <= 0.0f)
    {
        relHumidityPct = 0.1f;
    }
    if (relHumidityPct > 100.0f)
    {
        relHumidityPct = 100.0f;
    }

    const float a = 17.62f;
    const float b = 243.12f;
    const float rh = relHumidityPct / 100.0f;
    const float gamma = (a * temperatureC) / (b + temperatureC) + log(rh);
    const float dew = (b * gamma) / (a - gamma);
    return dew;
}

void setup()
{
    Serial.begin(115200);

    ConfigManagerClass::setLogger([](const char *msg) {
        Serial.print("[ConfigManager] ");
        Serial.println(msg);
    });

    ConfigManager.setAppName(APP_NAME);
    ConfigManager.setVersion(VERSION);

    systemSettings.init();
    wifiSettings.init();
    tempSettings.init();

    ConfigManager.loadAll();

    if (wifiSettings.wifiSsid.get().isEmpty())
    {
        Serial.println("-------------------------------------------------------------");
        Serial.println("[SETUP] SSID is empty, setting default values");
        Serial.println("-------------------------------------------------------------");

        wifiSettings.wifiSsid.set(MY_WIFI_SSID);
        wifiSettings.wifiPassword.set(MY_WIFI_PASSWORD);
        wifiSettings.staticIp.set(MY_WIFI_IP);
        wifiSettings.useDhcp.set(false);
        ConfigManager.saveAll();
        delay(1000);
    }

    startWebServer();
    setupRuntimeUI();

    ConfigManager.enableWebSocketPush();
    ConfigManager.setWebSocketInterval(1000);
    ConfigManager.setPushOnConnect(true);

    setupTemperatureMeasuring();

    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
    {
        Serial.printf("[MAIN] Web server running at: %s (AP Mode)\n", WiFi.softAPIP().toString().c_str());
    }
    else if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("[MAIN] Web server running at: %s (Station Mode)\n", WiFi.localIP().toString().c_str());
    }
    else
    {
        Serial.println("[MAIN] Web server running (IP not available)");
    }

    Serial.println("[MAIN] Setup completed successfully. Starting main loop...");
}

void loop()
{
    ConfigManager.updateLoopTiming();
    ConfigManager.getWiFiManager().update();
    ConfigManager.handleClient();
    ConfigManager.handleWebsocketPush();
    ConfigManager.handleOTA();
    ConfigManager.handleRuntimeAlarms();

    static unsigned long lastLoopLog = 0;
    if (millis() - lastLoopLog > 60000)
    {
        lastLoopLog = millis();
        Serial.printf("[MAIN] Loop running, WiFi status: %d, heap: %d\n", WiFi.status(), ESP.getFreeHeap());
    }
}
