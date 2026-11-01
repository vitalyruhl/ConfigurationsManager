#pragma once
#include <ArduinoJson.h>
#include <cstdint>
#include <cstring>
namespace control {
// Fixed bump arena for short-lived JSON parsing; keeps MQTT receive off the heap.
// Reset before each parse; exhaustion surfaces as NoMemory and marks the input invalid.
class JsonArena final : public ArduinoJson::Allocator {
public:
    void reset() { used_ = 0; }
    void *allocate(size_t size) override {
        const size_t need = kHeader + align(size);
        if (need > sizeof(buffer_) - used_) return nullptr;
        uint8_t *block = buffer_ + used_;
        *reinterpret_cast<uint32_t *>(block) = uint32_t(size);
        used_ += need;
        return block + kHeader;
    }
    void deallocate(void *) override {}
    void *reallocate(void *ptr, size_t size) override {
        if (!ptr) return allocate(size);
        uint8_t *block = static_cast<uint8_t *>(ptr) - kHeader;
        const uint32_t old = *reinterpret_cast<uint32_t *>(block);
        const size_t start = size_t(block - buffer_);
        if (start + kHeader + align(old) == used_) {
            if (start + kHeader + align(size) > sizeof(buffer_)) return nullptr;
            used_ = start + kHeader + align(size);
            *reinterpret_cast<uint32_t *>(block) = uint32_t(size);
            return ptr;
        }
        if (size <= old) return ptr;
        void *fresh = allocate(size);
        if (fresh) std::memcpy(fresh, ptr, old);
        return fresh;
    }
private:
    static constexpr size_t kHeader = 8;
    static size_t align(size_t size) { return (size + 7u) & ~size_t(7u); }
    alignas(8) uint8_t buffer_[4096];
    size_t used_ = 0;
};

}
