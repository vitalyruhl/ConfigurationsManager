// Default weak stubs for WiFi event hooks.
// These prevent linker errors if an application does not provide its own implementations.
// If the application defines the same functions in another TU, those strong definitions override these.

void onWiFiConnected() __attribute__((weak));
void onWiFiDisconnected() __attribute__((weak));
void onWiFiAPMode() __attribute__((weak));

void onWiFiConnected() {}
void onWiFiDisconnected() {}
void onWiFiAPMode() {}
