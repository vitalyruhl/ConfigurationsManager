

#include <Arduino.h>
#include <unity.h>
#include <ConfigManager.h>

ConfigManagerClass testManager;

// Main Test
Config<int> testInt("tInt", "cfg", 42);
Config<bool> testBool("tBool", "cfg", true);
Config<String> testString("tStr", "cfg", "def");
Config<float> testFloat("tFlt", "cfg", 3.14f);
Config<String> testPassword("pwd", "auth", "secret", true, true);

// Test Callback function on change value
// This is a small test callback function that sets a boolean flag when called
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
  
  UNITY_END();
}

void loop() {}