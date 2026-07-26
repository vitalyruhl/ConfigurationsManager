// Updated tests for new ConfigOptions-based interface
#include <Arduino.h>
#include <esp_heap_caps.h>
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

#ifdef CM_RUNTIME_META_TEST_INSTRUMENTATION
namespace {

constexpr size_t RUNTIME_META_FIXTURE_ENTRIES = 30;
constexpr size_t RUNTIME_META_KEY_MAX_LEN = 15;
const char* const RUNTIME_STYLE_TARGETS[] = {
    "row", "label", "values", "unit", "state",
    "stateDotOnTrue", "stateDotOnFalse", "stateDotOnAlarm"
};

String runtimeMetaText(const char* prefix, size_t index) {
    // Runtime metadata strings have no API length cap. Keep persistent-style
    // keys at their documented 15-character limit and use long valid UTF-8
    // text that exercises JSON escaping in every other text field.
    String value(prefix);
    value += " \"\\\n\t\xC3\xA4\xC3\x9F\xE6\xBC\xA2";
    value += " value ";
    value += index;
    return value;
}

RuntimeFieldMeta makeRuntimeMetaFixtureEntry(size_t index) {
    RuntimeFieldMeta meta;
    meta.group = runtimeMetaText("group", index);
    meta.sourceGroup = runtimeMetaText("source", index);
    meta.page = runtimeMetaText("page", index);
    meta.card = runtimeMetaText("card", index);
    meta.key = String("runtime_key_") + String(index);
    while (meta.key.length() < RUNTIME_META_KEY_MAX_LEN) {
        meta.key += "x";
    }
    meta.label = runtimeMetaText("label", index);
    meta.unit = runtimeMetaText("unit", index);
    meta.staticValue = runtimeMetaText("static", index);
    meta.onLabel = runtimeMetaText("on", index);
    meta.offLabel = runtimeMetaText("off", index);
    meta.precision = static_cast<int>(index % 4);
    meta.order = static_cast<int>(index);
    meta.isBool = true;
    meta.isString = true;
    meta.isStateButton = true;
    meta.isIntSlider = true;
    meta.isFloatSlider = true;
    meta.isIntInput = true;
    meta.isFloatInput = true;
    meta.hasAlarm = true;
    meta.alarmWhenTrue = true;
    meta.boolAlarmValue = true;
    meta.intMin = -100;
    meta.intMax = 100;
    meta.intInit = static_cast<int>(index);
    meta.floatMin = -10.5f;
    meta.floatMax = 50.5f;
    meta.floatInit = static_cast<float>(index) / 10.0f;
    meta.alarmMin = -5.0f;
    meta.alarmMax = 45.0f;
    meta.warnMin = 0.0f;
    meta.warnMax = 40.0f;

    for (const char* target : RUNTIME_STYLE_TARGETS) {
        RuntimeStyleRule& rule = meta.style.rule(target);
        rule.setVisible((index % 2) == 0);
        rule.addCSSClass("style-\"class");
        rule.set("data-\"value", "\\escaped\n\t\xC3\x9F");
    }
    return meta;
}

void assertRuntimeMetaJson(const String& json, size_t expectedEntries) {
    DynamicJsonDocument doc(65536);
    const DeserializationError error = deserializeJson(doc, json);
    TEST_ASSERT_FALSE(error);
    JsonArray entries = doc.as<JsonArray>();
    TEST_ASSERT_EQUAL_UINT32(expectedEntries, entries.size());
    String normalized;
    TEST_ASSERT_EQUAL_UINT32(json.length(), serializeJson(doc, normalized));
    TEST_ASSERT_EQUAL_UINT32(json.length(), normalized.length());
    for (size_t index = 0; index < expectedEntries; ++index) {
        String expectedKey = String("runtime_key_") + String(index);
        while (expectedKey.length() < RUNTIME_META_KEY_MAX_LEN) {
            expectedKey += "x";
        }

        JsonObject entry;
        for (JsonObject candidate : entries) {
            const char* key = candidate["key"].as<const char*>();
            if (key && expectedKey == key) {
                entry = candidate;
                break;
            }
        }

        TEST_ASSERT_FALSE(entry.isNull());
        TEST_ASSERT_EQUAL_STRING(runtimeMetaText("group", index).c_str(), entry["group"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING(runtimeMetaText("source", index).c_str(), entry["sourceGroup"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING(runtimeMetaText("page", index).c_str(), entry["page"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING(runtimeMetaText("card", index).c_str(), entry["card"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING(runtimeMetaText("label", index).c_str(), entry["label"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING(runtimeMetaText("unit", index).c_str(), entry["unit"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING(runtimeMetaText("static", index).c_str(), entry["staticValue"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING(runtimeMetaText("on", index).c_str(), entry["onLabel"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING(runtimeMetaText("off", index).c_str(), entry["offLabel"].as<const char*>());

        JsonObject style = entry["style"].as<JsonObject>();
        for (const char* target : RUNTIME_STYLE_TARGETS) {
            TEST_ASSERT_TRUE(style.containsKey(target));
            JsonObject rule = style[target].as<JsonObject>();
            TEST_ASSERT_TRUE(rule.containsKey("visible"));
            TEST_ASSERT_EQUAL_STRING("style-\"class", rule["className"].as<const char*>());
            TEST_ASSERT_EQUAL_STRING("\\escaped\n\t\xC3\x9F", rule["data-\"value"].as<const char*>());
        }
    }
}

void test_runtime_meta_serialization_avoids_deep_style_copy() {
    ConfigManagerRuntime emptyRuntime;
    RuntimeStyleRule::resetCopyCount();
    const String emptyJson = emptyRuntime.runtimeMetaToJSON();
    TEST_ASSERT_EQUAL_STRING("[]", emptyJson.c_str());
    TEST_ASSERT_EQUAL_UINT32(0, RuntimeStyleRule::getCopyCount());

    ConfigManagerRuntime oneRuntime;
    oneRuntime.addRuntimeMeta(makeRuntimeMetaFixtureEntry(0));
    RuntimeStyleRule::resetCopyCount();
    const String oneJson = oneRuntime.runtimeMetaToJSON();
    assertRuntimeMetaJson(oneJson, 1);
    TEST_ASSERT_EQUAL_UINT32(0, RuntimeStyleRule::getCopyCount());

    ConfigManagerRuntime runtime;
    for (size_t index = 0; index < RUNTIME_META_FIXTURE_ENTRIES; ++index) {
        runtime.addRuntimeMeta(makeRuntimeMetaFixtureEntry(index));
    }

    const size_t freeBefore = ESP.getFreeHeap();
    const size_t largestBefore = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    RuntimeStyleRule::resetCopyCount();
    const String json = runtime.runtimeMetaToJSON();
    const size_t freeAfter = ESP.getFreeHeap();
    const size_t largestAfter = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    assertRuntimeMetaJson(json, RUNTIME_META_FIXTURE_ENTRIES);
    TEST_ASSERT_EQUAL_UINT32(0, RuntimeStyleRule::getCopyCount());
    TEST_ASSERT_TRUE(json.length() > 10000);
    Serial.printf("[runtime-meta] entries=%u json=%u heap_delta=%d largest_delta=%d style_copies=%u\n",
                  static_cast<unsigned>(RUNTIME_META_FIXTURE_ENTRIES),
                  static_cast<unsigned>(json.length()),
                  static_cast<int>(freeBefore) - static_cast<int>(freeAfter),
                  static_cast<int>(largestBefore) - static_cast<int>(largestAfter),
                  static_cast<unsigned>(RuntimeStyleRule::getCopyCount()));

    for (size_t iteration = 0; iteration < 100; ++iteration) {
        RuntimeStyleRule::resetCopyCount();
        const String repeatedJson = runtime.runtimeMetaToJSON();
        TEST_ASSERT_EQUAL_UINT32(json.length(), repeatedJson.length());
        TEST_ASSERT_EQUAL_UINT32(0, RuntimeStyleRule::getCopyCount());
    }

    runtime.setRuntimeMetaSerializationFailureForTest(true);
    TEST_ASSERT_EQUAL_UINT32(0, runtime.runtimeMetaToJSON().length());
    runtime.setRuntimeMetaSerializationFailureForTest(false);
    const String recoveredJson = runtime.runtimeMetaToJSON();
    assertRuntimeMetaJson(recoveredJson, RUNTIME_META_FIXTURE_ENTRIES);
}

} // namespace
#endif

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

#ifdef CM_RUNTIME_META_TEST_INSTRUMENTATION
    RUN_TEST(test_runtime_meta_serialization_avoids_deep_style_copy);
#endif

    UNITY_END();
}

void loop() {
    while(1) { delay(10); }
}
