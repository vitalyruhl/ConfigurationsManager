#include <Arduino.h>
#include <unity.h>
#include <ConfigManager.h>

ConfigManagerClass testManager;
Config<int> testInt("test", "category", 42);

void test_config_storage() {
  testManager.addSetting(&testInt);
  testInt.set(1337);
  testManager.saveAll();
  testInt.set(0);
  testManager.loadAll();
  TEST_ASSERT_EQUAL(1337, testInt.get());
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_config_storage);
  UNITY_END();
}

void loop() {}