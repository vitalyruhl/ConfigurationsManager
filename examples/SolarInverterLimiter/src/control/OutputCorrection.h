#pragma once
#include <cstdint>

namespace control {
// Apply the persisted output bias once, after regulation. Stops and full-output
// bypass keep their exact command; arithmetic cannot overflow an int offset.
inline int correctOutputCommand(int rawWatts, int offsetWatts, int maximumWatts) {
    if (maximumWatts <= 0 || rawWatts <= 0) return 0;
    if (rawWatts >= maximumWatts) return maximumWatts;
    const int64_t corrected = int64_t(rawWatts) + int64_t(offsetWatts);
    if (corrected <= 0) return 0;
    if (corrected >= maximumWatts) return maximumWatts;
    return static_cast<int>(corrected);
}
} // namespace control
