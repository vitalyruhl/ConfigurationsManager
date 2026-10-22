#include <ConfigManager.h>
#include <bluetooth/BluetoothManager.h>
#include <core/CoreSettings.h>
#include <core/CoreWiFiServices.h>
static cm::BluetoothManager bluetooth;
static auto& core = cm::CoreSettings::instance();
static cm::CoreWiFiServices wifi;
void setup() {
  ConfigManager.setAppName("CM-Bluetooth-Demo");
  core.attachWiFi(ConfigManager);
  core.attachSystem(ConfigManager);
  bluetooth.attach(ConfigManager); // Registers settings once, before loadAll().
  ConfigManager.getOTAManager().setLifecycleObserver([](const OtaLifecycleEvent& event) {
    if (event.stage == OtaStage::Starting || event.stage == OtaStage::Succeeded)
      bluetooth.requestPause(cm::BlePauseReason::Ota);
    else
      bluetooth.requestResume(cm::BlePauseReason::Ota);
  });
  ConfigManager.loadAll();
  // BluetoothEnable defaults false. Configure device addresses/UUIDs in your application.
  bluetooth.setEventHandler([](const cm::BleEvent&) { /* Decode in the application loop. */ });
  bluetooth.begin();
  ConfigManager.startWebServer();
}
void loop() {
  ConfigManager.getWiFiManager().update();
  ConfigManager.handleClient();
  bluetooth.loop(millis());
}
void onWiFiConnected() {
  wifi.onConnected(ConfigManager, "CM-Bluetooth-Demo", core.system, core.ntp);
}
void onWiFiDisconnected() {
  wifi.onDisconnected();
}
void onWiFiAPMode() {
  wifi.onAPMode();
}
