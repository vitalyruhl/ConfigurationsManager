#include <Arduino.h>
#include "ConfigManager.h"
#include <Ticker.h> // for read temperature periodically
#include <BME280_I2C.h> // Include BME280 library Temperature and Humidity sensor
// Always async web server now
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
AsyncWebServer server(80);
// #include <WiFiClientSecure.h>


#define VERSION "V2.4.1"

#define BUTTON_PIN_AP_MODE 13
// Relay (Heater) output pin (adjust to your wiring). Choose a free GPIO that can drive the relay.
#define RELAY_HEATER_PIN 25

// ⚠️ Warning ⚠️
// ESP32 has a limitation of 15 characters for the key name.
// The key name is built from the category and the key name (<category>_<key>).
// The category is limited to 13 characters, the key name to 1 character.
// Since V2.0.0, the key will be truncated if it is too long, but you now have a user-friendly displayName to show in the web interface.

// Since V2.0.0 there is a way to upload the firmware over the air (OTA).
// You can set the hostname and the password for OTA in the setupOTA function.


/* Please note:
        struct ConfigOptions {
            const char* keyName;
            const char* category;
            T defaultValue;
            const char* prettyName = nullptr;
            const char* prettyCat = nullptr;
            bool showInWeb = true;
            bool isPassword = false;
            void (*cb)(T) = nullptr;
            std::function<bool()> showIf = nullptr;
        };
*/

ConfigManagerClass cfg; // Create an instance of ConfigManager before using it in structures etc.
ConfigManagerClass::LogCallback ConfigManagerClass::logger = nullptr; // Initialize the logger to nullptr

// (Server instance moved into conditional above)
int cbTestValue = 0;

// Here, global variables are used without a struct, e.g., Config is a helper class for the settings stored in ConfigManager.h
// 2025.08.17 Breaking Changes in Interface --> now with struct initialization


// minimal Init
Config<bool> testBool(ConfigOptions<bool>{
    .keyName = "tbool",
    .category = "main",
    .defaultValue = true
});


// extended version with UI-friendly prettyName and prettyCategory
// Improved version since V2.0.0

Config<int> updateInterval(ConfigOptions<int>{
    .keyName = "interval",
    .category = "main",
    .defaultValue = 30,
    .prettyName = "Update Interval (seconds)"
});

// Now, these will be truncated and added if their truncated keys are unique:
Config<float> VeryLongCategoryName(ConfigOptions<float>{
    .keyName = "VlongC",
    .category = "VeryLongCategoryName",
    .defaultValue = 0.1f,
    .prettyName = "category Correction long",
    .prettyCat = "key Correction"
});
Config<float> VeryLongKeyName(ConfigOptions<float>{
    .keyName = "VeryLongKeyName",
    .category = "Temp",
    .defaultValue = 0.1f,
    .prettyName = "key Correction long",
    .prettyCat = "key Correction"
});

// ---- Temporary dynamic visibility test settings ----
// Category kept short ("DynTest") to avoid key truncation when combined with key names.
Config<bool> tempBoolToggle(ConfigOptions<bool>{
    .keyName = "toggle",
    .category = "DynTest",
    .defaultValue = true,
    .prettyName = "Temp Toggle",
    .prettyCat = "Dynamic Test"
});
Config<String> tempSettingAktiveOnTrue(ConfigOptions<String>{
    .keyName = "trueS",
    .category = "DynTest",
    .defaultValue = String("Shown if toggle = true"),
    .prettyName = "Visible When True",
    .prettyCat = "Dynamic Test",
    .showIf = [](){ return tempBoolToggle.get(); }
});
Config<String> tempSettingAktiveOnFalse(ConfigOptions<String>{
    .keyName = "falseS",
    .category = "DynTest",
    .defaultValue = String("Shown if toggle = false"),
    .prettyName = "Visible When False",
    .prettyCat = "Dynamic Test",
    .showIf = [](){ return !tempBoolToggle.get(); }
});
// ---- End temporary dynamic visibility test settings ----


//--------------------------------------------------------------------
// Forward declarations of functions
void SetupCheckForAPModeButton();
void blinkBuidInLED(int BlinkCount, int blinkRate);

// Example: Callback usage
void testCallback(int val); // Callback function for testCb here defined, later implemented...
Config<int> testCb(ConfigOptions<int>{
    .keyName = "cbt",
    .category = "main",
    .defaultValue = 0,
    .prettyName = "Test Callback",
    .showInWeb = true,
    .isPassword = false,
    .cb = testCallback
});
#pragma endregion
//--------------------------------------------------------------------

// Example: Using structures for grouped settings
// General configuration (structure example)
struct General_Settings
{
    Config<bool> enableController;
    Config<bool> enableMQTT;
    Config<bool> saveDisplay;
    Config<int> displayShowTime;
    Config<bool> allowOTA;
    Config<String> otaPassword;

    Config<String> Version;

    General_Settings() :

    enableController(ConfigOptions<bool>{ .keyName = "enCtrl", .category = "Limiter", .defaultValue = true, .prettyName = "Enable Limitation" }),
    enableMQTT(ConfigOptions<bool>{ .keyName = "enMQTT", .category = "Limiter", .defaultValue = true, .prettyName = "Enable MQTT Propagation" }),

    saveDisplay(ConfigOptions<bool>{ .keyName = "Save", .category = "Display", .defaultValue = true, .prettyName = "Turn Display Off" }),
    displayShowTime(ConfigOptions<int>{ .keyName = "Time", .category = "Display", .defaultValue = 60, .prettyName = "Display On-Time in Sec" }),

    allowOTA(ConfigOptions<bool>{ .keyName = "OTAEn", .category = "System", .defaultValue = true, .prettyName = "Allow OTA Updates" }),
    otaPassword(ConfigOptions<String>{ .keyName = "OTAPass", .category = "System", .defaultValue = "ota1234", .prettyName = "OTA Password", .showInWeb = true, .isPassword = true }),
    Version(ConfigOptions<String>{ .keyName = "Version", .category = "System", .defaultValue = String(VERSION), .prettyName = "Program Version" })

    {
    // Register settings with ConfigManager
        cfg.addSetting(&enableController);
        cfg.addSetting(&enableMQTT);

        cfg.addSetting(&saveDisplay);
        cfg.addSetting(&displayShowTime);

        cfg.addSetting(&allowOTA);
        cfg.addSetting(&otaPassword);

        cfg.addSetting(&Version);

    }
};

General_Settings generalSettings; // Create an instance of General_Settings-Struct



// Example of a structure for WiFi settings
struct WiFi_Settings //wifiSettings
{
    Config<String> wifiSsid;
    Config<String> wifiPassword;
    Config<bool> useDhcp;
    Config<String> staticIp;
    Config<String> gateway;
    Config<String> subnet;

    WiFi_Settings() :
    wifiSsid(ConfigOptions<String>{ .keyName = "ssid", .category = "wifi", .defaultValue = "MyWiFi", .prettyName = "WiFi SSID", .prettyCat = "Network Settings" }),
    wifiPassword(ConfigOptions<String>{ .keyName = "password", .category = "wifi", .defaultValue = "secretpass", .prettyName = "WiFi Password", .prettyCat = "Network Settings", .showInWeb = true, .isPassword = true }),
    useDhcp(ConfigOptions<bool>{ .keyName = "dhcp", .category = "network", .defaultValue = false, .prettyName = "Use DHCP", .prettyCat = "Network Settings" }),
    staticIp(ConfigOptions<String>{ .keyName = "sIP", .category = "network", .defaultValue = "192.168.2.126", .prettyName = "Static IP", .prettyCat = "Network Settings", .showIf = [this]() { return !this->useDhcp.get(); } }),
    subnet(ConfigOptions<String>{ .keyName = "subnet", .category = "network", .defaultValue = "255.255.255.0", .prettyName = "Subnet-Mask", .prettyCat = "Network Settings", .showIf = [this]() { return !this->useDhcp.get(); } }),
    gateway(ConfigOptions<String>{ .keyName = "GW", .category = "network", .defaultValue = "192.168.2.250", .prettyName = "Gateway", .prettyCat = "Network Settings", .showIf = [this]() { return !this->useDhcp.get(); } })

    {
        cfg.addSetting(&wifiSsid);
        cfg.addSetting(&wifiPassword);
        cfg.addSetting(&useDhcp);
        cfg.addSetting(&staticIp);
        cfg.addSetting(&gateway);
        cfg.addSetting(&subnet);
    }
};

WiFi_Settings wifiSettings; // Create an instance of WiFi_Settings-Struct


// Declaration as a struct with callback function
struct MQTT_Settings {
    Config<int> mqtt_port;
    Config<String> mqtt_server;
    Config<String> mqtt_username;
    Config<String> mqtt_password;
    Config<String> mqtt_sensor_powerusage_topic;
    Config<String> Publish_Topic;

    // For dynamic topics based on Publish_Topic
    String mqtt_publish_setvalue_topic;
    String mqtt_publish_getvalue_topic;
    String mqtt_publish_Temperature_topic;
    String mqtt_publish_Humidity_topic;
    String mqtt_publish_Dewpoint_topic;

    // Now show extra pretty category name since V2.2.0: e.g., ("keyname", "category", "web displayName", "web Pretty category", defaultValue)
    MQTT_Settings() :
    mqtt_port(ConfigOptions<int>{ .keyName = "Port", .category = "MQTT", .defaultValue = 1883, .prettyName = "Port", .prettyCat = "MQTT-Section" }),
    mqtt_server(ConfigOptions<String>{ .keyName = "Server", .category = "MQTT", .defaultValue = String("192.168.2.3"), .prettyName = "Server-IP", .prettyCat = "MQTT-Section" }),
    mqtt_username(ConfigOptions<String>{ .keyName = "User", .category = "MQTT", .defaultValue = String("housebattery"), .prettyName = "User", .prettyCat = "MQTT-Section" }),
    mqtt_password(ConfigOptions<String>{ .keyName = "Pass", .category = "MQTT", .defaultValue = String("mqttsecret"), .prettyName = "Password", .prettyCat = "MQTT-Section", .showInWeb = true, .isPassword = true }),
    mqtt_sensor_powerusage_topic(ConfigOptions<String>{ .keyName = "PUT", .category = "MQTT", .defaultValue = String("emon/emonpi/power1"), .prettyName = "Powerusage Topic", .prettyCat = "MQTT-Section" }),
    Publish_Topic(ConfigOptions<String>{ .keyName = "MQTTT", .category = "MQTT", .defaultValue = String("SolarLimiter"), .prettyName = "Publish-Topic", .prettyCat = "MQTT-Section" })

    {
        cfg.addSetting(&mqtt_port);
        cfg.addSetting(&mqtt_server);
        cfg.addSetting(&mqtt_username);
        cfg.addSetting(&mqtt_password);
        cfg.addSetting(&mqtt_sensor_powerusage_topic);
        cfg.addSetting(&Publish_Topic);

    // Callback to update topics when Publish_Topic changes
        Publish_Topic.setCallback([this](String newValue) {
            this->updateTopics();
        });

    updateTopics(); // Make sure topics are initialized
    }

    void updateTopics() {
    String hostname = Publish_Topic.get(); // You can throw an error here if it's empty
        mqtt_publish_setvalue_topic = hostname + "/SetValue";
        mqtt_publish_getvalue_topic = hostname + "/GetValue";
        mqtt_publish_Temperature_topic = hostname + "/Temperature";
        mqtt_publish_Humidity_topic = hostname + "/Humidity";
        mqtt_publish_Dewpoint_topic = hostname + "/Dewpoint";
    }
};

MQTT_Settings mqttSettings; //mqttSettings

#pragma endregion

//--------------------------------------------------------------------
// implement read temperature function and variables to show how to unse live values
//--------------------------------------------------------------------------------------------------------------
// set the I2C address for the BME280 sensor for temperature and humidity
#define I2C_SDA 21
#define I2C_SCL 22
#define I2C_FREQUENCY 400000
#define BME280_FREQUENCY 400000
// #define BME280_ADDRESS 0x76 // I2C address for the BME280 sensor (default is 0x76) redefine, if needed

#define ReadTemperatureTicker 10.0 // time in seconds to read the temperature and humidity

BME280_I2C bme280;
Ticker temperatureTicker;

void readBme280(); //read the values from the BME280 (Temperature, Humidity) and calculate the dewpoint
void SetupStartTemperatureMeasuring(); //setup the BME280 temperature and humidity sensor
static float computeDewPoint(float temperatureC, float relHumidityPct); //compute the dewpoint from temperature and humidity

float temperature = 0.0; // current temperature in Celsius
float Dewpoint = 0.0; // current dewpoint in Celsius
float Humidity = 0.0; // current humidity in percent
float Pressure = 0.0; // current pressure in hPa

Config<float> TempCorrectionOffset(ConfigOptions<float>{
    .keyName = "TCO",
    .category = "Temp",
    .defaultValue = 0.1f,
    .prettyName = "Temperature Correction",
    .prettyCat = "Temperature Settings"
});
Config<float> HumidityCorrectionOffset(ConfigOptions<float>{
    .keyName = "HYO",
    .category = "Temp",
    .defaultValue = 0.1f,
    .prettyName = "Humidity Correction",
    .prettyCat = "Temperature Settings"
});

Config<int> SeaLevelPressure(ConfigOptions<int>{
    .keyName = "slp",
    .category = "Temp",
    .defaultValue = 1013,
    .prettyName = "Sea Level Pressure (hPa)",
    .prettyCat = "Temperature Settings"
});

Config<int> sensorInterval(ConfigOptions<int>{
    .keyName = "interval",
    .category = "Temp",
    .defaultValue = 30,
    .prettyName = "Read temperature Interval (seconds)",
    .prettyCat = "Temperature Settings"
});
//--------------------------------------------------------------------------------------------------------------


void setup()
{
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(BUTTON_PIN_AP_MODE, INPUT_PULLUP);
    pinMode(RELAY_HEATER_PIN, OUTPUT);
    digitalWrite(RELAY_HEATER_PIN, LOW); // assume LOW = off (adjust if your relay is active LOW/HIGH)

    //-----------------------------------------------------------------
    // Set logger callback to log in your own way, but do this before using the cfg object!
    // Example 1: use Serial for logging
    // void cbMyConfigLogger(const char *msg){ Serial.println(msg);}
    // ConfigManagerClass::setLogger(cbMyConfigLogger);

    // Or as a lambda function...
    ConfigManagerClass::setLogger([](const char *msg){
            Serial.print("[CFG] ");
            Serial.println(msg);
        });

    //-----------------------------------------------------------------


    //-----------------------------------------------------------------
    //-----------------------------------------------------------------
    //-----------------------------------------------------------------
    //temperature - Sensor settings (BME280) to show how to use settings in your own code
    cfg.addSetting(&TempCorrectionOffset);
    cfg.addSetting(&HumidityCorrectionOffset);
    cfg.addSetting(&SeaLevelPressure);
    cfg.addSetting(&sensorInterval);



    // Register example runtime provider
    cfg.addRuntimeProvider({
        .name = "system",
        .fill = [](JsonObject &o){
            o["freeHeap"] = ESP.getFreeHeap();
            o["rssi"] = WiFi.RSSI();
        }
    });

    cfg.addRuntimeProvider({
        .name = "flags",
        .fill = [](JsonObject &o){
            o["tempToggle"] = tempBoolToggle.get(); //yes, we can use before load all, cfg is allready initialized, but the value is not yet loaded, dont use it for logic! this a demo only for live values
        }
    });

    cfg.defineRuntimeField("system", "freeHeap", "Free Heap", "B", 0);
    cfg.defineRuntimeField("system", "rssi", "WiFi RSSI", "dBm", 0);
    cfg.defineRuntimeBool("flags", "tempToggle", "Temp Toggle", false); // no alarm styling

    // Sensor data provider
    cfg.addRuntimeProvider({
        .name = "sensors",
        .fill = [](JsonObject &o){
            // Primary short keys expected by frontend
            o["temp"] = temperature;
            o["hum"] = Humidity;
            o["dew"] = Dewpoint;
            o["Pressure"] = Pressure;
        }
    });

    // Runtime field metadata for dynamic UI
    // With thresholds: warn (yellow) and alarm (red). Example ranges; adjust as needed.
    cfg.defineRuntimeFieldThresholds("sensors", "temp", "Temperature", "°C", 1,
        1.0f, 30.0f,   // warnMin / warnMax
        0.0f, 32.0f,   // alarmMin / alarmMax
         true,true,true,true
    );

    cfg.defineRuntimeFieldThresholds("sensors", "hum", "Humidity", "%", 1,
        30.0f, 70.0f,
        15.0f, 90.0f,
        true,false,true,true
    );

    // only basic field, no thresholds
    cfg.defineRuntimeField("sensors", "dew", "Dewpoint", "°C", 1);
    cfg.defineRuntimeField("sensors", "Pressure", "Pressure", "hPa", 1);

    // Runtime boolean alarm
    cfg.defineRuntimeBool("alarms", "dewpoint_risk", "Dewpoint Risk", true); // show as bool alarm when true

    // Cross-field alarm: temperature within 1.0°C above dewpoint (risk of condensation)
    cfg.defineRuntimeAlarm(
        "dewpoint_risk",
        [](const JsonObject &root){
            if(!root.containsKey("sensors")) return false;
            const JsonObject sensors = root["sensors"].as<JsonObject>();
            if(!sensors.containsKey("temp") || !sensors.containsKey("dew")) return false;
            float t = sensors["temp"].as<float>();
            float d = sensors["dew"].as<float>();
            return (t - d) <= 1.2f; // risk window
        },
        [](){ Serial.println("[ALARM] Dewpoint proximity risk ENTER"); }, //here you could also trigger a relay or similar
        [](){ Serial.println("[ALARM] Dewpoint proximity risk EXIT"); }
    );

    // Temperature MIN alarm -> Heater relay ON when temperature below alarmMin (0.0°C) and OFF when recovered.
    // Uses a little hysteresis (enter < 0.0, exit > 0.5) to avoid fast toggling.
    cfg.defineRuntimeAlarm(
        "temp_low",
        [](const JsonObject &root){
            static bool lastState = false; // for hysteresis
            if(!root.containsKey("sensors")) return false;
            const JsonObject sensors = root["sensors"].as<JsonObject>();
            if(!sensors.containsKey("temp")) return false;
            float t = sensors["temp"].as<float>();
            // Hysteresis: once active keep it on until t > 0.5
            if(lastState){ // currently active -> wait until we are clearly above release threshold
                lastState = (t < 0.5f);
            } else {       // currently inactive -> trigger when below entry threshold
                lastState = (t < 0.0f);
            }
            return lastState;
        },
        [](){
            Serial.println("[ALARM] Temperature below 0.0°C -> HEATER ON");
            // digitalWrite(RELAY_HEATER_PIN, HIGH); 
        },
        [](){
            Serial.println("[ALARM] Temperature recovered -> HEATER OFF");
            // digitalWrite(RELAY_HEATER_PIN, LOW);
        }
    );
    cfg.defineRuntimeBool("alarms", "temp_low", "too low temperature", true); // show as bool alarm when true in UI

    SetupStartTemperatureMeasuring();
    //-----------------------------------------------------------------
    //-----------------------------------------------------------------
    //-----------------------------------------------------------------


    // Register other settings
    cfg.addSetting(&updateInterval);
    cfg.addSetting(&testCb);
    cfg.addSetting(&testBool);
    cfg.addSetting(&VeryLongCategoryName);
    cfg.addSetting(&VeryLongKeyName);

    // Register temporary dynamic test settings
    cfg.addSetting(&tempBoolToggle);
    cfg.addSetting(&tempSettingAktiveOnTrue);
    cfg.addSetting(&tempSettingAktiveOnFalse);

    cfg.addSetting(&sensorInterval);


    // 2025.09.04 New function to check all settings for errors
    cfg.checkSettingsForErrors();


    try
    {
    cfg.loadAll(); // Load all settings from preferences
    }
    catch(const std::exception& e)
    {
         Serial.println(e.what());
    }

    Serial.println("Loaded configuration:");

    generalSettings.Version.set(VERSION); // Update version on device
    cfg.saveAll(); // Save all settings to preferences

    SetupCheckForAPModeButton();

    delay(300);
    Serial.println("Configuration printout:");
    Serial.println(cfg.toJSON(false));

    // Test setting changes
    testBool.set(false);
    updateInterval.set(15);
    cfg.saveAll();
    delay(300);

    if (wifiSettings.wifiSsid.get().length() == 0)
    {
        Serial.printf("⚠️ SETUP: SSID is empty! [%s]\n", wifiSettings.wifiSsid.get().c_str());
        cfg.startAccessPoint();
    }

    if (WiFi.getMode() == WIFI_AP)
    {
        Serial.printf("🖥️  AP Mode! \n");
        return; // Skip webserver setup in AP mode
    }

    if (wifiSettings.useDhcp.get()) {
        Serial.println("DHCP enabled");
        cfg.startWebServer(wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());
    } else {
        Serial.println("DHCP disabled");
        cfg.startWebServer(wifiSettings.staticIp.get(), wifiSettings.gateway.get(), wifiSettings.subnet.get(), wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());
    }


    // Enable WebSocket push (always available)
    cfg.enableWebSocketPush(2000); // 2s Interval
    delay(1500);
    if (WiFi.status() == WL_CONNECTED && generalSettings.allowOTA.get()) {
        cfg.setupOTA("Ota-esp32-device", generalSettings.otaPassword.get().c_str());
    }
    Serial.printf("🖥️ Webserver running at: %s\n", WiFi.localIP().toString().c_str());
}

void loop()
{
    if (WiFi.getMode() == WIFI_AP)
    {
        blinkBuidInLED(3, 100);
    }
    else
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("❌ WiFi not connected!");
            cfg.reconnectWifi();
            delay(1000);
            return;
        }
        blinkBuidInLED(1, 100);
    }

    static unsigned long lastPrint = 0;
    int interval = max(updateInterval.get(), 1); // Prevent zero interval
    if (millis() - lastPrint > interval * 1000)
    {
        lastPrint = millis();
    // Serial.printf("Loop --> DHCP status: %s\n", useDhcp.get() ? "yes" : "no");
    // Serial.printf("Loop --> WiFi status: %s\n", WiFi.status() == WL_CONNECTED ? "connected" : "not connected");
        cbTestValue++;
        testCb.set(cbTestValue);
        if (cbTestValue > 10)
        {
            cbTestValue = 0;
        }
    }

    cfg.handleClient();
    cfg.handleWebsocketPush();
    // Evaluate cross-field runtime alarms periodically (cheap doc build ~ small JSON)
    static unsigned long lastAlarmEval = 0; if(millis() - lastAlarmEval > 1500){ lastAlarmEval = millis(); cfg.handleRuntimeAlarms(); }
    cfg.handleOTA();

    static unsigned long lastOTAmessage = 0;
    if (millis() - lastOTAmessage > 10000) {
        lastOTAmessage = millis();
        Serial.printf("OTA Status: %s\n", cfg.getOTAStatus().c_str());
    }

    delay(500);
}

void testCallback(int val)
{
    Serial.printf("Callback called with value: %d\n", val);
}

void SetupCheckForAPModeButton()
{
    String APName = "ESP32_Config";
    String pwd = "config1234"; // Default AP password

    Serial.println("Checking AP mode button...");

    if (digitalRead(BUTTON_PIN_AP_MODE) == LOW)
    {
        Serial.printf("AP mode button pressed -> Starting AP with\n --> SSID: %s \n --> Password: %s\n", APName.c_str(), pwd.c_str());

        // Choose preferred AP mode:
        // cfg.startAccessPoint("192.168.4.1", "255.255.255.0", "ESP32_Config", "config1234");
        // cfg.startAccessPoint("192.168.4.1", "255.255.255.0", "ESP32_Config", pwd);
        // cfg.startAccessPoint("192.168.4.1", "255.255.255.0", APName, pwd);
        cfg.startAccessPoint("192.168.4.1", "255.255.255.0", APName, "");
        // cfg.startAccessPoint();
    }
}

void SetupStartTemperatureMeasuring()
{
  // init BME280 for temperature and humidity sensor
  bme280.setAddress(BME280_ADDRESS, I2C_SDA, I2C_SCL);
  bool isStatus = bme280.begin(
      bme280.BME280_STANDBY_0_5,
      bme280.BME280_FILTER_16,
      bme280.BME280_SPI3_DISABLE,
      bme280.BME280_OVERSAMPLING_2,
      bme280.BME280_OVERSAMPLING_16,
      bme280.BME280_OVERSAMPLING_1,
      bme280.BME280_MODE_NORMAL);
  if (!isStatus)
  {
    Serial.println("can NOT initialize for using BME280.");
  }
  else
  {
    Serial.println("ready to using BME280. Sart Ticker...");
    int iv = sensorInterval.get();
    if(iv < 2) iv = 2;
    temperatureTicker.attach((float)iv, readBme280); // Attach ticker with configured interval
    readBme280();                                                // Read once at startup
  }
}

static float computeDewPoint(float temperatureC, float relHumidityPct) {
    if (isnan(temperatureC) || isnan(relHumidityPct)) return NAN;
    if (relHumidityPct <= 0.0f) relHumidityPct = 0.1f;       // Unterlauf abfangen
    if (relHumidityPct > 100.0f) relHumidityPct = 100.0f;    // Clamp
    const float a = 17.62f;
    const float b = 243.12f;
    float rh = relHumidityPct / 100.0f;
    float gamma = (a * temperatureC) / (b + temperatureC) + log(rh);
    float dew = (b * gamma) / (a - gamma);
    return dew;
}

void readBme280()
{
  //   set sea-level pressure
  bme280.setSeaLevelPressure(SeaLevelPressure.get());

  bme280.read();

  temperature = bme280.data.temperature + TempCorrectionOffset.get();
  Humidity = bme280.data.humidity + HumidityCorrectionOffset.get();
  Pressure = bme280.data.pressure;
  
  Dewpoint = computeDewPoint(temperature, Humidity);

  // output formatted values to serial console
  Serial.println("-----------------------");
  Serial.printf("\r\nTemperature: %2.1lf °C | offset: %2.1lf K", temperature, TempCorrectionOffset.get());
  Serial.printf("\r\nHumidity   : %2.1lf %rH | offset: %2.1lf %rH", Humidity, HumidityCorrectionOffset.get());
  Serial.printf("\r\nDewpoint   : %2.1lf °C", Dewpoint);
  Serial.printf("\r\nPressure   : %4.0lf hPa", Pressure);
  Serial.printf("\r\nAltitude   : %4.2lf m", bme280.data.altitude);
  Serial.println("-----------------------");
}

void blinkBuidInLED(int BlinkCount, int blinkRate)
{
    for (int i = 0; i < BlinkCount; i++)
    {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(blinkRate);
        digitalWrite(LED_BUILTIN, LOW);
        delay(blinkRate);
    }
}