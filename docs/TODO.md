# TODO / Roadmap / Refactor Notes (Planning)

## Terminology

- Top buttons: Live / Settings / Flash / Theme (always visible)
- Settings:
  - SettingsPage = tab in Settings (e.g. "MQTT-Topics")
  - SettingsCard = big card container with title one page can have multiple cards
  - SettingsGroup = group box inside a card (contains fields; shown stacked vertically)
- Live/Runtime:
  - LivePage = top-level page selector under "Live" (similar to Settings tabs)
  - LiveCard = card container inside a live page
  - LiveGroup = if we want a second nesting level like Settings

## High Priority (Prio 1) - Proposed API vNext (Draft)

- Add new stability check:

  ´´´cpp

      static bool ensureNvsReady()
      {
          esp_err_t err = nvs_flash_init();
          if (err == ESP_OK)
          {
              return true;
          }

          if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND || err == ESP_ERR_NVS_INVALID_STATE)
          {
              const esp_err_t eraseErr = nvs_flash_erase();
              if (eraseErr != ESP_OK)
              {
                  Serial.printf("[E] NVS erase failed (%d)\n", static_cast<int>(eraseErr));
                  return false;
              }

              err = nvs_flash_init();
              if (err == ESP_OK)
              {
                  return true;
              }
          }

          Serial.printf("[E] NVS init failed (%d)\n", static_cast<int>(err));
          return false;
      }

  ´´´

## Medium Priority (Prio 5)

- no items yet

## Low Priority (Prio 10)

- SD card modulle + logging (CSV) on SD card (e.g. for data logging, or for storing config files)
- IOManager improvements (PWM/LEDC backend, ramping, fail-safe states)
- Headless mode (no HTTP server)
- GUI: add simple Trend for DI/AI values

## need a check / review

- check other mqtt libraries for better API design (e.g. async, callback-based, etc.) AND QOS support!
 - https://registry.platformio.org/libraries/elims/PsychicMqttClient

## Done / Resolved

- New API design and refactor

## Status Notes

- stable
