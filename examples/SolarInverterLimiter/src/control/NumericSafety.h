#pragma once
#include <climits>
#include <cmath>
#include <cstdint>

namespace control {
inline bool checkedFloatToInt(float value, int &out) {
    // Compare as double: float(INT_MAX) rounds up on the 32-bit target.
    const double widened = double(value);
    if (!std::isfinite(widened) || widened < double(INT_MIN) || widened > double(INT_MAX)) return false;
    out = static_cast<int>(value);
    return true;
}

inline bool checkedIntegerToFloat(double value, float &out) {
    if (!std::isfinite(value) || value < double(INT_MIN) || value > double(INT_MAX)) return false;
    const float stored = float(static_cast<int>(value));
    int checked = 0;
    if (!checkedFloatToInt(stored, checked)) return false;
    out = stored;
    return true;
}

inline int saturatingAdd(int left, int right) {
    const int64_t sum = int64_t(left) + int64_t(right);
    if (sum > INT_MAX) return INT_MAX;
    if (sum < INT_MIN) return INT_MIN;
    return static_cast<int>(sum);
}

inline int nonnegativeExportMagnitude(int signedGridW) {
    if (signedGridW >= 0) return 0;
    if (signedGridW == INT_MIN) return INT_MAX;
    return -signedGridW;
}
} // namespace control
