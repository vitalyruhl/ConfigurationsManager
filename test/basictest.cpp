#include <Arduino.h>
#include <unity.h>
#include <ConfigManager.h>
#include <exception>
#include <WebServer.h>

#ifdef UNIT_TEST
class WebHTML {
public:
    const char* getWebHTML() {
        return ""; // Return empty string for tests
    }
};
#endif

WebServer server(80);

ConfigManagerClass testManager;

// Test instances with displayName (V2 constructor)
Config<int> testInt("tInt", "cfg", "Test Integer", 42);
Config<bool> testBool("tBool", "cfg", "Test Boolean", true);
Config<String> testString("tStr", "cfg", "Test String", "def");
Config<float> testFloat("tFlt", "cfg", "Test Float", 3.14f);
Config<String> testPassword("pwd", "auth", "Test Password", "secret", true, true);
Config<String> wifiSsid("ssid", "wifi", "WiFi SSID", "MyWiFi");
Config<String> wifiPassword("password", "wifi", "WiFi Password", "secretpass", true, true);
Config<bool> useDhcp("dhcp", "network", "Use DHCP", true);

ConfigManagerClass::LogCallback ConfigManagerClass::logger = nullptr;

// Exception tests
void test_key_too_long_exception() {
    bool exceptionThrown = false;
    try {
        // Category > 13 characters should throw KeyTooLongException
        Config<String> invalidSetting(
            "key",
            "category_too_long", // 14 characters → Exception
            "Test Setting",
            "value"
        );
        invalidSetting.getKey();
    }
    catch (const KeyTooLongException &e) {
        exceptionThrown = true;
    }
    TEST_ASSERT_TRUE(exceptionThrown);
}

void test_key_truncation_warning() {
    bool warningThrown = false;
    try {
        // Name > available length should throw KeyTruncatedWarning
        Config<String> longKeySetting(
            "very_long_key_name_that_exceeds_limit",
            "cfg", // 3 characters → max name length = 11 (15 - 3 - 1)
            "Test Setting",
            "value"
        );
        longKeySetting.getKey(); // Should trigger warning
    }
    catch (const KeyTruncatedWarning &e) {
        warningThrown = true;
    }
    TEST_ASSERT_TRUE(warningThrown);
}


void test_ap_mode_initialization() {
    testManager.startAccessPoint("TestAP", "password");
    TEST_ASSERT_EQUAL(WIFI_AP, WiFi.getMode());
}

void test_sta_mode_initialization() {
    testManager.loadAll();
    //testManager.startWebServer("192.168.2.126", "255.255.255.0", "192.168.0.250" , wifiSsid.get(), wifiPassword.get());
    testManager.startWebServer(wifiSsid.get(), wifiPassword.get());
    testManager.handleClient();
    TEST_ASSERT_EQUAL(WIFI_STA, WiFi.getMode());
}

// Test callback function
bool callbackCalled = false;
void testCallback(int val) { callbackCalled = true; }
Config<int> testCb("cb", "cfg", "Test Callback", 0, true, false, testCallback);

// Basic functionality tests
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

void test_password_masking() {
    DynamicJsonDocument doc(256);
    JsonObject obj = doc.to<JsonObject>();
    JsonObject authObj = obj.createNestedObject("auth");
    testPassword.toJSON(authObj);

    // Check the nested structure
    JsonObject pwdObj = authObj["pwd"];
    TEST_ASSERT_EQUAL_STRING("***", pwdObj["value"].as<const char*>());
    TEST_ASSERT_TRUE(pwdObj["isPassword"].as<bool>());
}

void test_callback_function() {
    callbackCalled = false;
    testCb.set(10);
    TEST_ASSERT_TRUE(callbackCalled);
}

// Test display name functionality
void test_display_name() {
    TEST_ASSERT_EQUAL_STRING("Test Integer", testInt.getDisplayName());
    TEST_ASSERT_EQUAL_STRING("Test Password", testPassword.getDisplayName());
}

void setup() {
    delay(2000);

    Serial.begin(115200);
    while (!Serial);
    disableCore0WDT();

    ConfigManagerClass::setLogger([](const char *msg){
           Serial.print("[test] ");
           Serial.println(msg); });

    UNITY_BEGIN();

    // Add settings to manager
    testManager.addSetting(&wifiSsid);
    testManager.addSetting(&wifiPassword);
    testManager.addSetting(&useDhcp);
    testManager.addSetting(&testInt);
    testManager.addSetting(&testBool);
    testManager.addSetting(&testString);
    testManager.addSetting(&testFloat);
    testManager.addSetting(&testPassword);
    testManager.addSetting(&testCb);

    // Run tests
    RUN_TEST(test_int_config);
    RUN_TEST(test_bool_config);
    RUN_TEST(test_string_config);
    RUN_TEST(test_float_config);
    RUN_TEST(test_password_masking);
    RUN_TEST(test_callback_function);
    RUN_TEST(test_display_name);
    RUN_TEST(test_key_too_long_exception);
    RUN_TEST(test_key_truncation_warning);
    RUN_TEST(test_ap_mode_initialization);
    RUN_TEST(test_sta_mode_initialization);

    UNITY_END();
}

void loop() {
    while (1) {
        delay(1);
    }
}