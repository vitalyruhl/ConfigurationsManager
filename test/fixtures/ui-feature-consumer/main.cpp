#include <ConfigManager.h>
#include <core/CoreSettings.h>
#include <logging/LoggingManager.h>
#include <mqtt/MQTTManager.h>

// Build-only consumer. Never uploaded by the feature matrix runner.
cm::CoreSystemSettings core(CONFIGMANAGER_VERSION);
cm::CoreWiFiSettings wifi;
cm::MQTTManager& mqtt = cm::MQTTManager::instance();
Config<int> featureThreshold(ConfigOptions<int>{.key = "Threshold", .name = "Threshold", .category = "Control", .defaultValue = 42});
volatile int featureOperations = 0;
int receivedValue = 0;

void setup() {
  ConfigManager.setAppName("UI feature consumer");
  ConfigManager.setSettingsPassword("fixture-access");
  core.attachTo(ConfigManager);
  wifi.attachTo(ConfigManager);
  ConfigManager.addSetting(&featureThreshold);
  featureThreshold.setCallback([](int value) { featureOperations = value; });
  featureThreshold.showIfFunc = [] { return featureThreshold.get() > 0; };
  ConfigManager.addSettingsPage("Control", 1);
  ConfigManager.addToSettings(featureThreshold.getKey(), "Control", 1);
  ConfigManager.addLivePage("Control", 1);
  ConfigManager.getRuntime().addRuntimeProvider("consumer", [](JsonObject& object) { object["value"] = featureThreshold.get(); });
  RuntimeFloatMeta meta;
  meta.base.sourceGroup = "consumer";
  meta.base.key = "value";
  meta.base.label = "Value";
  meta.base.page = "Control";
  ConfigManager.getRuntime().addRuntimeMeta(meta);
  ConfigManager.liveGroup("fluent").page("Control").value("fluentValue", [] { return featureThreshold.get(); }).label("Fluent").addCSSClass("feature-value");
  ConfigManager.defineRuntimeButton("consumer", "increment", "Increment", [] { featureThreshold.set(featureThreshold.get() + 1); });
  ConfigManager.getRuntime().addRuntimeAlarm("threshold", [] { return receivedValue > featureThreshold.get(); }, [] { ++featureOperations; }, [] { --featureOperations; });
  mqtt.attach(ConfigManager);
  mqtt.addTopicReceiveInt("received", "Received", "fixture/value", &receivedValue);
  mqtt.addMQTTRuntimeProviderToGUI(ConfigManager);
  cm::LoggingManager::instance().addOutput(std::make_unique<cm::LoggingManager::SerialOutput>());
#if !CM_DISABLE_GUI_LOGGING
  cm::LoggingManager::instance().addOutput(std::make_unique<cm::LoggingManager::GuiOutput>(ConfigManager));
#endif
  ConfigManager.loadAll();
  ConfigManager.startWebServerOnNetwork();
#ifdef development
  ConfigManager.getWebManager().addDevelopmentRoutes();
#endif
}

void loop() {
  ConfigManager.handleClient();
  mqtt.loop();
  featureOperations = featureThreshold.get();
}
