#pragma once
#include "JsonArena.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace control {
// UTC forecast offer only. No actuator, persistence, or acquisition permission.
class PvForecast {
public:
    static constexpr unsigned kSlots = 144, kChunk = 24;
    void clear() { if (active_.count) ++contentRevision_; active_ = Snapshot{}; pending_ = Snapshot{}; parts_ = 0; poisoned_ = false; }
    bool valid(uint32_t utc) const {
        return utc >= 1704067200UL && active_.count && utc >= active_.start &&
            utc < active_.expires && utc < active_.start + active_.count * 900UL;
    }
    uint32_t revision() const { return active_.revision; }
    uint32_t contentRevision() const { return contentRevision_; }
    uint32_t expiry() const { return active_.expires; }
    unsigned slots() const { return active_.count; }
    bool calibrated() const { return active_.days >= 14; }
    static constexpr size_t stateBytes() { return sizeof(PvForecast) - sizeof(JsonArena); }
    bool wattsAt(uint32_t utc, float &watts, unsigned *quality = nullptr) const {
        if (!valid(utc)) return false;
        const unsigned i = (utc - active_.start) / 900;
        const unsigned q = flag(active_, i);
        if (quality) *quality = q;
        if (q == 1) return false;
        watts = float(active_.watts[i]) * float(active_.factor) / 1000.0f;
        return true;
    }
    // Missing intervals fail the entire requested interval; no extrapolation.
    bool energyWh(uint32_t from, uint32_t to, float &wh) const {
        wh = 0;
        if (to <= from || !valid(from) || to > active_.expires ||
            to > active_.start + active_.count * 900UL) return false;
        for (uint32_t at = from; at < to;) {
            float watts = 0;
            if (!wattsAt(at, watts)) { wh = 0; return false; }
            const uint32_t end = std::min<uint32_t>(to, active_.start + ((at - active_.start) / 900 + 1) * 900UL);
            wh += watts * float(end - at) / 3600.0f;
            at = end;
        }
        return true;
    }
    const char *receive(const uint8_t *payload, unsigned length, uint32_t utc, uint32_t ms) {
        if (utc < 1704067200UL) return "clock_invalid";
        if (!payload || !length || length > 768) return "packet_size";
        if (std::memchr(payload, 0, length)) return "packet_json";
        arena_.reset();
        JsonDocument d(&arena_);
        if (deserializeJson(d, payload, length, DeserializationOption::NestingLimit(3))) return "packet_json";
        for (const char *key : {"v", "s", "r", "t", "d", "n", "k", "g", "e", "f", "c", "a"})
            if (!d[key].is<uint32_t>()) return "packet_schema";
        if (!d["p"].is<JsonArray>() || !d["x"].is<JsonArray>() ||
            !d["q"].is<const char *>() || !d["o"].is<const char *>()) return "packet_schema";
        const uint32_t start = d["t"], revision = d["r"], generated = d["g"], expires = d["e"], session = d["s"];
        const unsigned count = d["n"], part = d["k"], factor = d["f"], days = d["c"], age = d["a"];
        const bool calibrated = !std::strcmp(d["q"], "calibrated");
        const bool uncalibrated = !std::strcmp(d["q"], "uncalibrated");
        if (d["v"].as<unsigned>() != 2 || d["d"].as<unsigned>() != 900 ||
            std::strcmp(d["o"], "forecast_solar") || (!calibrated && !uncalibrated)) return "packet_schema";
        if (count < 2 || count > kSlots || part >= 6 || part * kChunk >= count ||
            start % 900 || start > generated || generated - start >= 900 ||
            generated != revision || generated > utc + 120 ||
            session < 1704067200UL || session > generated ||
            start > utc || utc - start >= kSlots * 900UL ||
            expires <= generated || expires <= utc || expires > start + count * 900UL ||
            factor < 500 || factor > 1200 || days > 30 || age > 65535 ||
            (uncalibrated && (factor != 1000 || days >= 14)) || (calibrated && days < 14)) return "packet_range";
        const JsonArrayConst values = d["p"], flags = d["x"];
        const unsigned offset = part * kChunk, size = std::min(kChunk, count - offset);
        if (values.size() != size || flags.size() != size) return "packet_count";
        for (unsigned i = 0; i < size; ++i) {
            if (!flags[i].is<unsigned>()) return "packet_quality";
            const unsigned q = flags[i];
            if (q > 2 || (q == 1 ? !values[i].isNull() :
                (!values[i].is<unsigned>() || values[i].as<unsigned>() > 10000))) return "packet_value";
        }
        if (revision < active_.revision || revision < pending_.revision) return "old_revision";
        if (session < active_.session || session < pending_.session) return "old_session";
        const auto sameMeta = [&](const Snapshot &snapshot) {
            return snapshot.session == session && snapshot.revision == revision && snapshot.start == start &&
                snapshot.generated == generated && snapshot.expires == expires && snapshot.count == count &&
                snapshot.factor == factor && snapshot.days == days && snapshot.age == age;
        };
        const auto samePart = [&](const Snapshot &snapshot) {
            for (unsigned i = 0; i < size; ++i)
                if (flag(snapshot, offset + i) != flags[i].as<unsigned>() ||
                    (flags[i].as<unsigned>() != 1 && snapshot.watts[offset + i] != values[i].as<unsigned>())) return false;
            return true;
        };
        if (revision == active_.revision)
            return sameMeta(active_) && samePart(active_) ? "duplicate" : "packet_conflict";
        if (revision == pending_.revision && poisoned_) return "packet_conflict";
        if (revision != pending_.revision || uint32_t(ms - pendingAt_) > 30000) {
            pending_ = Snapshot{};
            pending_.session = session; pending_.revision = revision; pending_.start = start;
            pending_.generated = generated; pending_.expires = expires; pending_.count = uint16_t(count);
            pending_.factor = uint16_t(factor); pending_.days = uint16_t(days); pending_.age = uint16_t(age);
            parts_ = 0; poisoned_ = false; pendingAt_ = ms;
        }
        if (!sameMeta(pending_) || ((parts_ & (1u << part)) && !samePart(pending_))) {
            poisoned_ = true; return "packet_conflict";
        }
        for (unsigned i = 0; i < size; ++i) {
            pending_.watts[offset + i] = values[i].isNull() ? 0 : values[i].as<uint16_t>();
            setFlag(pending_, offset + i, flags[i].as<unsigned>());
        }
        parts_ |= uint8_t(1u << part);
        if (parts_ != (1u << ((count + kChunk - 1) / kChunk)) - 1) return "pending";
        // Compare validated complete content before the existing swap. Poll r/g are provenance only.
        bool same = active_.session == pending_.session && active_.start == pending_.start &&
            active_.expires == pending_.expires && active_.count == pending_.count &&
            active_.factor == pending_.factor && active_.days == pending_.days && active_.age == pending_.age;
        for (unsigned i = 0; same && i < pending_.count; ++i)
            same = active_.watts[i] == pending_.watts[i] && flag(active_, i) == flag(pending_, i);
        if (!same) ++contentRevision_;
        active_ = pending_;
        return "accepted";
    }
private:
    struct Snapshot {
        uint16_t watts[kSlots] = {};
        uint8_t quality[kSlots / 4] = {};
        uint32_t session = 0, revision = 0, start = 0, generated = 0, expires = 0;
        uint16_t count = 0, factor = 1000, days = 0, age = 0;
    };
    static unsigned flag(const Snapshot &s, unsigned i) { return (s.quality[i / 4] >> ((i % 4) * 2)) & 3; }
    static void setFlag(Snapshot &s, unsigned i, unsigned q) {
        const unsigned shift = (i % 4) * 2;
        s.quality[i / 4] = uint8_t((s.quality[i / 4] & ~(3u << shift)) | (q << shift));
    }
    Snapshot active_, pending_;
    uint32_t pendingAt_ = 0, contentRevision_ = 0;
    uint8_t parts_ = 0;
    bool poisoned_ = false;
    // Independent MQTT parsing arena. No unproven sharing with price/telemetry callbacks.
    JsonArena arena_;
};
static_assert(PvForecast::stateBytes() <= 784, "PV snapshots exceed the reviewed state budget");
} // namespace control
