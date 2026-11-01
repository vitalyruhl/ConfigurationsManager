#pragma once
#include <ArduinoJson.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace sbt {
enum class Kind { Real, Boolean, Unsigned, Text, Unsupported };
struct Channel {
  const char *topic;
  const char *key;
  Kind kind;
  uint8_t source;
};
inline constexpr Channel channels[] = {
    {"bms/charging", "bms_charging", Kind::Boolean, 0},
    {"bms/discharging", "bms_discharging", Kind::Boolean, 0},
    {"bms/state_of_charge_pct", "bms_soc_pct", Kind::Real, 0},
    {"bms/voltage_v", "bms_voltage_v", Kind::Real, 0},
    {"bms/current_a", "bms_current_a", Kind::Real, 0},
    {"bms/power_w", "bms_power_w", Kind::Real, 0},
    {"bms/remaining_capacity_ah", "bms_remaining_ah", Kind::Real, 0},
    {"bms/full_capacity_ah", "bms_full_ah", Kind::Real, 0},
    {"bms/charge_enabled", "bms_charge_enabled", Kind::Boolean, 0},
    {"bms/discharge_enabled", "bms_discharge_enabled", Kind::Boolean, 0},
    {"bms/temperature_max_c", "bms_temperature_max_c", Kind::Real, 0},
    {"bms/cell_voltage_min_v", "bms_cell_min_v", Kind::Real, 0},
    {"bms/cell_voltage_max_v", "bms_cell_max_v", Kind::Real, 0},
    {"bms/cell_voltage_delta_v", "bms_cell_delta_v", Kind::Real, 0},
    {"bms/alarm_bits", "bms_alarm_bits", Kind::Unsigned, 0},
    {"victron/solar_power_w", "victron_solar_power_w", Kind::Real, 1},
    {"victron/pv_voltage_v", "victron_pv_voltage_v", Kind::Unsupported, 1},
    {"victron/pv_current_a", "victron_pv_current_a", Kind::Unsupported, 1},
    {"victron/battery_voltage_v", "victron_battery_voltage_v", Kind::Real, 1},
    {"victron/charge_current_a", "victron_charge_current_a", Kind::Real, 1},
    {"victron/battery_power_w", "victron_battery_power_w", Kind::Unsupported,1},
    {"victron/load_current_a", "victron_load_current_a", Kind::Real, 1},
    {"victron/charge_state", "victron_charge_state", Kind::Text, 1},
    {"victron/error_code", "victron_error_code", Kind::Unsigned, 1},
    {"victron/yield_today_wh", "victron_yield_today_wh", Kind::Real, 1},
    {"bms/temperature_min_c", "bms_temperature_min_c", Kind::Real, 0},
    {"bms/cell_count", "bms_cell_count", Kind::Unsigned, 0},
    {"bms/temperature_sensor_count", "bms_temperature_sensor_count",
     Kind::Unsigned, 0},
    {"bms/protection", "bms_protection", Kind::Text, 0},
    {"bms/temperature_1_c", "bms_temperature_1_c", Kind::Real, 0},
    {"bms/temperature_2_c", "bms_temperature_2_c", Kind::Real, 0}};
inline constexpr size_t channelCount = sizeof(channels) / sizeof(channels[0]);
static_assert(channelCount <= 32, "Diagnostic channel mask must fit uint32_t");
class Receiver {
public:
  static constexpr uint32_t timeoutMs = 180000;
  static constexpr uint32_t acquisitionMs = 30000;
  void setTimeoutSeconds(unsigned seconds) {
    deadlineMs_ = (seconds >= 1 && seconds <= 9999 ? seconds : 180) * 1000u;
  }
  uint32_t configuredTimeoutMs() const { return deadlineMs_; }
  struct Health {
    bool online = false, available = false, seeded = false, advanced = false, timestamped = false;
    uint32_t sequence = 0, received = 0, age = 0;
    uint32_t heldReceived = 0, heldAge = 0, displayReceived = 0, displayAge = 0;
    bool heldKnown = false, displayKnown = false;
    char reason[64] = "not_received";
  };
  void reset() {
    const uint32_t deadline = deadlineMs_;
    const bool emergencyAttempted = emergencyAttempted_;
    const bool dischargeAlarm = alarmRestricted_, chargeAlarm = chargeRestricted_;
    const uint32_t dischargeBits = dischargeRestrictionBits_, chargeBits = chargeRestrictionBits_;
    *this = Receiver{};
    deadlineMs_ = deadline;
    emergencyAttempted_ = emergencyAttempted;
    alarmRestricted_ = dischargeAlarm; chargeRestricted_ = chargeAlarm;
    dischargeRestrictionBits_ = dischargeBits; chargeRestrictionBits_ = chargeBits;
  }
  // Transport loss revokes permission, while retaining the original display data.
  void disconnect() {
    online_ = remoteReady_ = controlAvailable_ = permissionsKnown_ = false;
    for (unsigned source = 0; source < 2; ++source) fence(source);
  }
  bool alarmRestricted() const { return alarmRestricted_; }
  bool softwareMosLockOnly() const { return dischargeRestrictionBits_ == (1u << 12); }
  bool genuineAlarmRestricted() const { return ((dischargeRestrictionBits_ | chargeRestrictionBits_) & ~(1u << 12)) != 0; }
  // An open discharge MOS is physical feedback, not a veto on the visible inverter intent.
  bool batteryDischargeIntentAvailable(uint32_t now) const {
    return valid("bms/alarm_bits", now) && valid("bms/discharge_enabled", now) &&
        !(dischargeRestrictionBits_ & ~(1u << 12));
  }
  bool batteryPathAvailable(uint32_t now) const {
    return batteryDischargeIntentAvailable(now) && numeric("bms/discharge_enabled") != 0;
  }
  // A battery-powered MPPT can disappear with the actual discharge MOS OFF.
  // This explains missing samples; it never makes its voltage/current fresh or zero.
  bool chargerStandby(uint32_t now) const {
    const double fault = cachedNumeric("victron/error_code");
    return !fresh(1, now) && !genuineAlarmRestricted() &&
        batteryDischargeIntentAvailable(now) && numeric("bms/discharge_enabled") == 0 &&
        valid("bms/state_of_charge_pct", now) && valid("bms/power_w", now) &&
        (!std::isfinite(fault) || fault == 0);
  }
  // Reserving PV does not force a BMS/charger write or claim future stored energy.
  bool batteryChargeAvailable(uint32_t now) const {
    return valid("bms/alarm_bits", now) && valid("bms/charge_enabled", now) && numeric("bms/charge_enabled") != 0 &&
        !(chargeRestrictionBits_ & ~(1u << 12));
  }
  bool batteryChargeConfirmed(uint32_t now) const {
    return batteryChargeAvailable(now);
  }
  bool chargeRestricted() const { return chargeRestricted_; }
  bool hasSample(const char *topic) const {
    const int i = index(topic);
    return i >= 0 && values_[i].seen && values_[i].eligible;
  }
  bool old(unsigned source, uint32_t now) const {
    if (source > 1 || !health_[source].displayKnown) return false;
    const auto &h = health_[source];
    return h.displayAge >= deadlineMs_ || uint32_t(now - h.displayReceived) >= deadlineMs_ - h.displayAge;
  }
  bool withinGrace(unsigned source, uint32_t now) const {
    if (source > 1) return false;
    const auto &h = health_[source];
    return h.heldKnown && h.heldAge < deadlineMs_ &&
        uint32_t(now - h.heldReceived) < deadlineMs_ - h.heldAge;
  }
  bool commandsLocked(uint32_t now = 0) const { return !emergencyOffEligible(now); }
  bool emergencyOffEligible(uint32_t now) const {
    return !emergencyAttempted_ && remoteEnabled_ && remoteReady_ && !remotePending_ && controlAvailable_ &&
        hexSession(session_) && valid("bms/alarm_bits", now) && (uint32_t(numeric("bms/alarm_bits")) & 0xfffu);
  }
  static constexpr const char* emergencyOffTopic() { return "bms/emergency_off/set"; }
  // Caller owns synchronization. Consume before publishing: even an uncertain send is never retried.
  bool takeEmergencyOff(uint32_t now, uint64_t utc, const char* requestId, char* output, size_t capacity) {
    if (!emergencyOffEligible(now) || !previewDisable(false,requestId,utc,output,capacity)) return false;
    emergencyAttempted_ = true;
    return true;
  }
  bool fresh(unsigned source, uint32_t now) const {
    if (source > 1)
      return false;
    const auto &h = health_[source];
    return online_ && h.online && h.available && h.advanced && h.age < deadlineMs_ &&
           uint32_t(now - h.received) < deadlineMs_ - h.age &&
           h.age < acquisitionMs && uint32_t(now - h.received) < acquisitionMs - h.age;
  }
  bool valid(const char *topic, uint32_t now) const {
    int i = index(topic);
    return i >= 0 && hasSample(topic) && fresh(channels[i].source, now);
  }
  uint32_t sequence(unsigned source) const {
    return source < 2 ? health_[source].sequence : 0;
  }
  Health sourceHealth(unsigned source) const {
    return source < 2 ? health_[source] : Health{};
  }
  void diagnosticValues(uint32_t now, double *values, uint32_t &validMask) const {
    validMask = 0;
    for (size_t i = 0; i < channelCount; ++i) {
      values[i] = 0;
      if (channels[i].kind == Kind::Text || channels[i].kind == Kind::Unsupported ||
          !valid(channels[i].topic, now)) continue;
      values[i] = values_[i].number;
      validMask |= uint32_t(1) << i;
    }
  }
  bool copyDiagnosticText(const char *topic, uint32_t now, char *out, size_t capacity) const {
    const int i = index(topic);
    if (i < 0 || !out || !capacity || !valid(topic, now) || channels[i].kind != Kind::Text) return false;
    const char *value = i == 28 ? protection_ : chargeState_;
    const int n = std::snprintf(out, capacity, "%s", value);
    return n >= 0 && size_t(n) < capacity;
  }
  double cachedNumeric(const char *topic) const {
    const int i = index(topic);
    return i >= 0 && values_[i].seen && channels[i].kind != Kind::Text ? values_[i].number : NAN;
  }
  double numeric(const char *topic) const {
    int i = index(topic);
    return i >= 0 ? values_[i].number : 0;
  }
  bool receive(const char *topic, const char *payload, size_t length,
               uint32_t now) {
    if (!topic || !payload)
      return false;
    if (length > 1024) return false;
    char text[1025];
    std::memcpy(text, payload, length);
    text[length] = '\0';
    if (std::strlen(text) != length) return false;
    if (!std::strcmp(topic, "status")) {
      if (std::strcmp(text, "online") && std::strcmp(text, "offline"))
        return false;
      if (!std::strcmp(text, "offline")) {
        disconnect();
        return true;
      }
      online_ = true;
      return true;
    }
    for (unsigned s = 0; s < 2; ++s) {
      const char *availability =
          s ? "victron/availability" : "bms/availability";
      if (!std::strcmp(topic, availability)) {
        if (std::strcmp(text, "available") && std::strcmp(text, "unavailable"))
          return false;
        if (!std::strcmp(text, "unavailable")) {
          health_[s].online = false;
        } else
          health_[s].online = true;
        return true;
      }
      if (!std::strcmp(topic, s ? "victron/health" : "bms/health"))
        return receiveHealth(s, text, now);
    }
    if (!std::strcmp(topic, "bms/control/availability")) {
      if (std::strcmp(text, "available") && std::strcmp(text, "unavailable") &&
          std::strcmp(text, "disabled"))
        return false;
      controlAvailable_ = !std::strcmp(text, "available");
      return true;
    }
    if (!std::strcmp(topic, "bms/control")) {
      JsonDocument d;
      auto error = deserializeJson(d, text);
      const char *session = d["session"] | "";
      if (error || !d["enabled"].is<bool>() || !d["ready"].is<bool>() ||
          !d["pending"].is<bool>() || !hexSession(session) ||
          !d["reason"].is<const char *>()) {
        return false;
      }
      bool known = false, requestedCharge = false, requestedDischarge = false;
      if (d["permissions_known"].is<bool>()) {
        known = d["permissions_known"].as<bool>();
        if (known && (!d["requested_charge_enabled"].is<bool>() || !d["requested_discharge_enabled"].is<bool>())) {
          return false;
        }
        if (known) { requestedCharge = d["requested_charge_enabled"].as<bool>(); requestedDischarge = d["requested_discharge_enabled"].as<bool>(); }
      } else if (!d["permissions_known"].isNull()) { return false; }
      // The first explicit session also invalidates any earlier retained health high-water.
      if (std::strcmp(session_, session)) {
        for (unsigned source = 0; source < 2; ++source) {
          fence(source);
          health_[source].seeded = false;
        }
      }
      std::snprintf(session_, sizeof(session_), "%s", session);
      permissionsKnown_ = known; requestedCharge_ = requestedCharge; requestedDischarge_ = requestedDischarge;
      remoteEnabled_ = d["enabled"];
      remoteReady_ = d["ready"];
      remotePending_ = d["pending"];
      std::snprintf(controlReason_, sizeof(controlReason_), "%s",
                    d["reason"].as<const char *>());
      return true;
    }
    if (!std::strcmp(topic, "bms/control/result")) {
      JsonDocument d;
      if (deserializeJson(d, text) || !d["request_id"].is<const char *>() ||
          !d["phase"].is<const char *>() || !d["reason"].is<const char *>() ||
          length >= sizeof(result_))
        return false;
      std::memcpy(result_, text, length + 1);
      return true;
    }
    const int i = index(topic);
    if (i < 0)
      return false;
    auto &v = values_[i];
    const auto kind = channels[i].kind;
    if (!std::strcmp(text, "null") || kind == Kind::Unsupported) {
      v.eligible = false;
      const bool missingNtc = !std::strcmp(topic, "bms/temperature_1_c") ||
          !std::strcmp(topic, "bms/temperature_2_c");
      if (kind == Kind::Unsupported || missingNtc) v.seen = false;
      if (missingNtc) recordQualified(channels[i].source, now);
      return true;
    }
    if (kind == Kind::Text) {
      char *output = i == 28 ? protection_ : chargeState_;
      const size_t capacity =
          i == 28 ? sizeof(protection_) : sizeof(chargeState_);
      if (length >= capacity)
        return false;
      if (i == 22) {
        JsonDocument d;
        if (deserializeJson(d, text) || !d.is<const char *>())
          return false;
        std::snprintf(output, capacity, "%s", d.as<const char *>());
      } else
        std::memcpy(output, text, length + 1);
      v.seen = v.eligible = true;
      return true;
    }
    JsonDocument d;
    if (deserializeJson(d, text))
      return false;
    double candidate = 0;
    if (kind == Kind::Boolean) {
      if (!d.is<bool>())
        return false;
      candidate = d.as<bool>() ? 1 : 0;
    } else {
      if (!d.is<double>() || d.is<bool>())
        return false;
      candidate = d.as<double>();
      if (!std::isfinite(candidate))
        return false;
      if (kind == Kind::Unsigned && (candidate < 0 || candidate > 4294967295.0 ||
                                     std::floor(candidate) != candidate))
        return false;
    }
    // Match the existing control-snapshot semantics before committing a candidate.
    const bool invalidEssential =
        (!std::strcmp(topic, "bms/state_of_charge_pct") && (candidate < 0 || candidate > 100)) ||
        (!std::strcmp(topic, "victron/battery_voltage_v") && candidate <= 0) ||
        (!std::strcmp(topic, "victron/charge_current_a") && candidate < 0);
    if (invalidEssential) {
      v.eligible = false; // Keep the last-good display value and its original clocks.
      return false;
    }
    v.number = candidate;
    v.seen = v.eligible = true;
    recordQualified(channels[i].source, now);
    if (!std::strcmp(topic, "bms/alarm_bits")) {
      const uint32_t bits = static_cast<uint32_t>(candidate);
      // Only a fresh clear rearms the one-shot emergency; link/session changes do not.
      if (fresh(0, now) && !(bits & 0xfffu)) emergencyAttempted_ = false;
      // JBD protocol directions, not new numeric battery thresholds.
      constexpr uint32_t dischargeMask = 0xffff0000u | (1u<<1) | (1u<<3) | (1u<<6) |
          (1u<<7) | (1u<<9) | (1u<<10) | (1u<<11) | (1u<<12) | (1u<<14) | (1u<<15);
      constexpr uint32_t chargeMask = 0xffff0000u | (1u<<0) | (1u<<2) | (1u<<4) |
          (1u<<5) | (1u<<8) | (1u<<10) | (1u<<11) | (1u<<12) | (1u<<13) | (1u<<14) | (1u<<15);
      // A stale/software-lock packet must not erase a separately latched critical fault.
      if (fresh(0, now)) dischargeRestrictionBits_ = bits & dischargeMask;
      else dischargeRestrictionBits_ |= bits & dischargeMask;
      alarmRestricted_ = dischargeRestrictionBits_ != 0;
      if (fresh(0, now)) chargeRestrictionBits_ = bits & chargeMask;
      else chargeRestrictionBits_ |= bits & chargeMask;
      chargeRestricted_ = chargeRestrictionBits_ != 0;
    }
    return true;
  }
  // Preview only: deliberately has no MQTT transport or publish dependency.
  bool previewDisable(bool charge, const char *requestId, uint64_t now,
                      char *output, size_t capacity) const {
    (void)charge;
    if (!output || !capacity || !hexSession(session_) || !requestId ||
        now < 1700000000ULL || now > 4294967265ULL)
      return false;
    const size_t n = std::strlen(requestId);
    if (!n || n > 32)
      return false;
    for (size_t i = 0; i < n; ++i) {
      char c = requestId[i];
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-'))
        return false;
    }
    const int nout = std::snprintf(
        output, capacity,
        "{\"state\":false,\"request_id\":\"%s\",\"session\":\"%s\","
        "\"expires_at\":%llu}",
        requestId, session_, static_cast<unsigned long long>(now + 30));
    return nout >= 0 && size_t(nout) < capacity;
  }
  static const char *disableTopic(bool charge) {
    return charge ? "bms/charge_enabled/set" : "bms/discharge_enabled/set";
  }
  void writeRuntime(JsonObject out, uint32_t now) const {
    out["online"] = online_;
    out["commands_locked"] = commandsLocked(now);
    out["emergency_state"] = emergencyAttempted_ ? "Attempted; manual retry if needed" :
        emergencyOffEligible(now) ? "Emergency stop ready" : !remoteEnabled_ ? "Control disabled" : "Waiting for a fresh fault";
    out["charge_restriction"] = chargeRestricted_;
    out["discharge_restriction"] = alarmRestricted_;
    out["remote_control_ready"] =
        remoteReady_ && controlAvailable_ && fresh(0, now);
    out["remote_control_enabled"] = remoteEnabled_;
    out["remote_control_pending"] = remotePending_;
    out["remote_control_available"] = controlAvailable_;
    // ArduinoJson treats const arrays as borrowed literals. Force copies while
    // the service mutex is held; HTTP serialization runs after this method.
    out["control_reason"] = static_cast<const char *>(controlReason_);
    out["control_result"] = static_cast<const char *>(result_);
    out["charge_disable_topic"] = disableTopic(true);
    out["discharge_disable_topic"] = disableTopic(false);
    for (unsigned s = 0; s < 2; ++s) {
      const auto &h = health_[s];
      const bool standby = s == 1 && chargerStandby(now);
      out[s ? "victron_fresh" : "bms_fresh"] = fresh(s, now);
      out[s ? "victron_reason" : "bms_reason"] =
          standby ? "standby_discharge_mos_off" : static_cast<const char *>(h.reason);
      out[s ? "victron_sequence" : "bms_sequence"] = h.sequence;
      out[s ? "victron_age" : "bms_age"] = !standby && old(s, now) ? "Old" : "";
      out[s ? "victron_grace" : "bms_grace"] = withinGrace(s, now);
    }
    unsigned received = 0;
    for (size_t i = 0; i < channelCount; ++i) {
      if (values_[i].seen)
        ++received;
      if (!values_[i].seen) {
        out[channels[i].key] = nullptr;
        continue;
      }
      if (channels[i].kind == Kind::Text)
        out[channels[i].key] = i == 28 ? protection_ : chargeState_;
      else if (channels[i].kind == Kind::Boolean)
        out[channels[i].key] = values_[i].number != 0;
      else
        out[channels[i].key] = values_[i].number;
    }
    out["received_fields"] = received;
    out["permissions_known"] = permissionsKnown_;
    if (permissionsKnown_) { out["requested_charge_enabled"] = requestedCharge_; out["requested_discharge_enabled"] = requestedDischarge_; }
    else { out["requested_charge_enabled"] = nullptr; out["requested_discharge_enabled"] = nullptr; }
    out["battery_charge_possible"] = batteryChargeAvailable(now);
    out["battery_charge_confirmed"] = batteryChargeConfirmed(now);
    out["battery_discharge_available"] = batteryPathAvailable(now);
    out["battery_discharge_intent_available"] = batteryDischargeIntentAvailable(now);
  }

private:
  struct Value {
    double number = 0;
    bool seen = false, eligible = false;
  };
  std::array<Value, channelCount> values_{};
  Health health_[2];
  uint32_t deadlineMs_ = timeoutMs;
  bool alarmRestricted_ = false, chargeRestricted_ = false;
  uint32_t dischargeRestrictionBits_ = 0, chargeRestrictionBits_ = 0;
  bool permissionsKnown_ = false, requestedCharge_ = false, requestedDischarge_ = false;
  bool emergencyAttempted_ = false;
  bool online_ = false, remoteReady_ = false, remoteEnabled_ = false,
       remotePending_ = false, controlAvailable_ = false;
  char protection_[512] = "", chargeState_[32] = "", session_[33] = "",
       controlReason_[64] = "not_received", result_[512] = "";
  static int index(const char *topic) {
    for (size_t i = 0; i < channelCount; ++i)
      if (!std::strcmp(topic, channels[i].topic))
        return int(i);
    return -1;
  }
  static bool hexSession(const char *s) {
    if (!s || std::strlen(s) != 32)
      return false;
    for (unsigned i = 0; i < 32; ++i)
      if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f') ||
            (s[i] >= 'A' && s[i] <= 'F')))
        return false;
    return true;
  }
  void fence(unsigned source) {
    for (size_t i = 0; i < channelCount; ++i)
      if (channels[i].source == source) values_[i].eligible = false;
    health_[source].advanced = false;
    health_[source].available = false;
  }
  void recordQualified(unsigned source, uint32_t now) {
    auto &h = health_[source];
    bool allCurrent = true;
    for (size_t i = 0; i < channelCount; ++i)
      if (channels[i].source == source && values_[i].seen && !values_[i].eligible) allCurrent = false;
    if (allCurrent && h.timestamped) {
      h.displayKnown = true; h.displayReceived = h.received; h.displayAge = h.age;
    }
    const bool essential = source ? hasSample("victron/battery_voltage_v") && hasSample("victron/charge_current_a")
        : hasSample("bms/state_of_charge_pct") && hasSample("bms/power_w");
    if (essential && fresh(source, now)) {
      h.heldKnown = true; h.heldReceived = h.received; h.heldAge = h.age;
    }
  }
  bool receiveHealth(unsigned source, const char *text, uint32_t now) {
    JsonDocument d;
    if (deserializeJson(d, text) || !d["available"].is<bool>() ||
        !d["reason"].is<const char *>() ||
        std::strlen(d["reason"].as<const char *>()) >= sizeof(health_[0].reason) ||
        !d["sample_sequence"].is<uint32_t>() ||
        (!d["sample_age_ms"].is<uint32_t>() &&
         (d["available"].as<bool>() || !d["sample_age_ms"].isNull()))) return false;
    auto &h = health_[source];
    const bool available = d["available"];
    const uint32_t seq = d["sample_sequence"];
    const uint32_t age = d["sample_age_ms"] | UINT32_MAX;
    // Diagnostic failures never erase a good scalar or renew its sample clock.
    if (!available || age >= acquisitionMs) {
      h.available = false;
      std::snprintf(h.reason, sizeof(h.reason), "%s", d["reason"].as<const char *>());
      return true;
    }
    // A delayed retained packet is not evidence of a source restart.
    // Only a changed explicit collector control session permits a lower sequence.
    if (h.seeded && int32_t(seq - h.sequence) < 0) return false;
    if (!h.seeded) {
      h.seeded = true; h.sequence = seq;
      h.received = now; h.age = age; h.timestamped = true;
      h.advanced = false;
    } else if (int32_t(seq - h.sequence) > 0) {
      h.sequence = seq; h.received = now; h.age = age;
      h.timestamped = h.advanced = true;
    }
    h.available = true;
    recordQualified(source, now);
    std::snprintf(h.reason, sizeof(h.reason), "%s", d["reason"].as<const char *>());
    return true;
  }
};
} // namespace sbt
