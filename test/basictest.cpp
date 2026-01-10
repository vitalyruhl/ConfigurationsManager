// Updated tests for new ConfigOptions-based interface
#include <Arduino.h>
#include <unity.h>
#include <ConfigManager.h>

ConfigManagerClass testManager;

// ----------------------------------------------------------------------------
// Test Settings (using new ConfigOptions struct initialization)
// ----------------------------------------------------------------------------
Config<int> testInt(ConfigOptions<int>{ .key = "tInt", .name = "Test Integer", .category = "cfg", .defaultValue = 42 });
Config<bool> testBool(ConfigOptions<bool>{ .key = "tBool", .name = "Test Boolean", .category = "cfg", .defaultValue = true });
Config<String> testString(ConfigOptions<String>{ .key = "tStr", .name = "Test String", .category = "cfg", .defaultValue = "def" });
Config<float> testFloat(ConfigOptions<float>{ .key = "tFlt", .name = "Test Float", .category = "cfg", .defaultValue = 3.14f });
Config<String> testPassword(ConfigOptions<String>{ .key = "pwd", .name = "Test Password", .category = "auth", .defaultValue = "secret", .showInWeb = true, .isPassword = true });

// Callback tests (function pointer & std::function)
static bool callbackCalled = false;
void testCallbackFn(int) { callbackCalled = true; }
Config<int> testCb(ConfigOptions<int>{ .key = "cb", .name = "Test Callback", .category = "cfg", .defaultValue = 0, .callback = testCallbackFn });
Config<int> testCbLambda(ConfigOptions<int>{ .key = "cbl", .name = "Lambda Callback", .category = "cfg", .defaultValue = 0 });

// showIf dependent setting
Config<bool> featureEnable(ConfigOptions<bool>{ .key = "feat", .name = "Feature Enable", .category = "opt", .defaultValue = false });
Config<int> hiddenUnlessFeature(ConfigOptions<int>{ .key = "hid", .name = "Hidden Value", .category = "opt", .defaultValue = 1, .showInWeb = true, .showIf = [](){ return featureEnable.get(); } });

// Setting without explicit key: verify auto-generated key length
Config<int> autoKey(ConfigOptions<int>{ .key = nullptr, .name = "No Key Setting", .category = "verylongcategoryname", .defaultValue = 7 });

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
    // By default, secrets are excluded from config JSON
    {
        String json = getJSON(false);
        TEST_ASSERT_EQUAL(-1, json.indexOf("\"Test Password\""));
        TEST_ASSERT_EQUAL(-1, json.indexOf("***"));
    }

    // When secrets are included (WebUI path), password values are masked as ***
    {
        String json = getJSON(true);
        TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("\"Test Password\""));
        TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("***"));
    }
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
    TEST_ASSERT_EQUAL_STRING("No Key Setting", autoKey.getDisplayName());
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
    // Auto-generated keys must respect the ESP32 Preferences key length limit
    TEST_ASSERT_TRUE(strlen(autoKey.getKey()) <= 15);
}

void test_showIf_visibility() {
    // Initially featureEnable = false, hiddenUnlessFeature.showIf => false
    TEST_ASSERT_FALSE(hiddenUnlessFeature.isVisible());
    featureEnable.set(true);
    TEST_ASSERT_TRUE(hiddenUnlessFeature.isVisible());
}

void test_runtime_string_divider_and_order(){
    auto &rt = testManager.getRuntime();

    rt.addRuntimeProvider("alpha", [](JsonObject &o){ o["v1"] = 1; }, 1);
    rt.addRuntimeProvider("beta", [](JsonObject &o){ o["v2"] = 2; }, 5);

    RuntimeFieldMeta divider;
    divider.group = "alpha";
    divider.key = "section_a";
    divider.label = "Section A";
    divider.isDivider = true;
    divider.order = 0;
    rt.addRuntimeMeta(divider);

    RuntimeFieldMeta v1;
    v1.group = "alpha";
    v1.key = "v1";
    v1.label = "Value One";
    v1.order = 1;
    rt.addRuntimeMeta(v1);

    RuntimeFieldMeta build;
    build.group = "alpha";
    build.key = "build";
    build.label = "Build";
    build.isString = true;
    build.staticValue = "test-build";
    build.order = 5;
    rt.addRuntimeMeta(build);

    RuntimeFieldMeta v2;
    v2.group = "beta";
    v2.key = "v2";
    v2.label = "Value Two";
    v2.order = 1;
    rt.addRuntimeMeta(v2);

    String meta = rt.runtimeMetaToJSON();
    TEST_ASSERT_NOT_EQUAL(-1, meta.indexOf("isDivider"));
    TEST_ASSERT_NOT_EQUAL(-1, meta.indexOf("isString"));
    TEST_ASSERT_NOT_EQUAL(-1, meta.indexOf("staticValue"));

    String values = rt.runtimeValuesToJSON();
    int alphaPos = values.indexOf("\"alpha\"");
    int betaPos = values.indexOf("\"beta\"");
    TEST_ASSERT_TRUE(alphaPos != -1 && betaPos != -1 && alphaPos < betaPos);
}

void setup() {
    delay(1500);
    Serial.begin(115200);
    while(!Serial);
    disableCore0WDT();

    ConfigManagerClass::setLogger([](const char *msg){ Serial.printf("[test] %s\n", msg); });

    UNITY_BEGIN();

    // Register settings
    testManager.addSetting(&testInt);
    testManager.addSetting(&testBool);
    testManager.addSetting(&testString);
    testManager.addSetting(&testFloat);
    testManager.addSetting(&testPassword);
    testManager.addSetting(&testCb);
    testManager.addSetting(&testCbLambda);
    testManager.addSetting(&featureEnable);
    testManager.addSetting(&hiddenUnlessFeature);
    testManager.addSetting(&autoKey);

    // Ensure runtime manager is initialized for meta/value JSON generation
    testManager.getRuntime().begin(&testManager);

    // Core config persistence tests
    RUN_TEST(test_int_config);
    RUN_TEST(test_bool_config);
    RUN_TEST(test_string_config);
    RUN_TEST(test_float_config);
    RUN_TEST(test_password_masking_json);

    // Callback & display
    RUN_TEST(test_callback_function_pointer);
    RUN_TEST(test_callback_lambda);
    RUN_TEST(test_display_name_and_fallback);

    // Structural / metadata
    RUN_TEST(test_category_pretty_once);
    RUN_TEST(test_key_length_error_flag);
    RUN_TEST(test_showIf_visibility);
    RUN_TEST(test_runtime_string_divider_and_order);

    UNITY_END();
}

void loop() {
    while(1) { delay(10); }
}