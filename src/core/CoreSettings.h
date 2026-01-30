#pragma once

// CoreSettings.h
//
// Header-only, opt-in "core settings templates" for sketches.
//
// Rationale:
// - Keep examples/sketches smaller by providing reusable baseline settings bundles.
// - Keep this strictly opt-in: nothing changes unless the sketch includes this header and calls attach().
//
// Notes:
// - Defaults are intentionally conservative (no real credentials).
// - Settings are persisted automatically by ConfigManager when a key does not exist yet.

#include <Arduino.h>

#include "ConfigManager.h"

namespace cm
{

namespace CoreCategories
{
inline constexpr char WiFi[] = "WiFi";
inline constexpr char System[] = "System";
inline constexpr char Buttons[] = "Buttons";
inline constexpr char IO[] = "IO";
inline constexpr char Ntp[] = "NTP";
}

struct CoreWiFiSettings
{
    Config<String> wifiSsid{ConfigOptions<String>{.key = "WiFiSSID", .name = "WiFi SSID", .category = CoreCategories::WiFi, .defaultValue = String(""), .showInWeb = true, .sortOrder = 1}};
    Config<String> wifiPassword{ConfigOptions<String>{.key = "WiFiPassword", .name = "WiFi Password", .category = CoreCategories::WiFi, .defaultValue = String(""), .showInWeb = true, .isPassword = true, .sortOrder = 2}};
    Config<bool> useDhcp{ConfigOptions<bool>{.key = "WiFiUseDHCP", .name = "Use DHCP", .category = CoreCategories::WiFi, .defaultValue = true, .showInWeb = true, .sortOrder = 3}};

    Config<String> staticIp{ConfigOptions<String>{.key = "WiFiStaticIP", .name = "Static IP", .category = CoreCategories::WiFi, .defaultValue = String("192.168.0.10"), .showInWeb = true, .sortOrder = 4}};
    Config<String> gateway{ConfigOptions<String>{.key = "WiFiGateway", .name = "Gateway", .category = CoreCategories::WiFi, .defaultValue = String("192.168.0.1"), .showInWeb = true, .sortOrder = 5}};
    Config<String> subnet{ConfigOptions<String>{.key = "WiFiSubnet", .name = "Subnet Mask", .category = CoreCategories::WiFi, .defaultValue = String("255.255.255.0"), .showInWeb = true, .sortOrder = 6}};
    Config<String> dnsPrimary{ConfigOptions<String>{.key = "WiFiDNS1", .name = "Primary DNS", .category = CoreCategories::WiFi, .defaultValue = String("192.168.0.1"), .showInWeb = true, .sortOrder = 7}};
    Config<String> dnsSecondary{ConfigOptions<String>{.key = "WiFiDNS2", .name = "Secondary DNS", .category = CoreCategories::WiFi, .defaultValue = String("8.8.8.8"), .showInWeb = true, .sortOrder = 8}};

    Config<int> rebootTimeoutMin{ConfigOptions<int>{.key = "WiFiRb", .name = "Reboot if WiFi lost (min)", .category = CoreCategories::WiFi, .defaultValue = 5, .showInWeb = true, .sortOrder = 20}};

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&wifiSsid);
        cfg.addSetting(&wifiPassword);
        cfg.addSetting(&useDhcp);
        cfg.addSetting(&staticIp);
        cfg.addSetting(&gateway);
        cfg.addSetting(&subnet);
        cfg.addSetting(&dnsPrimary);
        cfg.addSetting(&dnsSecondary);
        cfg.addSetting(&rebootTimeoutMin);

        // Keep DHCP-dependent visibility consistent with the original demo pattern.
        staticIp.showIfFunc = [this]()
        { return !useDhcp.get(); };
        gateway.showIfFunc = [this]()
        { return !useDhcp.get(); };
        subnet.showIfFunc = [this]()
        { return !useDhcp.get(); };
        dnsPrimary.showIfFunc = [this]()
        { return !useDhcp.get(); };
        dnsSecondary.showIfFunc = [this]()
        { return !useDhcp.get(); };
    }
};

struct CoreSystemSettings
{
    explicit CoreSystemSettings(const String &defaultVersion)
                : allowOTA(ConfigOptions<bool>{.key = "OTAEn", .name = "Allow OTA Updates", .category = CoreCategories::System, .defaultValue = true, .showInWeb = true, .sortOrder = 1}),
                    otaPassword(ConfigOptions<String>{.key = "OTAPass", .name = "OTA Password", .category = CoreCategories::System, .defaultValue = String(""), .showInWeb = true, .isPassword = true, .sortOrder = 2}),
                    version(ConfigOptions<String>{.key = "P_Version", .name = "Program Version", .category = CoreCategories::System, .defaultValue = defaultVersion, .showInWeb = true, .sortOrder = 3})
    {
    }

    Config<bool> allowOTA;
    Config<String> otaPassword;
    Config<String> version;

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&allowOTA);
        cfg.addSetting(&otaPassword);
        cfg.addSetting(&version);
    }
};

struct CoreNtpSettings
{
    Config<int> frequencySec{ConfigOptions<int>{.key = "NTPFrq", .name = "NTP Sync Interval (s)", .category = CoreCategories::Ntp, .defaultValue = 3600, .showInWeb = true, .sortOrder = 1}};
    Config<String> server1{ConfigOptions<String>{.key = "NTP1", .name = "NTP Server 1", .category = CoreCategories::Ntp, .defaultValue = String("192.168.2.250"), .showInWeb = true, .sortOrder = 2}};
    Config<String> server2{ConfigOptions<String>{.key = "NTP2", .name = "NTP Server 2", .category = CoreCategories::Ntp, .defaultValue = String("pool.ntp.org"), .showInWeb = true, .sortOrder = 3}};
    Config<String> tz{ConfigOptions<String>{.key = "NTPTZ", .name = "Time Zone (POSIX)", .category = CoreCategories::Ntp, .defaultValue = String("CET-1CEST,M3.5.0/02,M10.5.0/03"), .showInWeb = true, .sortOrder = 4}};

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&frequencySec);
        cfg.addSetting(&server1);
        cfg.addSetting(&server2);
        cfg.addSetting(&tz);
    }
};

struct CoreButtonSettings
{
    // Pins are disabled by default.
    // Convention: pin < 0 means "not present" (skip pinMode/digitalRead).
    // Note: ESP32 has GPIO0. It is a boot strapping pin, so using it for buttons is possible but needs care.
    // TODO(IOManager): Later move button handling (pinMode/digitalRead/debounce) into an IOManager module.
    // API idea:
    //   - bool IOManager::checkResetToDefaultsButton(const CoreButtonSettings& cfg, std::function<void()> onPressed);
    //   - bool IOManager::checkApModeButton(const CoreButtonSettings& cfg, std::function<void()> onPressed);
    // Notes:
    //   - The callback should decide the action (e.g. show feedback, clear prefs, reboot).
    Config<int> apModePin{ConfigOptions<int>{.key = "BtnAP", .name = "AP Mode Button GPIO", .category = CoreCategories::Buttons, .defaultValue = -1, .showInWeb = true, .sortOrder = 1}};
    Config<int> resetDefaultsPin{ConfigOptions<int>{.key = "BtnRst", .name = "Reset Defaults Button GPIO", .category = CoreCategories::Buttons, .defaultValue = -1, .showInWeb = true, .sortOrder = 2}};

    // Default behavior matches the existing examples: INPUT_PULLUP + pressed == LOW.
    Config<bool> apModeActiveLow{ConfigOptions<bool>{.key = "BtnAPLow", .name = "AP Mode Active LOW", .category = CoreCategories::Buttons, .defaultValue = true, .showInWeb = true, .sortOrder = 3}};
    Config<bool> apModeUsePullup{ConfigOptions<bool>{.key = "BtnAPPU", .name = "AP Mode Use Pull-Up", .category = CoreCategories::Buttons, .defaultValue = true, .showInWeb = true, .sortOrder = 4}};

    Config<bool> resetActiveLow{ConfigOptions<bool>{.key = "BtnRstLow", .name = "Reset Active LOW", .category = CoreCategories::Buttons, .defaultValue = true, .showInWeb = true, .sortOrder = 5}};
    Config<bool> resetUsePullup{ConfigOptions<bool>{.key = "BtnRstPU", .name = "Reset Use Pull-Up", .category = CoreCategories::Buttons, .defaultValue = true, .showInWeb = true, .sortOrder = 6}};

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&apModePin);
        cfg.addSetting(&resetDefaultsPin);

        cfg.addSetting(&apModeActiveLow);
        cfg.addSetting(&apModeUsePullup);
        cfg.addSetting(&resetActiveLow);
        cfg.addSetting(&resetUsePullup);
    }
};

class CoreSettings
{
public:
    static CoreSettings &instance()
    {
        static CoreSettings inst;
        return inst;
    }

    // Attach all core settings bundles.
    void attach(ConfigManagerClass &cfg)
    {
        attachWiFi(cfg);
        attachSystem(cfg);
        attachButtons(cfg);
    }

    void attachWiFi(ConfigManagerClass &cfg)
    {
        if (wifiAttached)
            return;
        wifi.attachTo(cfg);
        wifiAttached = true;
    }

    void attachSystem(ConfigManagerClass &cfg)
    {
        if (systemAttached)
            return;
        system.attachTo(cfg);
        systemAttached = true;
    }

    void attachButtons(ConfigManagerClass &cfg)
    {
        if (buttonsAttached)
            return;
        buttons.attachTo(cfg);
        buttonsAttached = true;
    }

    // Optional bundle: NTP settings.
    // Intentionally not part of attach() to keep the core bundles minimal.
    void attachNtp(ConfigManagerClass &cfg)
    {
        if (ntpAttached)
            return;
        ntp.attachTo(cfg);
        ntpAttached = true;
    }

    CoreWiFiSettings wifi;
    CoreButtonSettings buttons;
    CoreSystemSettings system{String(CONFIGMANAGER_VERSION)};
    CoreNtpSettings ntp;

private:
    CoreSettings() = default;

    bool wifiAttached = false;
    bool systemAttached = false;
    bool buttonsAttached = false;
    bool ntpAttached = false;

    // Intentionally no dynamic allocation here.
};

} // namespace cm
