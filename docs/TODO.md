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

## High Priority (Prio 1) - Runtime stability

- [ ] MQTT verbose logging: Investigate and fix the reported TX payload-preview `LoadProhibited` crash (`strlen(nullptr)` via `vsnprintf`), and harden the related RX preview. Use allocation-free, bounded 200-character previews, null-safe formatting and compile-time exclusion when verbose logging is disabled; preserve MQTT behavior. See [the crash evidence and bounded implementation scope](todo_mqtt_verbose_crash.md). Root cause and fix remain unconfirmed.

- [x] Web UI/HTTP/WebSocket iteration: reduce temporary JSON/payload copies, bound HTTP delivery and runtime socket queues, and serialize startup/runtime generation. Normal SIL/SBT access was operator-accepted on 2026-10-22 as faster and without observed crashes. See [the acceptance and preserved investigation history](todo_http_delivery_panic.md). Whole-response output buffers still exist; optional overload/headroom research is deferred, with no claim that earlier failed stress cohorts passed.

- [x] Close the current HTTP delivery/panic work item by operator acceptance (2026-10-22). Preserve historical failures, firmware identities and unproven causes in the linked handoff; this acceptance authorizes neither a release nor main integration. The separate WT32-ETH01 Ethernet rapid-reload issue #71 is unaffected.

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

- The current HTTP/WebSocket iteration is operator-accepted; optional historical overload research is deferred. The separate MQTT verbose-logging item remains open. CM 4.8.0 release preparation is authorized, subject to green CI and the integration/release gates.
