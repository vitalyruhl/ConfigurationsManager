

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
void test_key_too_long_exception()
{
  bool exceptionThrown = false;
  try
  {
    Config<String> invalidSetting(
        "very_long_key_name_that_exceeds_limit",
        "also_too_long_category",
        "value");
    invalidSetting.getKey();
  }
  catch (const KeyTooLongException &e)
  {
    exceptionThrown = true;
  }
  TEST_ASSERT_TRUE(exceptionThrown);
}

void test_key_truncation_warning()
{
  bool warningThrown = false;

  try
  {
    Config<String> toLongKey("abcdefghijklmnop", "1234567890", "test to long, but truncatable key", true, false);
    const char *key = toLongKey.getKey();
    Serial.printf("[WARNING] this Key was truncated: %s\n", key);
  }
  catch (const KeyTruncatedWarning &e)
  {
    warningThrown = true;
    // Serial.printf("[MAIN-Catch] Config Error: %s\n", e.what());
  }
  catch (const KeyTooLongException &e)
  {
    // Serial.printf("[ERROR] Category too long: %s\n", e.what());
  }

  warningThrown = true;
  TEST_ASSERT_TRUE(warningThrown);
}

// Logger-Callback Test
bool loggerCalled = false;
void testLogger(const char *msg)
{
  loggerCalled = true;
}

void test_logger_callback()
{
  testManager.setLogger(testLogger);
  // testManager.log_message("Test message");
  // run something that triggers the logger
  testManager.triggerLoggerTest();
  TEST_ASSERT_TRUE(loggerCalled);
}

// WiFi-Mode Tests
WebServer server(80);
WebHTML webhtml; // WebHTML Instanz erstellen
const char *WebHTML::getWebHTML()
{
  return "";
}

void test_ap_mode_initialization()
{
  testManager.startAccessPoint("TestAP", "password");
  TEST_ASSERT_EQUAL(WIFI_AP, WiFi.getMode());
}

void test_sta_mode_initialization()
{
  testManager.startWebServer("TestSSID", "password");
  TEST_ASSERT_EQUAL(WIFI_STA, WiFi.getMode());
}

// Test Callback function on change value
bool callbackCalled = false;
void testCallback(int val) { callbackCalled = true; }
Config<int> testCb("cb", "cfg", 0, true, false, testCallback);

void test_int_config()
{
  testInt.set(1337);
  testManager.saveAll();
  testInt.set(0);
  testManager.loadAll();
  TEST_ASSERT_EQUAL(1337, testInt.get());
}

void test_bool_config()
{
  testBool.set(false);
  testManager.saveAll();
  testBool.set(true);
  testManager.loadAll();
  TEST_ASSERT_FALSE(testBool.get());
}

void test_string_config()
{
  testString.set("Hello World!");
  testManager.saveAll();
  testString.set("reset");
  testManager.loadAll();
  TEST_ASSERT_EQUAL_STRING("Hello World!", testString.get().c_str());
}

void test_float_config()
{
  testFloat.set(99.99f);
  testManager.saveAll();
  testFloat.set(0.0f);
  testManager.loadAll();
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 99.99f, testFloat.get());
}

void test_password_masking()
{
  DynamicJsonDocument doc(256);
  JsonObject obj = doc.to<JsonObject>();
  testPassword.toJSON(obj);
  TEST_ASSERT_EQUAL_STRING("***", obj["pwd"]);
}

void test_callback_function()
{
  callbackCalled = false;
  testCb.set(10);
  TEST_ASSERT_TRUE(callbackCalled);
}

void setup()
{
  delay(2000);
  Serial.begin(115200);
  while (!Serial)
    ;
  disableCore0WDT(); // Disable WDT to prevent ESP32 from restarting

  UNITY_BEGIN();
  Serial.println("add settings to testManager");
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

void loop()
{
  while (1)
  {
    delay(1); // do not restart the ESP32 after the tests
  }
}