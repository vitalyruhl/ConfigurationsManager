

#include <Arduino.h>
#include <unity.h>
#include <ConfigManager.h>
#include <exception>

ConfigManagerClass testManager;

// todo: how can i test the web-UI?
        // todo: Add test for Web-AP (Save, Load, Reboot, etc.)
        

// Main Test
Config<int> testInt("tInt", "cfg", 42);
Config<bool> testBool("tBool", "cfg", true);
Config<String> testString("tStr", "cfg", "def");
Config<float> testFloat("tFlt", "cfg", 3.14f);
Config<String> testPassword("pwd", "auth", "secret", true, true);


// Exception-Tests
void test_key_too_long_exception() {
    bool exceptionThrown = false;
    try {
        Config<String> invalidSetting(
            "very_long_key_name_that_exceeds_limit", 
            "also_too_long_category", 
            "value"
        );
    } catch(const KeyTooLongException& e) {
        exceptionThrown = true;
    }
    TEST_ASSERT_TRUE(exceptionThrown);
}

void test_key_truncation_warning() {
    bool warningThrown = false;
    try {
        Config<String> truncatedSetting(
            "category",
            "key_name_that_is_too_long_but_gets_truncated",
            "value"
        );
    } catch(const KeyTruncatedWarning& e) {
        warningThrown = true;
    }
    TEST_ASSERT_TRUE(warningThrown);
}

// Logger-Callback Test
bool loggerCalled = false;
void testLogger(const char* msg) {
    loggerCalled = true;
}

void test_logger_callback() {
    testManager.setLogger(testLogger);
    testManager.log_message("Test message");
    TEST_ASSERT_TRUE(loggerCalled);
}

// WiFi-Mode Tests
void test_ap_mode_initialization() {
    testManager.startAccessPoint("TestAP", "password");
    TEST_ASSERT_EQUAL(WIFI_AP, WiFi.getMode());
}

void test_sta_mode_initialization() {
    testManager.startWebServer("TestSSID", "password");
    TEST_ASSERT_EQUAL(WIFI_STA, WiFi.getMode());
}

// Test Callback function on change value
bool callbackCalled = false;
void testCallback(int val) { callbackCalled = true; }
Config<int> testCb("cb", "cfg", 0, true, false, testCallback);


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
  testPassword.toJSON(obj);
  TEST_ASSERT_EQUAL_STRING("***", obj["pwd"]);
}

void test_callback_function() {
  callbackCalled = false;
  testCb.set(10);
  TEST_ASSERT_TRUE(callbackCalled);
}

void setup() {
  delay(2000);
  Serial.begin(115200);

  UNITY_BEGIN();
  testManager.addSetting(&testInt);
  testManager.addSetting(&testBool);
  testManager.addSetting(&testString);
  testManager.addSetting(&testFloat);
  testManager.addSetting(&testPassword);
  testManager.addSetting(&testCb);

  RUN_TEST(test_int_config);
  RUN_TEST(test_bool_config);
  RUN_TEST(test_string_config);
  RUN_TEST(test_float_config);
  RUN_TEST(test_password_masking);
  RUN_TEST(test_callback_function);

   // new tests
  RUN_TEST(test_key_too_long_exception);
  RUN_TEST(test_key_truncation_warning);
  RUN_TEST(test_logger_callback);
  RUN_TEST(test_ap_mode_initialization);
  RUN_TEST(test_sta_mode_initialization);


  UNITY_END();
}

void loop() {}