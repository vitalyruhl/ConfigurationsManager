#pragma once

#include "PreparedControl.h"
#include "DischargeBudget.h"
#include "JsonArena.h"
#include "PvForecast.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace control {
struct FuturePlan {
    bool valid = false, pvQualified = false, coupledProjection = false;
    float offeredPvWh = 0; // Sum of known DC offers; unknown intervals contribute no invented energy.
    float usablePvWh = 0;  // AC-deliverable energy actually stored in the private chronological projection.
    float projectedUnmetWhDC = 0;
    int batteryCeilingW = 0;
    float chargeTargetW = 0, availableWh = 0, reservedWh = 0, shortfallWh = 0, nextPrice = 0;
    uint32_t targetAt = 0;
    const char *reason = "forecast_missing";
};

// Published quarter-hour prices in micro EUR/kWh. No persistence or heap cache.
class FuturePrices {
public:
    static constexpr unsigned kSlots = 288, kChunk = 24, kPlanSlots = 144;
    static_assert(kPlanSlots == DischargeBudget::kSlots, "Trace must cover the entire allocation window");
    void clear() {
        projectionCount_ = 0;
        if (activeCount_) ++contentRevision_;
        activeCount_ = pendingCount_ = 0; parts_ = 0;
        activeRevision_ = pendingRevision_ = 0;
        pendingPoisoned_ = cacheValid_ = false;
        // Losing a schedule must never replenish the independently measured energy pool.
    }
    const char *receive(const uint8_t *payload, unsigned length, uint32_t utc, uint32_t ms) {
        if (utc < 1704067200UL) return "clock_invalid";
        if (!payload || !length || length > 768) return "packet_size";
        if (std::memchr(payload, 0, length)) return "packet_json";
        arena_.reset();
        JsonDocument doc(&arena_);
        if (deserializeJson(doc, payload, length)) return "packet_json";
        if (!doc["v"].is<unsigned>() || doc["v"].as<unsigned>() != 1 ||
            !doc["r"].is<uint32_t>() || !doc["t"].is<uint32_t>() ||
            !doc["n"].is<unsigned>() || !doc["k"].is<unsigned>() ||
            !doc["p"].is<JsonArray>()) return "packet_schema";
        const uint32_t revision = doc["r"], start = doc["t"];
        const unsigned count = doc["n"], part = doc["k"];
        const JsonArrayConst prices = doc["p"].as<JsonArrayConst>();
        const uint64_t coverageEnd = uint64_t(start) + uint64_t(count) * 900;
        // Retained recovery keeps original generation/coverage; receipt time is not expiry.
        if (uint64_t(revision) > uint64_t(utc) + 120 || uint64_t(utc) > uint64_t(revision) + 259200 ||
            start % 900 || start > revision || revision - start >= 900 || start > utc ||
            count > kSlots || part >= 12 || coverageEnd > UINT32_MAX ||
            (count && uint64_t(utc) >= coverageEnd)) return "packet_range";
        if (revision < activeRevision_ || revision < pendingRevision_) return "old_revision";
        if (!count) {
            if (part || prices.size()) return "packet_empty";
            if (revision == pendingRevision_ && pendingCount_) {
                pendingPoisoned_ = true; return "packet_conflict";
            }
            if (revision == activeRevision_) return activeCount_ ? "packet_conflict" : "duplicate";
            clear(); activeRevision_ = revision;
            return "withdrawn";
        }
        const unsigned offset = part * kChunk;
        if (count < 2 || offset >= count || prices.size() != std::min(kChunk, count - offset))
            return "packet_count";
        for (JsonVariantConst value : prices)
            if (!value.is<int32_t>() || value.as<int32_t>() < -2000000 || value.as<int32_t>() > 10000000)
                return "packet_price";
        if (revision == activeRevision_) {
            if (start != activeStart_ || count != activeCount_) return "packet_conflict";
            for (unsigned i = 0; i < prices.size(); ++i)
                if (active_[offset + i] != prices[i].as<int32_t>()) return "packet_conflict";
            return "duplicate";
        }
        if (revision == pendingRevision_ && pendingPoisoned_) return "packet_conflict";
        if (revision != pendingRevision_ || uint32_t(ms - pendingAt_) > 30000UL) {
            pendingRevision_ = revision; pendingStart_ = start; pendingCount_ = count;
            parts_ = 0; pendingAt_ = ms; pendingPoisoned_ = false;
        }
        if (start != pendingStart_ || count != pendingCount_) {
            pendingPoisoned_ = true; return "packet_conflict";
        }
        if (parts_ & (1u << part)) {
            for (unsigned i = 0; i < prices.size(); ++i)
                if (pending_[offset + i] != prices[i].as<int32_t>()) {
                    pendingPoisoned_ = true; return "packet_conflict";
                }
        }
        for (unsigned i = 0; i < prices.size(); ++i) pending_[offset + i] = prices[i];
        parts_ |= uint16_t(1u << part);
        const unsigned chunks = (count + kChunk - 1) / kChunk;
        if (parts_ != (1u << chunks) - 1) return "pending";
        if (activeStart_ != start || activeCount_ != count ||
            std::memcmp(active_, pending_, count * sizeof(active_[0]))) ++contentRevision_;
        std::memcpy(active_, pending_, count * sizeof(active_[0]));
        activeStart_ = start; activeCount_ = count; activeRevision_ = revision; cacheValid_ = false;
        return "accepted";
    }

    FuturePlan plan(uint32_t utc, uint32_t ms, const PreparedSettings &cfg,
                    bool bmsValid, float soc, float batteryW, float fullAh,
                    uint32_t sequence, bool currentPriceValid, float currentPrice, bool currentExpensive,
                    float factor, int maximumW, const PvForecast *pv = nullptr, float qualifiedMaxChargeW = 0, DischargeBudget *trace = nullptr,
                    float currentChargerW = -1, bool currentCheap = false) {
        FuturePlan out;
        projectionCount_ = 0;
        if (trace) trace->reset(utc, activeRevision_, pv ? pv->revision() : 0);
        const bool energyValid = bmsValid && std::isfinite(fullAh) && fullAh > 0 && fullAh <= 389 &&
            std::isfinite(soc) && soc >= 0 && soc <= 100 && std::isfinite(batteryW);
        if (!energyValid) {
            energyTickReady_ = false; // Preserve known debits; never integrate an unknown outage.
            out.reason = "energy_missing"; return finish(out, trace);
        }
        if (!std::isfinite(cfg.reserveOffSocPercent) || !std::isfinite(cfg.reserveOnSocPercent) ||
            cfg.reserveOffSocPercent < 0 || cfg.reserveOnSocPercent > 100 ||
            cfg.reserveOffSocPercent >= cfg.reserveOnSocPercent || !std::isfinite(cfg.priceComfortEurPerKwh) ||
            !std::isfinite(cfg.priceDeadbandEurPerKwh) || cfg.priceDeadbandEurPerKwh < 0) {
            out.reason = "forecast_settings"; return finish(out, trace);
        }
        const float fullWh = fullAh * 51.38f;
        const float socWh = std::max(0.0f, (soc - cfg.reserveOffSocPercent) * .01f * fullWh);
        if (!energyKnown_) {
            energyWh_ = socWh; energyKnown_ = true;
        } else if (energyTickReady_ && uint32_t(ms - energyAt_) <= kSourceStaleMs) {
            // Every qualified control call debits held signed power; fresh samples update the endpoint.
            const float watts = sequence == energySequence_ ? lastBatteryW_ : (batteryW + lastBatteryW_) * .5f;
            energyWh_ += watts * float(uint32_t(ms - energyAt_)) / 3600000.0f;
        }
        energySequence_ = sequence; energyAt_ = ms; lastBatteryW_ = batteryW; energyTickReady_ = true;
        energyWh_ = std::max(0.0f, std::min(energyWh_, socWh));
        out.availableWh = energyWh_ * kDischargeEfficiency;
        if (!cfg.valid || !std::isfinite(factor) || factor <= 0 || factor > 5) {
            out.reason = "forecast_settings"; return finish(out, trace);
        }
        if (utc < 1704067200UL || (lastUtc_ && uint64_t(utc) + 5 < lastUtc_)) {
            out.reason = "clock_invalid"; clear(); lastUtc_ = utc; return finish(out, trace);
        }
        lastUtc_ = utc;
        if (!activeCount_ || utc < activeStart_ || uint64_t(utc) >= uint64_t(activeStart_) + activeCount_ * 900ULL ||
            uint64_t(utc) > uint64_t(activeRevision_) + 259200) return finish(out, trace);
        const unsigned first = (utc - activeStart_) / 900;
        const unsigned end = std::min(unsigned(activeCount_), first + kPlanSlots);
        if (end <= first + 1) return finish(out, trace);
        if (!currentPriceValid || !std::isfinite(currentPrice)) {
            out.reason = "current_price_missing"; return finish(out, trace);
        }
        // Cap resolution follows the whole-watt forecast wire contract, not arbitrary noise percentages.
        // 12000 W is the maximum encoded offer (10000 W * maximum calibration 1.2), not a plant limit.
        const int chargeCapW = std::isfinite(qualifiedMaxChargeW) && qualifiedMaxChargeW > 0
            ? int(std::min(qualifiedMaxChargeW, 12000.0f)) : 0;
        const uint32_t pvRevision = pv ? pv->revision() : 0;
        const bool pvValid = pv && pv->valid(utc);
        const float capacityAc = (100 - cfg.reserveOffSocPercent) * .01f * fullWh * kDischargeEfficiency;
        const float initialReserveDeficitDc = std::max(0.0f, (cfg.reserveOffSocPercent - soc) * .01f * fullWh);
        if (!cacheValid_ || cacheFirst_ != first || cachePriceRevision_ != activeRevision_ ||
            cachePv_ != pv || cachePvRevision_ != pvRevision || cachePvValid_ != pvValid ||
            cachePrice_ != currentPrice || cacheFactor_ != factor || cacheMaximumW_ != maximumW ||
            cacheChargeCapW_ != chargeCapW || cacheCapacityAc_ != capacityAc ||
            cacheReserveDebtAc_ != initialReserveDeficitDc * kDischargeEfficiency ||
            cacheReserve_ != cfg.reserveOffSocPercent || uint32_t(ms - cacheAt_) >= kReplanMs)
            rebuild(first, end, utc, ms, cfg, currentPrice, factor, maximumW, pv, chargeCapW, capacityAc,
                initialReserveDeficitDc * kDischargeEfficiency);

        const float elapsedHours = float(utc - (activeStart_ + first * 900UL)) / 3600.0f;
        out.offeredPvWh = std::max(0.0f, cacheOfferWh_ - cacheFirstOfferW_ * elapsedHours);
        out.pvQualified = cachePvValid_ && chargeCapW > 0;
        const auto price = [&](unsigned i) { return i == first ? currentPrice : float(active_[i]) * .000001f * factor; };
        out.nextPrice = price(first + 1);
        if (trace) { trace->start = activeStart_ + first * 900UL; trace->count = end - first; }
        project(trace, first, end, utc, factor, maximumW, out, capacityAc,
            std::max(0.0f, (100 - soc) * .01f * fullWh),
            initialReserveDeficitDc,
            fullWh, cfg, currentExpensive, currentCheap, currentChargerW, chargeCapW);
        out.valid = true;
        out.reason = out.batteryCeilingW > 0 ? "forecast_release" : (out.shortfallWh > 0 ? "forecast_charge" : "forecast_hold");
        return finish(out, trace);
    }
    uint32_t planningBuilds() const { return planningBuilds_; }
    uint32_t revision() const { return activeRevision_; }
    uint32_t contentRevision() const { return contentRevision_; }
    unsigned slots() const { return activeCount_; }
    // Diagnostic lookup only; never advances the measured-energy ledger or planner cache.
    bool priceAt(uint32_t utc, float factor, float &price) const {
        if (!activeCount_ || !std::isfinite(factor) || factor <= 0 || factor > 5 ||
            utc < activeStart_ || uint64_t(utc) >= uint64_t(activeStart_) + activeCount_ * 900ULL ||
            uint64_t(utc) > uint64_t(activeRevision_) + 259200ULL) return false;
        price = float(active_[(utc - activeStart_) / 900UL]) * .000001f * factor;
        return std::isfinite(price);
    }
    bool projectionMatches(uint32_t utc, uint32_t start, unsigned count) const {
        return projectionCount_ && projectionUtc_ == utc && projectionStart_ == start && projectionCount_ == count;
    }
    float projectedChargeW(unsigned i) const { return i < projectionCount_ ? projectedChargeW_[i] : 0; }
    int32_t projectedSolarW(unsigned i) const {
        if (i >= projectionCount_) return -1;
        return !i ? int32_t(projectionFirstSolarW_) : (storageKnown_[i] ? int32_t(solarW_[i]) : -1);
    }
    uint32_t planningSteps() const { return planningSteps_; }
private:
    static FuturePlan finish(FuturePlan out, DischargeBudget *trace) {
        if (trace) trace->finish(out.valid, out.reason);
        return out;
    }
    static bool eligible(unsigned i, unsigned first, float p, float currentPrice) {
        // Held energy has already paid acquisition losses/cycle costs. Defer its
        // current use only for a materially better future price; flat/cheaper slots cannot hoard it.
        return p > 0 && (i == first || p >= currentPrice + kMinimumBenefitEurPerKwh);
    }
    void project(DischargeBudget *trace, unsigned first, unsigned end, uint32_t utc,
                 float factor, int maximumW, FuturePlan &out, float capacityAc, float headroomDc,
                 float reserveDeficitDc, float fullWh, const PreparedSettings &cfg, bool currentExpensive, bool currentCheap,
                 float currentChargerW, int chargeCapW) {
        const unsigned count = end - first;
        const float maximum = std::max(0.0f, float(maximumW));
        float pool = out.availableWh;
        int band = currentExpensive ? 2 : (currentCheap ||
            cachePrice_ <= cfg.priceComfortEurPerKwh - cfg.priceDeadbandEurPerKwh ? 0 : 1);
        projectionStart_ = activeStart_ + first * 900UL; projectionUtc_ = utc;
        // One private physical ledger: a slot either charges or discharges the battery.
        // Unknown PV grants no inflow, but cannot erase later independently known sunlight.
        for (unsigned k = 0; k < count; ++k) {
            const uint32_t start = projectionStart_ + k * 900UL;
            const float hours = float(start + 900UL - (k ? start : utc)) / 3600.0f;
            const float price = k ? float(active_[first + k]) * .000001f * factor : cachePrice_;
            if (k) {
                const float high = cfg.priceComfortEurPerKwh + cfg.priceDeadbandEurPerKwh;
                const float low = cfg.priceComfortEurPerKwh - cfg.priceDeadbandEurPerKwh;
                const float hysteresis = cfg.priceDeadbandEurPerKwh / 3.0f;
                if (!(band == 2 && price > high - hysteresis) && !(band == 0 && price < low + hysteresis))
                    band = price >= high ? 2 : (price <= low ? 0 : 1);
            }
            const bool measured = !k && std::isfinite(currentChargerW) && currentChargerW >= 0;
            const bool legacyOffer = k || currentChargerW == -1;
            const float solarW = measured ? std::min(currentChargerW, 12000.0f) :
                (legacyOffer ? std::min(solarW_[k], float(chargeCapW)) : 0);
            if (!k) projectionFirstSolarW_ = measured ? std::min(currentChargerW, 12000.0f) :
                (legacyOffer && storageKnown_[0] ? solarW_[0] : -1);
            const float solarDc = std::max(0.0f, solarW) * hours;
            const float restoredDebtAc = std::max(0.0f, cacheReserveDebtAc_ - reserveDeficitDc * kDischargeEfficiency);
            const float economicMissing = std::max(0.0f,
                economicWh_[k] - std::min(restoredDebtAc, economicDebtWh_[k]) - pool);
            const bool backwardDebt = cacheDebtTarget_ < count && k < cacheDebtTarget_;
            float desiredDc = price >= 0 && economicMissing > 0 ? economicMissing / kDischargeEfficiency +
                (backwardDebt ? 0 : reserveDeficitDc) : 0;
            const float physicalDc = fullWh - headroomDc;
            const float soc = fullWh > 0 ? physicalDc / fullWh * 100 : 0;
            const float reserveDc = soc <= cfg.reserveOffSocPercent && band != 2 ? solarDc :
                (soc > cfg.reserveOffSocPercent && soc < cfg.reserveOnSocPercent && band == 0 ? solarDc * .5f : 0);
            desiredDc = std::max(desiredDc, reserveDc);
            // Surplus beyond the AC output limit can charge without foregoing direct PV use.
            if (price >= 0) desiredDc = std::max(desiredDc, std::max(0.0f, solarDc - maximum * hours / kDischargeEfficiency));
            else desiredDc = 0;
            float storedDc = std::max(0.0f, std::min({desiredDc, solarDc, float(chargeCapW) * hours, headroomDc,
                reserveDeficitDc + std::max(0.0f, capacityAc - pool) / kDischargeEfficiency}));
            float allocation = 0;
            if (storedDc <= 0 && price > 0 && reserveDeficitDc <= 0) {
                const float batteryLimit = std::max(0.0f, maximum * hours - solarDc * kDischargeEfficiency);
                allocation = std::min(batteryLimit, std::max(0.0f,
                    pool - std::max(0.0f, protectedWh_[k] - std::min(restoredDebtAc, protectedDebtWh_[k]))));
            }
            pool = std::max(0.0f, pool - allocation);
            headroomDc += allocation / kDischargeEfficiency;
            const float restoredDc = std::min(reserveDeficitDc, storedDc);
            reserveDeficitDc = std::max(0.0f, reserveDeficitDc - restoredDc);
            pool = std::min(capacityAc, pool + (storedDc - restoredDc) * kDischargeEfficiency);
            headroomDc = std::max(0.0f, headroomDc - storedDc);
            projectedChargeW_[k] = storedDc / hours;
            out.usablePvWh += (storedDc - restoredDc) * kDischargeEfficiency;
            const int watts = int(std::max(0.0f, std::min(maximum, allocation / hours)));
            if (trace) trace->watts[k] = watts;
            if (!k) {
                out.batteryCeilingW = watts; out.chargeTargetW = storedDc / hours;
                out.reservedWh = std::min(std::max(0.0f, out.availableWh - allocation), protectedWh_[0]);
                out.shortfallWh = economicMissing;
                out.projectedUnmetWhDC = std::max(0.0f, economicMissing / kDischargeEfficiency -
                    (backwardDebt ? storedDc : storedDc - restoredDc));
                out.targetAt = targetIndex_[0] < count ? projectionStart_ + targetIndex_[0] * 900UL : 0;
            }
        }
        projectionCount_ = count; out.coupledProjection = true;
    }
    void rebuild(unsigned first, unsigned end, uint32_t utc, uint32_t ms, const PreparedSettings &cfg,
                 float currentPrice, float factor, int maximumW, const PvForecast *pv, int chargeCapW,
                 float capacityAc, float reserveDebtAc) {
        cacheFirst_ = first; cachePriceRevision_ = activeRevision_; cachePv_ = pv;
        cachePvRevision_ = pv ? pv->revision() : 0; cachePvValid_ = pv && pv->valid(utc);
        cachePrice_ = currentPrice; cacheFactor_ = factor; cacheMaximumW_ = maximumW;
        cacheChargeCapW_ = chargeCapW; cacheReserve_ = cfg.reserveOffSocPercent; cacheCapacityAc_ = capacityAc;
        cacheReserveDebtAc_ = reserveDebtAc;
        cacheAt_ = ms; cacheValid_ = true; ++planningBuilds_;
        const unsigned count = end - first;
        const auto price = [&](unsigned i) { return i == first ? currentPrice : float(active_[i]) * .000001f * factor; };
        for (unsigned i = 0; i < count; ++i) order_[i] = uint16_t(first + i);
        std::sort(order_, order_ + count, [&](unsigned a, unsigned b) { return price(a) == price(b) ? a < b : price(a) > price(b); });
        cacheOfferWh_ = cacheFirstOfferW_ = 0;
        for (unsigned j = 0; j < count; ++j) {
            const uint32_t at = activeStart_ + (first + j) * 900UL;
            float watts = 0;
            const bool known = pv && pv->wattsAt(at, watts) && pv->valid(at + 899UL);
            storageKnown_[j] = known;
            solarW_[j] = known ? watts : 0;
            if (known) cacheOfferWh_ += watts * .25f;
            if (!j) cacheFirstOfferW_ = known ? watts : 0;
        }
        std::fill(pendingWh_, pendingWh_ + count, 0);
        // The initial reserve debt is a priced prerequisite of one real future battery
        // target. It consumes PV but is never usable inventory or pruned as usable capacity.
        cacheDebtTarget_ = count;
        if (reserveDebtAc > 0) for (unsigned rank = 0; rank < count; ++rank) {
            const unsigned j = order_[rank] - first;
            const float direct = std::min(solarW_[j], float(chargeCapW)) * kDischargeEfficiency;
            if (j && price(first + j) > 0 && float(maximumW) > direct) {
                cacheDebtTarget_ = j; break;
            }
        }
        float pendingDebt = 0;
        planningSteps_ = 0;
        // Backward marginal demand, capped by physical storage. The strict price order is
        // transitive; the 3-cent deferral gate is applied only when measuring protected demand.
        for (unsigned reverse = count; reverse > 0; --reverse) {
            const unsigned k = reverse - 1;
            const float current = price(first + k);
            protectedWh_[k] = economicWh_[k] = protectedDebtWh_[k] = economicDebtWh_[k] = 0;
            targetIndex_[k] = uint16_t(count);
            const float solarAc = current >= 0 ? std::min(solarW_[k], float(chargeCapW)) * .25f * kDischargeEfficiency : 0;
            const float direct = std::min(solarAc, std::max(0.0f, float(maximumW)) * .25f);
            float freePv = std::max(0.0f, solarAc - direct), shiftablePv = direct, remainingCapacity = capacityAc;
            for (unsigned rank = 0; rank < count; ++rank) {
                ++planningSteps_;
                const unsigned j = order_[rank] - first;
                if (j == k) {
                    pendingWh_[j] = current > 0 ? std::max(0.0f, float(maximumW)) * .25f - shiftablePv : 0;
                    if (j == cacheDebtTarget_) { pendingDebt = reserveDebtAc; pendingWh_[j] += pendingDebt; }
                }
                else if (j > k && pendingWh_[j] > 0) {
                    const float future = price(first + j), need = pendingWh_[j];
                    if (eligible(j, k, future, current)) {
                        protectedWh_[k] += need;
                        if (j == cacheDebtTarget_) protectedDebtWh_[k] = pendingDebt;
                    }
                    const bool profitable = future * kRoundTripEfficiency - current >= kWearEurPerKwh + kMinimumBenefitEurPerKwh;
                    if (profitable) {
                        economicWh_[k] += need;
                        if (j == cacheDebtTarget_) economicDebtWh_[k] = pendingDebt;
                        targetIndex_[k] = std::min(targetIndex_[k], uint16_t(j));
                    }
                    const float freeCredit = std::min(freePv, pendingWh_[j]);
                    if (j == cacheDebtTarget_) pendingDebt = std::max(0.0f, pendingDebt - freeCredit);
                    freePv -= freeCredit; pendingWh_[j] -= freeCredit;
                    if (profitable) {
                        const float credit = std::min(shiftablePv, pendingWh_[j]);
                        if (j == cacheDebtTarget_) pendingDebt = std::max(0.0f, pendingDebt - credit);
                        shiftablePv -= credit; pendingWh_[j] -= credit;
                    }
                }
                const float debt = j == cacheDebtTarget_ ? pendingDebt : 0;
                const float usable = std::min(std::max(0.0f, pendingWh_[j] - debt), remainingCapacity);
                pendingWh_[j] = debt + usable;
                remainingCapacity -= usable;
            }
        }
    }
    int32_t active_[kSlots] = {}, pending_[kSlots] = {};
    uint32_t activeStart_ = 0, activeRevision_ = 0, pendingStart_ = 0, pendingRevision_ = 0;
    uint32_t pendingAt_ = 0, lastUtc_ = 0, contentRevision_ = 0;
    unsigned activeCount_ = 0, pendingCount_ = 0;
    uint16_t parts_ = 0;
    bool pendingPoisoned_ = false, energyKnown_ = false, energyTickReady_ = false;
    float energyWh_ = 0, lastBatteryW_ = 0;
    uint32_t energyAt_ = 0, energySequence_ = 0;
    // Fixed BSS workspace; no per-control-step allocation or large stack copies.
    uint16_t order_[kPlanSlots] = {};
    float solarW_[kPlanSlots] = {}, pendingWh_[kPlanSlots] = {};
    float protectedWh_[kPlanSlots] = {}, economicWh_[kPlanSlots] = {}, projectedChargeW_[kPlanSlots] = {};
    float protectedDebtWh_[kPlanSlots] = {}, economicDebtWh_[kPlanSlots] = {};
    uint16_t targetIndex_[kPlanSlots] = {};
    bool storageKnown_[kPlanSlots] = {};
    uint32_t projectionUtc_ = 0, projectionStart_ = 0, planningSteps_ = 0;
    unsigned projectionCount_ = 0;
    float projectionFirstSolarW_ = -1;
    const PvForecast *cachePv_ = nullptr;
    uint32_t cachePriceRevision_ = 0, cachePvRevision_ = 0, cacheAt_ = 0, planningBuilds_ = 0;
    unsigned cacheFirst_ = 0, cacheDebtTarget_ = 0;
    int cacheMaximumW_ = 0, cacheChargeCapW_ = 0;
    float cachePrice_ = 0, cacheFactor_ = 0, cacheReserve_ = 0, cacheCapacityAc_ = 0, cacheReserveDebtAc_ = 0;
    float cacheOfferWh_ = 0, cacheFirstOfferW_ = 0;
    bool cacheValid_ = false, cachePvValid_ = false;
    JsonArena arena_;
};
} // namespace control
