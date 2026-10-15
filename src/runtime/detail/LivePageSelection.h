#pragma once
#include <cstddef>
#include <cstring>

namespace cm {
inline bool validLivePageSelector(const char* key, size_t length) {
  return key && length && length <= 256 && std::memchr(key, 0, length) == nullptr;
}
template <typename Placement>
bool sameLiveField(const Placement& placement, const char* key, const char* source) {
  return std::strcmp(placement.id.c_str(), key) == 0 && std::strcmp(placement.sourceGroup.c_str(), source) == 0;
}
// Empty-source placements are the existing explicit/global override API.
// Last matching placement retains the established override order.
template <typename Placements>
const typename Placements::value_type* findLivePlacement(const Placements& placements, const char* key, const char* source) {
  const typename Placements::value_type* selected = nullptr;
  for (const auto& placement : placements) {
    if (std::strcmp(placement.id.c_str(), key) == 0 &&
        (!source[0] || !placement.sourceGroup.length() || std::strcmp(placement.sourceGroup.c_str(), source) == 0)) selected = &placement;
  }
  return selected;
}
} // namespace cm
