#pragma once

// Copy this file to wifiSecret.h and adjust values.
// NOTE: wifiSecret.h must stay out of version control.

// WiFi credentials
#define MY_WIFI_SSID "YOUR_WIFI_SSID"
#define MY_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Optional static IP (only used when DHCP is disabled in settings)
#define MY_WIFI_IP "192.168.0.130"

// Web UI settings password
#define SETTINGS_PASSWORD "change-me"

// OTA password
#define OTA_PASSWORD "change-me"

// Access Point (AP mode) credentials
// IMPORTANT: WPA2 password should be at least 8 characters.
#define APMODE_SSID "espAP"
#define APMODE_PASSWORD "change-me-1234"
