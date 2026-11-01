#pragma once
#include <ConfigManager.h>
#include <type_traits>

namespace sbt {
template<typename T>
T readSettingValue(Preferences& prefs, const char* key, const T& fallback) {
    if constexpr (std::is_same_v<T, String>) return prefs.getString(key, fallback);
    else if constexpr (std::is_same_v<T, bool>) return prefs.getBool(key, fallback);
    else if constexpr (std::is_same_v<T, int>) return prefs.getInt(key, fallback);
    else if constexpr (std::is_same_v<T, float>) return prefs.getFloat(key, fallback);
}

// Only absence permits provisioning. Empty strings, false and zero are saved values.
template<typename T>
bool initializeSettingIfMissing(Preferences& prefs, Config<T>& setting, const T& value) {
    if (setting.hasError()) return false;
    if (prefs.isKey(setting.getKey())) return true;
    size_t written = 0;
    if constexpr (std::is_same_v<T, String>) written = prefs.putString(setting.getKey(), value);
    else if constexpr (std::is_same_v<T, bool>) written = prefs.putBool(setting.getKey(), value);
    else if constexpr (std::is_same_v<T, int>) written = prefs.putInt(setting.getKey(), value);
    else if constexpr (std::is_same_v<T, float>) written = prefs.putFloat(setting.getKey(), value);
    // ESP32 Preferences::putString returns the string length, including zero for
    // a successfully stored empty value. Key existence and readback confirm it.
    bool writeReported = written != 0;
    if constexpr (std::is_same_v<T, String>) writeReported = writeReported || value.length() == 0;
    return writeReported && prefs.isKey(setting.getKey()) &&
        readSettingValue(prefs, setting.getKey(), value) == value;
}

template<typename T>
bool migrateSettingIfMissing(Preferences& prefs, Config<T>& setting, const char* legacyHint) {
    if (setting.hasError()) return false;
    if (prefs.isKey(setting.getKey())) return true;
    const String legacyKey = hashStringForStorage(String(legacyHint));
    if (!prefs.isKey(legacyKey.c_str())) return true;
    return initializeSettingIfMissing(prefs, setting,
        readSettingValue(prefs, legacyKey.c_str(), setting.get()));
}
// One-time owner-authorized upgrade from the old disabled BLE smoke firmware.
// A later user disable is authoritative because the migration marker persists.
inline bool enableReadoutBluetoothOnce(Preferences& prefs, Config<bool>& enabled, bool configured) {
    if (!configured) return true;
    if (enabled.hasError()) return false;
    const String marker = hashStringForStorage(String("SBTReadoutInit"));
    if (prefs.isKey(marker.c_str())) return true;
    if (!prefs.putBool(enabled.getKey(), true) || !prefs.getBool(enabled.getKey(), false)) return false;
    enabled.load(prefs); // Virtual load also synchronizes CM's atomic BLE dispatch state.
    return prefs.putBool(marker.c_str(), true) != 0 && prefs.getBool(marker.c_str(), false);
}
} // namespace sbt
