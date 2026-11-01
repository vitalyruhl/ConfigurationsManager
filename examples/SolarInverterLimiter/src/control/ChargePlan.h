#pragma once

#include "CurrentControl.h"
#include "FuturePrices.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace control {
struct ChargePlanInputs {
    bool sourcesQualified = false, chargeAvailable = false, controlEnabled = false;
    float soc = 0, fullCapacityAh = 0, maxChargeW = 0, currentChargerW = 0, priceFactor = 1;
};

// Diagnostic DC withholding intention; never a complete future inverter command.
struct ChargePlan {
    static constexpr unsigned kSlots = 144;
    static constexpr size_t kPayloadBytes = 2048, kMqttPacketBytes = 2304;
    uint32_t generatedAt = 0, priceRevision = 0, pvRevision = 0, start = 0;
    unsigned count = 0;
    bool valid = false;
    const char *reason = "source_missing";
    int32_t watts[kSlots], solarWatts[kSlots];
    uint32_t targetAt = 0;
    float remainingWhDC = 0;

    ChargePlan() { reset(0, 0, 0); }
    void reset(uint32_t utc, uint32_t prices, uint32_t pv) {
        generatedAt = utc; priceRevision = prices; pvRevision = pv; start = targetAt = 0;
        count = 0; valid = false; reason = "source_missing"; remainingWhDC = 0;
        std::fill(watts, watts + kSlots, -1); std::fill(solarWatts, solarWatts + kSlots, -1);
    }
    void finish(bool accepted, const char *why) { valid = accepted; reason = why; if (!valid) count = 0; }
    static bool packetFits(size_t topicBytes, size_t payloadBytes) {
        return payloadBytes > 0 && payloadBytes < kPayloadBytes && topicBytes <= kMqttPacketBytes - 7 &&
            payloadBytes <= kMqttPacketBytes - 7 - topicBytes;
    }
    void project(uint32_t utc, const PreparedSettings &cfg, const FuturePlan &future,
                 const FuturePrices &prices, const PvForecast &pv, const CurrentDecision &decision,
                 const DischargeBudget &red, const ChargePlanInputs &in) {
        reset(utc, prices.revision(), pv.revision());
        if (utc < 1704067200UL) { reason = "clock_invalid"; return; }
        if (!in.sourcesQualified || !cfg.valid || !std::isfinite(in.soc) || in.soc < 0 || in.soc > 100 ||
            !std::isfinite(in.fullCapacityAh) || in.fullCapacityAh <= 0 || in.fullCapacityAh > 389 ||
            !std::isfinite(in.currentChargerW) || in.currentChargerW < 0 ||
            !std::isfinite(in.maxChargeW) || in.maxChargeW < 0 ||
            !std::isfinite(in.priceFactor) || in.priceFactor <= 0 || in.priceFactor > 5) return;
        if (!future.valid || !red.valid || red.count < 2 || red.count > kSlots || red.start % 900 ||
            utc < red.start || uint64_t(utc) >= uint64_t(red.start) + 900 ||
            uint64_t(red.start) + red.count * 900ULL > UINT32_MAX ||
            !std::isfinite(future.shortfallWh) || future.shortfallWh < 0 ||
            !std::isfinite(future.chargeTargetW) || future.chargeTargetW < 0 ||
            !std::isfinite(cfg.reserveOffSocPercent) || !std::isfinite(cfg.reserveOnSocPercent) ||
            cfg.reserveOffSocPercent < 0 || cfg.reserveOnSocPercent > 100 ||
            cfg.reserveOffSocPercent >= cfg.reserveOnSocPercent ||
            !std::isfinite(cfg.priceComfortEurPerKwh) || !std::isfinite(cfg.priceDeadbandEurPerKwh) ||
            cfg.priceDeadbandEurPerKwh < 0) {
            reason = "forecast_missing"; return;
        }
        start = red.start; count = red.count; targetAt = future.targetAt;
        if (!future.coupledProjection || !prices.projectionMatches(utc, start, count)) {
            finish(false, "forecast_missing"); return;
        }
        remainingWhDC = future.projectedUnmetWhDC;
        for (unsigned i = 0; i < count; ++i) {
            solarWatts[i] = prices.projectedSolarW(i);
            watts[i] = !in.controlEnabled || !in.chargeAvailable ? 0 :
                (solarWatts[i] < 0 ? -1 : int32_t(std::max(0.0f, prices.projectedChargeW(i))));
        }
        // Slot zero follows the independently guarded live decision, including negative-price stops.
        if (!in.controlEnabled || !in.chargeAvailable || !decision.chargeTargetValid ||
            !std::isfinite(decision.chargeTargetW) || decision.chargeTargetW < 0) watts[0] = 0;
        else if (watts[0] >= 0) watts[0] = int32_t(std::min(float(watts[0]), decision.chargeTargetW));
        finish(true, !in.controlEnabled ? "control_off" : (!in.chargeAvailable ? "charge_blocked" : "intent_projection"));
    }
    // Fixed buffers only. Unknown values are null; any malformed/overflowed plan fails closed.
    size_t serialize(char *buffer, size_t capacity) const {
        if (!buffer || !capacity) return 0;
        buffer[0] = '\0';
        if (count > kSlots || (valid && count < 2) || !std::isfinite(remainingWhDC) || remainingWhDC < 0) return 0;
        int written = std::snprintf(buffer, capacity,
            "{\"v\":1,\"valid\":%s,\"reason\":\"%s\",\"kind\":\"intent_projection\",\"g\":%lu,\"r\":%lu,\"pv\":%lu,\"t\":%lu,\"d\":900,\"n\":%u,\"target\":%lu,\"unmetWh\":%.1f,\"w\":[",
            valid ? "true" : "false", reason, static_cast<unsigned long>(generatedAt),
            static_cast<unsigned long>(priceRevision), static_cast<unsigned long>(pvRevision),
            static_cast<unsigned long>(start), valid ? count : 0, static_cast<unsigned long>(targetAt),
            double(remainingWhDC));
        if (written < 0 || size_t(written) >= capacity) { buffer[0] = '\0'; return 0; }
        size_t used = size_t(written);
        for (unsigned array = 0; array < 2; ++array) {
            for (unsigned i = 0; valid && i < count; ++i) {
                const int32_t value = array ? solarWatts[i] : watts[i];
                if (value < -1 || value > 12000) { buffer[0] = '\0'; return 0; }
                if (value == -1) written = std::snprintf(buffer + used, capacity - used, "%snull", i ? "," : "");
                else written = std::snprintf(buffer + used, capacity - used, "%s%ld", i ? "," : "", static_cast<long>(value));
                if (written < 0 || size_t(written) >= capacity - used) { buffer[0] = '\0'; return 0; }
                used += size_t(written);
            }
            const char *tail = array ? "]}" : "],\"s\":[";
            const size_t size = std::strlen(tail);
            if (capacity - used <= size) { buffer[0] = '\0'; return 0; }
            std::memcpy(buffer + used, tail, size + 1); used += size;
        }
        return used;
    }
};
using ChargePlanPublication = PlanPublication<ChargePlan>;
} // namespace control
