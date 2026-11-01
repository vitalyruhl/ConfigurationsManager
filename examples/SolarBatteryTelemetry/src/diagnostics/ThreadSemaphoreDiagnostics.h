#pragma once
#include <cstddef>
#include <cstdint>
#ifndef SBT_ENABLE_HEAP_DIAGNOSTICS
#define SBT_ENABLE_HEAP_DIAGNOSTICS 0
#endif
#if SBT_ENABLE_HEAP_DIAGNOSTICS && defined(ESP32)
namespace sbt { namespace diagnostics {
void recordAllocationFailure(size_t size, uint32_t caps, const char* api, void* caller);
} }
#endif
