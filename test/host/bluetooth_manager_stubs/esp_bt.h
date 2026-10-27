#pragma once
// [MOCKED!] Native controller state for terminal worker shutdown checks.
enum esp_bt_controller_status_t { ESP_BT_CONTROLLER_STATUS_IDLE,
                                  ESP_BT_CONTROLLER_STATUS_INITED,
                                  ESP_BT_CONTROLLER_STATUS_ENABLED };
inline esp_bt_controller_status_t nativeControllerStatus = ESP_BT_CONTROLLER_STATUS_IDLE;
inline esp_bt_controller_status_t esp_bt_controller_get_status() {
  return nativeControllerStatus;
}
