#pragma once
#include "../ConfigManagerConfig.h"

#if CM_ENABLE_VERBOSE_LOGGING && defined(ESP32)
#include <esp_heap_caps.h>
#include <rom/ets_sys.h>

namespace cm {
// Use ROM output and fixed storage so the diagnostic bypasses allocating log
// tags, JSON and Arduino Strings. Samples describe this instant, not an atomic
// view across other tasks; integrity checking and UART output affect timing.
inline void traceHttpHeap(const char* route, const char* stage) {
  multi_heap_info_t info{};
  heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
  const bool intact = heap_caps_check_integrity_all(false);
  ets_printf("[HTTP-HEAP] route=%s stage=%s free=%u largest=%u min=%u blocks=%u integrity=%s\n",
             route, stage, static_cast<unsigned>(info.total_free_bytes),
             static_cast<unsigned>(info.largest_free_block),
             static_cast<unsigned>(info.minimum_free_bytes),
             static_cast<unsigned>(info.free_blocks), intact ? "ok" : "CORRUPT");
}
} // namespace cm
#define CM_HTTP_HEAP_TRACE(route, stage) ::cm::traceHttpHeap(route, stage)
#else
// Remove argument literals as well as the probe when verbose logging is off.
#define CM_HTTP_HEAP_TRACE(route, stage) do {} while (0)
#endif
