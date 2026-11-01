#pragma once

#ifndef SBT_ENABLE_HEAP_DIAGNOSTICS
#define SBT_ENABLE_HEAP_DIAGNOSTICS 0
#endif

#if (CM_ENABLE_VERBOSE_LOGGING || SBT_ENABLE_HEAP_DIAGNOSTICS) && defined(ESP32)
#include <atomic>
#include "ThreadSemaphoreDiagnostics.h"
#include <esp_heap_caps.h>
#include <rom/ets_sys.h>

namespace sbt { namespace diagnostics {
// App-owned hook: CM must not replace a consumer's process-wide allocator hook.
// No logging manager, Strings, JSON, integrity walk or allocation in this path.
// Report the first 16 failures, then every 64th large failure. Always retain
// small (<=64-byte) failures so header/new failures remain visible later.
inline void allocationFailed(size_t size, uint32_t caps, const char* api) {
#if SBT_ENABLE_HEAP_DIAGNOSTICS
  recordAllocationFailure(size, caps, api, __builtin_return_address(0));
#endif
  static std::atomic<unsigned> failures{0};
  static std::atomic_flag printing = ATOMIC_FLAG_INIT;
  const unsigned event = failures.fetch_add(1, std::memory_order_relaxed);
  const bool report = event < 16 || size <= 64 || (event % 64 == 63);
  if (!report || printing.test_and_set(std::memory_order_relaxed)) return;
  multi_heap_info_t eligible{}, defaults{};
  heap_caps_get_info(&eligible, caps);
  heap_caps_get_info(&defaults, MALLOC_CAP_DEFAULT);
  ets_printf("[HEAP-FAIL] n=%u size=%u caps=0x%x api=%s free=%u largest=%u defaultFree=%u defaultLargest=%u\n",
             event + 1, static_cast<unsigned>(size), static_cast<unsigned>(caps), api ? api : "unknown",
             static_cast<unsigned>(eligible.total_free_bytes),
             static_cast<unsigned>(eligible.largest_free_block),
             static_cast<unsigned>(defaults.total_free_bytes),
             static_cast<unsigned>(defaults.largest_free_block));
  printing.clear(std::memory_order_relaxed);
}
inline void installHeapFailureDiagnostics() {
  const auto result = heap_caps_register_failed_alloc_callback(allocationFailed);
  if (result != ESP_OK) ets_printf("[HEAP-FAIL] registration failed: %d\n", static_cast<int>(result));
}
} } // namespace sbt::diagnostics
#define SBT_INSTALL_HEAP_FAILURE_DIAGNOSTICS() ::sbt::diagnostics::installHeapFailureDiagnostics()
#else
#define SBT_INSTALL_HEAP_FAILURE_DIAGNOSTICS() do {} while (0)
#endif
