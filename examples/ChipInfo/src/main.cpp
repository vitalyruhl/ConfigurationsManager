#include <Arduino.h>

// ESP-IDF headers available inside Arduino-ESP32:
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_heap_caps.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

// ------------------------------ helpers ------------------------------

static const char* resetReasonToStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_UNKNOWN:   return "UNKNOWN";
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT (external pin reset)";
    case ESP_RST_SW:        return "SW (software reset)";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT (other)";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "???";
  }
}

static void printMac(const char* label, esp_mac_type_t type) {
  uint8_t mac[6] = {0};
  if (esp_read_mac(mac, type) == ESP_OK) { // reads MACs for WiFi/BT etc. :contentReference[oaicite:1]{index=1}
    Serial.printf("[MAC ] %-12s %02X:%02X:%02X:%02X:%02X:%02X\n",
                  label, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    Serial.printf("[MAC ] %-12s (not available)\n", label);
  }
}

static const char* partTypeToStr(esp_partition_type_t t) {
  switch (t) {
    case ESP_PARTITION_TYPE_APP:  return "APP";
    case ESP_PARTITION_TYPE_DATA: return "DATA";
    default:                      return "OTHER";
  }
}

// ------------------------------ main dump ------------------------------

static void printChipDumpOnce() {
  Serial.println();
  Serial.println("[CHIP] =================================================================");

  // Build info
  Serial.printf("[BUILD] Date/Time: %s %s\n", __DATE__, __TIME__);
#ifdef ARDUINO
  Serial.printf("[BUILD] Arduino: %lu\n", (unsigned long)ARDUINO);
#endif

  // Basic chip info (Arduino + IDF)
  esp_chip_info_t chipInfo;
  esp_chip_info(&chipInfo); // official IDF API :contentReference[oaicite:2]{index=2}

  Serial.printf("[CHIP ] Model: %s\n", ESP.getChipModel());
  Serial.printf("[CHIP ] Revision: %d\n", ESP.getChipRevision());
  Serial.printf("[CHIP ] Cores: %d\n", chipInfo.cores);
  Serial.printf("[CHIP ] CPU frequency: %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("[CHIP ] SDK: %s\n", ESP.getSdkVersion());

  Serial.print("[CHIP ] Features:");
  if (chipInfo.features & CHIP_FEATURE_WIFI_BGN) Serial.print(" WiFi");
  if (chipInfo.features & CHIP_FEATURE_BT)       Serial.print(" BT");
  if (chipInfo.features & CHIP_FEATURE_BLE)      Serial.print(" BLE");
  if (chipInfo.features & CHIP_FEATURE_EMB_FLASH)Serial.print(" EmbeddedFlash");
  Serial.println();

  // Reset reason
  esp_reset_reason_t rr = esp_reset_reason();
  Serial.printf("[RST  ] Reset reason: %s (%d)\n", resetReasonToStr(rr), (int)rr);

  // Uptime
  int64_t us = esp_timer_get_time();
  Serial.printf("[TIME ] Uptime: %lld us (%.3f s)\n", (long long)us, (double)us / 1e6);

  // Flash (Arduino ESP helpers)
  Serial.printf("[FLASH] Chip size: %u bytes\n", ESP.getFlashChipSize());
#if defined(ESP_ARDUINO_VERSION_MAJOR) || defined(ESP_ARDUINO_VERSION)
  // These exist in Arduino-ESP32 in most versions, but guarded anyway
  Serial.printf("[FLASH] Chip speed: %u Hz\n", ESP.getFlashChipSpeed());
  Serial.printf("[FLASH] Chip mode: %u\n", (unsigned)ESP.getFlashChipMode()); // enum (QIO/QOUT/DIO/DOUT)
#endif

  // Sketch / flash usage
  Serial.printf("[APP  ] Sketch size: %u bytes\n", ESP.getSketchSize());
  Serial.printf("[APP  ] Free sketch space: %u bytes\n", ESP.getFreeSketchSpace());

  // Heap summary + detailed caps (IDF heap_caps) :contentReference[oaicite:3]{index=3}
  Serial.printf("[HEAP ] Heap size: %u bytes\n", ESP.getHeapSize());
  Serial.printf("[HEAP ] Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("[HEAP ] Minimum ever free heap: %u bytes\n", esp_get_minimum_free_heap_size());

  Serial.printf("[HEAP ] Free (INTERNAL): %u\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.printf("[HEAP ] Free (DMA):      %u\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
  Serial.printf("[HEAP ] Free (8BIT):     %u\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));

  // PSRAM
  Serial.printf("[PSRAM] Size: %u bytes\n", ESP.getPsramSize());
  Serial.printf("[PSRAM] Free: %u bytes\n", ESP.getFreePsram());

  // MACs (more than only eFuse MAC)
  {
    const uint64_t mac = ESP.getEfuseMac();
    Serial.printf("[MAC ] eFuse MAC: %04X%08X\n", (uint16_t)(mac >> 32), (uint32_t)mac);
  }
  printMac("WiFi STA", ESP_MAC_WIFI_STA);
  printMac("WiFi AP",  ESP_MAC_WIFI_SOFTAP);
  printMac("BT",       ESP_MAC_BT);
  printMac("ETH",      ESP_MAC_ETH);

  // OTA / running partition info (IDF OTA APIs) :contentReference[oaicite:4]{index=4}
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running) {
    Serial.printf("[OTA  ] Running partition: label=%s type=%s subtype=0x%02X addr=0x%08lX size=%lu\n",
                  running->label,
                  partTypeToStr(running->type),
                  (unsigned)running->subtype,
                  (unsigned long)running->address,
                  (unsigned long)running->size);
  } else {
    Serial.println("[OTA  ] Running partition: (not available)");
  }

  const esp_partition_t* boot = esp_ota_get_boot_partition();
  if (boot) {
    Serial.printf("[OTA  ] Boot partition:    label=%s addr=0x%08lX size=%lu\n",
                  boot->label, (unsigned long)boot->address, (unsigned long)boot->size);
  }

  // Partition table dump (lists all partitions) :contentReference[oaicite:5]{index=5}
  Serial.println("[PART ] Partitions:");
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, nullptr);
  if (!it) {
    Serial.println("[PART ]   (no partitions found)");
  } else {
    while (it) {
      const esp_partition_t* p = esp_partition_get(it);
      if (p) {
        Serial.printf("[PART ]   %-6s subtype=0x%02X label=%-16s addr=0x%08lX size=%-8lu enc=%d\n",
                      partTypeToStr(p->type),
                      (unsigned)p->subtype,
                      p->label,
                      (unsigned long)p->address,
                      (unsigned long)p->size,
                      (int)p->encrypted);
      }
      it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);
  }

  Serial.println("[CHIP] =================================================================");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  printChipDumpOnce();
}

void loop() {
  delay(1000);
}
