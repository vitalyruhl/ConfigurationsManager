#include <Arduino.h>
#include "ConfigManager.h"

#define BUTTON_PIN_AP_MODE 13

#define sl Serial // Dummy-Logger

ConfigManagerClass configManager;
Config<String> wifiSsid("ssid", "wifi", "MyWiFi"); 
Config<String> wifiPassword("password", "wifi", "secretpass", true, true);
Config<bool> useDhcp("dhcp", "network", true);
Config<int> updateInterval("interval", "network", 30);


void SetupCheckForAPModeButton();

void setup() {
  sl.begin(115200);
    delay(500);

    // register settings
    configManager.addSetting(&wifiSsid);
    configManager.addSetting(&wifiPassword);
    configManager.addSetting(&useDhcp);
    configManager.addSetting(&updateInterval);

    configManager.loadAll();

    SetupCheckForAPModeButton();

    sl.println("Config:");
    sl.println(configManager.toJSON());

    // Test change of settings
    wifiSsid.set("NewNetwork");
    configManager.saveAll();

    configManager.startAccessPoint();


}

void loop() {
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > updateInterval.get() * 1000) {
        lastPrint = millis();
        sl.printf("Loop --> DHCP: %s\n", useDhcp.get() ? "jop" : "nop");
    }
}

void SetupCheckForAPModeButton() {
    if (wifiSsid.get().length() == 0) {
        sl.printf("⚠️ SETUP: config.wifi_config.ssid is empty! [%s]\n", wifiSsid.get().c_str());
        configManager.startAccessPoint();
        return;
    }

    pinMode(BUTTON_PIN_AP_MODE, INPUT_PULLUP);
    if (digitalRead(BUTTON_PIN_AP_MODE) == LOW) {
        sl.println("AP-Mode-Button pressed... -> Start AP-Mode...");
        configManager.startAccessPoint();
    }
}
