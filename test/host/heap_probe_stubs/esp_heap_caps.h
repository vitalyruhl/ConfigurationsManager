#pragma once
// [MOCKED!] Heap-query API only; production heap behavior is not simulated.
#include <cstddef>
#include <cstdint>
constexpr uint32_t MALLOC_CAP_DEFAULT = 1u << 12;
struct multi_heap_info_t {
  size_t total_free_bytes, total_allocated_bytes, largest_free_block,
    minimum_free_bytes, allocated_blocks, free_blocks, total_blocks;
};
void heap_caps_get_info(multi_heap_info_t*, uint32_t);
bool heap_caps_check_integrity_all(bool);
