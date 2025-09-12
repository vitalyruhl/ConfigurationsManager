// Updated tests for new ConfigOptions-based interface
#include <Arduino.h>
#include <unity.h>
#include <ConfigManager.h>
#include <WebServer.h>

// Removed duplicate WebHTML definition (provided by generated html_content.h)

WebServer server(80);
ConfigManagerClass testManager;
ConfigManagerClass::LogCallback ConfigManagerClass::logger = nullptr;

// ----------------------------------------------------------------------------
// Test Settings (using new ConfigOptions struct initialization)
// ----------------------------------------------------------------------------
Config<int> testInt(ConfigOptions<int>{ .keyName = "tInt", .category = "cfg", .defaultValue = 42, .prettyName = "Test Integer" });
Config<bool> testBool(ConfigOptions<bool>{ .keyName = "tBool", .category = "cfg", .defaultValue = true, .prettyName = "Test Boolean" });
Config<String> testString(ConfigOptions<String>{ .keyName = "tStr", .category = "cfg", .defaultValue = "def", .prettyName = "Test String" });
Config<float> testFloat(ConfigOptions<float>{ .keyName = "tFlt", .category = "cfg", .defaultValue = 3.14f, .prettyName = "Test Float" });
Config<String> testPassword(ConfigOptions<String>{ .keyName = "pwd", .category = "auth", .defaultValue = "secret", .prettyName = "Test Password", .showInWeb = true, .isPassword = true });
Config<String> wifiSsid(ConfigOptions<String>{ .keyName = "ssid", .category = "wifi", .defaultValue = "MyWiFi", .prettyName = "WiFi SSID", .prettyCat = "Network Settings" });
Config<String> wifiPassword(ConfigOptions<String>{ .keyName = "password", .category = "wifi", .defaultValue = "secretpass", .prettyName = "WiFi Password", .prettyCat = "Network Settings", .showInWeb = true, .isPassword = true });
Config<bool> useDhcp(ConfigOptions<bool>{ .keyName = "dhcp", .category = "network", .defaultValue = true, .prettyName = "Use DHCP", .prettyCat = "Network Settings" });

// Callback tests (function pointer & std::function)
static bool callbackCalled = false;
void testCallbackFn(int) { callbackCalled = true; }
Config<int> testCb(ConfigOptions<int>{ .keyName = "cb", .category = "cfg", .defaultValue = 0, .prettyName = "Test Callback", .cb = testCallbackFn });
Config<int> testCbLambda(ConfigOptions<int>{ .keyName = "cbl", .category = "cfg", .defaultValue = 0, .prettyName = "Lambda Callback" });

// showIf dependent setting
Config<bool> featureEnable(ConfigOptions<bool>{ .keyName = "feat", .category = "opt", .defaultValue = false, .prettyName = "Feature Enable" });
Config<int> hiddenUnlessFeature(ConfigOptions<int>{ .keyName = "hid", .category = "opt", .defaultValue = 1, .prettyName = "Hidden Value", .showInWeb = true, .showIf = [](){ return featureEnable.get(); } });

// Setting without prettyName to test fallback to keyName
Config<int> noPretty(ConfigOptions<int>{ .keyName = "nopretty", .category = "misc", .defaultValue = 7 });

// ----------------------------------------------------------------------------
// Helper to parse JSON produced by manager
// ----------------------------------------------------------------------------
static String getJSON(bool includeSecrets = false) {
    return testManager.toJSON(includeSecrets);
}

// ----------------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------------
void test_int_config() {
    testInt.set(1337);
    testManager.saveAll();
    testInt.set(0);
    testManager.loadAll();
    TEST_ASSERT_EQUAL(1337, testInt.get());
}

void test_bool_config() {
    testBool.set(false);
    testManager.saveAll();
    testBool.set(true);
    testManager.loadAll();
    TEST_ASSERT_FALSE(testBool.get());
}

void test_string_config() {
    testString.set("Hello World!");
    testManager.saveAll();
    testString.set("reset");
    testManager.loadAll();
    TEST_ASSERT_EQUAL_STRING("Hello World!", testString.get().c_str());
}

void test_float_config() {
    testFloat.set(99.99f);
    testManager.saveAll();
    testFloat.set(0.0f);
    testManager.loadAll();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 99.99f, testFloat.get());
}

void test_password_masking_json() {
    String json = getJSON();
    // Ensure password value replaced with ***
    TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("\"pwd\""));
    TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("***"));
}

void test_password_ignore_mask_on_fromJSON() {
    // Change password to new value
    testPassword.set("newpass");
    testManager.saveAll();
    // Simulate incoming JSON trying to set *** (should be ignored)
    DynamicJsonDocument doc(256);
    doc["value"] = "***";
    bool changed = testPassword.fromJSON(doc["value"]);
    TEST_ASSERT_FALSE(changed); // Should not accept masked value
    TEST_ASSERT_EQUAL_STRING("newpass", testPassword.get().c_str());
}

void test_callback_function_pointer() {
    callbackCalled = false;
    testCb.set(10);
    TEST_ASSERT_TRUE(callbackCalled);
}

void test_callback_lambda() {
    bool lambdaCalled = false;
    testCbLambda.setCallback([&](int){ lambdaCalled = true; });
    testCbLambda.set(5);
    TEST_ASSERT_TRUE(lambdaCalled);
}

void test_display_name_and_fallback() {
    TEST_ASSERT_EQUAL_STRING("Test Integer", testInt.getDisplayName());
    TEST_ASSERT_EQUAL_STRING("nopretty", noPretty.getDisplayName()); // fallback
}

void test_category_pretty_once() {
    String json = getJSON();
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, json);
    TEST_ASSERT_FALSE_MESSAGE(err, "JSON parse failed");
    // Iterate categories and ensure that if categoryPretty exists it is a single string field
    for (JsonPair kv : doc.as<JsonObject>()) {
        JsonObject catObj = kv.value().as<JsonObject>();
        if (catObj.containsKey("categoryPretty")) {
            // Ensure it's a string
            TEST_ASSERT_TRUE(catObj["categoryPretty"].is<const char*>());
        }
    }
    // We just ensure no category repeats its own categoryPretty key; duplicates across different categories allowed.
}

void test_key_length_error_flag() {
    Config<String> longCat(ConfigOptions<String>{ .keyName = "k", .category = "verylongcategoryname", .defaultValue = "x", .prettyName = "LC" });
    if (!longCat.hasError()) {
        // Force evaluation of getKey() to trigger potential truncation logging
        longCat.getKey();
    }
    TEST_ASSERT_TRUE(longCat.hasError() || strlen(longCat.getKey()) <= 15);
}

void test_truncation_duplicate_prevention() {
    ConfigManagerClass localMgr;
    Config<String> dup1(ConfigOptions<String>{ .keyName = "abc", .category = "verylongcategoryname", .defaultValue = "A" });
    Config<String> dup2(ConfigOptions<String>{ .keyName = "axx", .category = "verylongcategoryname", .defaultValue = "B" });
    localMgr.addSetting(&dup1);
    localMgr.addSetting(&dup2); // Should be rejected after truncation collision
    String json = localMgr.toJSON();
    // Only original keyName of first should appear
    TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("abc"));
    TEST_ASSERT_EQUAL(-1, json.indexOf("axx"));
}

void test_showIf_visibility() {
    // Initially featureEnable = false, hiddenUnlessFeature.showIf => false
    TEST_ASSERT_FALSE(hiddenUnlessFeature.shouldShowInWebDynamic());
    featureEnable.set(true);
    TEST_ASSERT_TRUE(hiddenUnlessFeature.shouldShowInWebDynamic());
}

void test_ap_mode_initialization() {
    testManager.startAccessPoint("TestAP", "password");
    TEST_ASSERT_EQUAL(WIFI_AP, WiFi.getMode());
}

void test_sta_mode_initialization() {
    testManager.loadAll();
    testManager.startWebServer(wifiSsid.get(), wifiPassword.get());
    testManager.handleClient();
    TEST_ASSERT_EQUAL(WIFI_STA, WiFi.getMode());
}

void setup() {
    delay(1500);
    Serial.begin(115200);
    while(!Serial);
    disableCore0WDT();

    ConfigManagerClass::setLogger([](const char *msg){ Serial.printf("[test] %s\n", msg); });

    UNITY_BEGIN();

    // Register settings
    testManager.addSetting(&wifiSsid);
    testManager.addSetting(&wifiPassword);
    testManager.addSetting(&useDhcp);
    testManager.addSetting(&testInt);
    testManager.addSetting(&testBool);
    testManager.addSetting(&testString);
    testManager.addSetting(&testFloat);
    testManager.addSetting(&testPassword);
    testManager.addSetting(&testCb);
    testManager.addSetting(&testCbLambda);
    testManager.addSetting(&featureEnable);
    testManager.addSetting(&hiddenUnlessFeature);
    testManager.addSetting(&noPretty);

    // Core config persistence tests
    RUN_TEST(test_int_config);
    RUN_TEST(test_bool_config);
    RUN_TEST(test_string_config);
    RUN_TEST(test_float_config);
    RUN_TEST(test_password_masking_json);
    RUN_TEST(test_password_ignore_mask_on_fromJSON);

    // Callback & display
    RUN_TEST(test_callback_function_pointer);
    RUN_TEST(test_callback_lambda);
    RUN_TEST(test_display_name_and_fallback);

    // Structural / metadata
    RUN_TEST(test_category_pretty_once);
    RUN_TEST(test_key_length_error_flag);
    RUN_TEST(test_truncation_duplicate_prevention);
    RUN_TEST(test_showIf_visibility);

    // WiFi / modes
    RUN_TEST(test_ap_mode_initialization);
    RUN_TEST(test_sta_mode_initialization);

    UNITY_END();
}

void loop() {
    while(1) { delay(10); }
}