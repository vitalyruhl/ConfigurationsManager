#pragma once
#include "FreeRTOS.h"
#include "ConfigManager.h"
// [MOCKED!] Execute the real worker synchronously with deterministic task-boundary interleavings.
inline int xTaskCreate(void (*entry)(void*), const char*, unsigned, void* value, int, TaskHandle_t* handle) {
  ble_host::entry = entry;
  ble_host::entryValue = value;
  *handle = value;
  return pdPASS;
}
inline unsigned uxTaskGetStackHighWaterMark(void*) {
  if (ble_host::watermark)
    ble_host::watermark();
  return 4096;
}
inline void vTaskDelay(unsigned) {
  if (ble_host::delay)
    ble_host::delay();
}
