#pragma once

#include "PreparedControl.h"
#include <cmath>
#include <algorithm>

namespace control {

// Standalone decision logic; no MQTT, RS485 or MOS side effects.
// Commissioning candidates, to be qualified on the installed system.
constexpr float kPvKp = 0.20f;
constexpr float kPvKiPerSecond = 0.03f;

struct CurrentInputs {
    bool sourcesWithinGrace = false;
    bool alarmRestricted = false;
    bool bmsValid = false;
    bool batteryDischargeIntentAvailable = false; // Fresh healthy source permits inverter intent, even with actual MOS OFF.
    bool batteryChargeAvailable = false; // Fresh, qualified charge permission independent of discharge.
    bool softwareMosLockOnly = false; // The sole latched discharge bit is JBD bit 12.
    bool chargerFault = false;
    bool chargerValid = false;
    bool chargerStandby = false; // Fresh actual discharge MOS OFF explains an unavailable MPPT.
    bool metersValid = false;
    bool priceValid = false;
    float soc = 0;
    float batteryW = 0; // Positive means charging.
    float householdW = 0; // Measured inverter AC + signed grid power, before correction offset.
    float chargerW = 0; // Victron battery voltage * charger output current.
    float price = 0;
    uint32_t bmsSequence = 0;
    uint32_t chargerSequence = 0;
    bool futureValid = false;
    int futureBatteryCeilingW = 0;
    float futureChargeTargetW = 0;
    const char *futureReason = "forecast_missing";
};

struct CurrentDecision {
    int ceilingW = 0;
    bool pvOnly = false; // Always use the household PID, including with Limiter OFF.
    bool minimumOverrideAllowed = false; // Qualified discharge intent and released reserve; economics may yield to configured MIN.
    bool direct = false; // A fallback/stop/bypass decision, not a demand request.
    bool reserveKnown = false;
    bool reserveAllowed = false;
    bool chargeTargetValid = false;
    float chargeTargetW = 0;
    const char *strategy = "hold";
    const char *reason = "startup";
    const char *priceBand = "unknown";
};

class CurrentControl {
public:
    bool expensivePrice(const PreparedSettings &cfg, bool valid, float price) {
        if (!cfg.valid) return false;
        classify(cfg, valid, price);
        return band_ == Expensive;
    }
    bool cheapPrice() const { return band_ == Cheap; }
    // heldCommandW is the last issued command before output correction, not a new demand.
    // The caller retains responsibility for transport and higher-priority protection.
    CurrentDecision update(const PreparedSettings &cfg, const CurrentInputs &in,
                           bool limiterEnabled, bool transportEnabled,
                           int maximumW, int heldCommandW, uint32_t now) {
        CurrentDecision out;
        const float maximum = maximumW > 0 ? float(maximumW) : 0.0f;
        const int forecastLimit = in.futureValid
            ? std::max(0, std::min(in.futureBatteryCeilingW, int(maximum))) : int(maximum);
        const bool bms = in.bmsValid && std::isfinite(in.soc) &&
            in.soc >= 0 && in.soc <= 100 && std::isfinite(in.batteryW);
        const bool charger = in.chargerValid && std::isfinite(in.chargerW) && in.chargerW >= 0;
        const bool price = in.priceValid && std::isfinite(in.price);
        if (!cfg.valid) return stop(out, "settings");
        if (bms) {
            if (in.soc <= cfg.reserveOffSocPercent) { reserveKnown_ = true; reserveAllowed_ = false; }
            else if (in.soc >= cfg.reserveOnSocPercent) { reserveKnown_ = true; reserveAllowed_ = true; }
        }
        out.reserveKnown = reserveKnown_;
        out.reserveAllowed = reserveKnown_ && reserveAllowed_;
        classify(cfg, price, in.price);
        out.priceBand = band_ == Expensive ? "expensive" :
            (band_ == Cheap ? "cheap" : (band_ == Comfort ? "comfort" : "unknown"));
        const bool chargeAvailable = bms && in.batteryChargeAvailable;
        const bool dischargeAvailable = bms && in.batteryDischargeIntentAvailable;
        const bool pvOnly = !chargeAvailable && !dischargeAvailable;
        const bool batteryOnly = bms && dischargeAvailable && !charger;
        if (pvOnly != pvOnly_ || chargeAvailable != chargeAvailable_ || dischargeAvailable != dischargeAvailable_) {
            resetDynamics(); pvOnly_ = pvOnly; chargeAvailable_ = chargeAvailable; dischargeAvailable_ = dischargeAvailable;
        }
        if (batteryOnly != batteryOnly_) {
            qualified_ = qualifying_ = false; pairs_ = 0;
            pauseDynamics(); batteryOnly_ = batteryOnly;
        }
        // A directional software lock is not a separate hardware protection fault.
        if (in.alarmRestricted && !in.softwareMosLockOnly) return stop(out, "source_alarm");
        if (!cfg.inverterEnabled) return stop(out, "inverter_off");
        if (!transportEnabled) return stop(out, "transport_off");
        if (price && in.price < 0) return stop(out, "negative_price");
        if (bms && !charger && in.chargerStandby) {
            if (in.chargerFault) return stop(out, "charger_fault");
            resetDynamics();
            out.direct = true; out.strategy = "preserve"; out.reason = "mppt_standby";
            return out;
        }
        if (pvOnly) {
            out.pvOnly = true; out.reserveAllowed = false;
            out.strategy = "pv_only";
            if (!in.metersValid || !std::isfinite(in.householdW)) return stop(out, "meters_missing");
            if (in.chargerFault) return stop(out, "charger_fault");
            if (!charger || in.chargerW <= 0) return stop(out, "pv_missing");
            // Fresh PV can qualify without a missing battery sequence ever advancing.
            if (!qualify(in, now, true)) {
                out.direct = true; out.reason = "pv_recovery"; return out;
            }
            const float demand = clamp(in.householdW, 0, maximum);
            out.ceilingW = int(clamp(in.chargerW, 0, demand));
            out.reason = bms ? "pv_only_mos" : "pv_only_bms_missing";
            return out; // No battery-power PI, forecast discharge budget or old reserve permission.
        }
        // Keep the household PID active for charge-only operation, including Limiter OFF.
        out.pvOnly = !dischargeAvailable;
        if (!limiterEnabled && dischargeAvailable) {
            resetDynamics();
            out.direct = true; out.ceilingW = int(maximum);
            out.strategy = "bypass"; out.reason = "limiter_off";
            return out;
        }
        if (!in.metersValid) return stop(out, "meters_missing");
        if (in.chargerFault) return stop(out, "charger_fault");
        if (!bms || (!charger && !batteryOnly)) return stop(out, "pv_missing");
        if (!qualify(in, now, false, batteryOnly)) {
            pauseDynamics();
            if (approved_ && in.sourcesWithinGrace && !(reserveKnown_ && !reserveAllowed_)) {
                out = lastApproved_;
                out.minimumOverrideAllowed = false; out.direct = false;
                out.ceilingW = std::max(0, std::min({out.ceilingW, forecastLimit, heldCommandW}));
                out.chargeTargetValid = false; out.reason = batteryOnly ? "source_grace" : "source_recovery";
                return out;
            }
            out.direct = true; out.reason = "recovery";
            return out;
        }
        // Missing MPPT measurements cannot add direct PV or charging power.
        const float chargerW = charger ? in.chargerW : 0.0f;
        out.minimumOverrideAllowed = dischargeAvailable && out.reserveAllowed;
        const bool reserveOffCharging = chargeAvailable && price &&
            in.soc <= cfg.reserveOffSocPercent && band_ != Expensive;
        const bool reserveOnCharging = chargeAvailable && price &&
            in.soc > cfg.reserveOffSocPercent && in.soc < cfg.reserveOnSocPercent && band_ == Cheap;
        const bool reserveCharging = reserveOffCharging || reserveOnCharging;
        const float reserveChargeW = reserveOffCharging ? chargerW :
            (reserveOnCharging ? chargerW * 0.5f : 0.0f);
        if (!reserveCharging && dischargeAvailable && out.reserveAllowed &&
            (in.futureValid ? in.futureBatteryCeilingW > 0 : band_ == Expensive)) {
            const float economicTarget = in.futureValid
                ? clamp(float(in.futureBatteryCeilingW) +
                    clamp(chargerW, 0, maximum) * kDischargeEfficiency, 0, maximum)
                : maximum;
            piActive_ = false; integral_ = 0;
            if (!economicActive_) {
                economicActive_ = true;
                economicCeiling_ = clamp(float(heldCommandW), 0, maximum);
                economicAt_ = now;
            }
            const float dt = clamp(float(uint32_t(now - economicAt_)) * 0.001f, 0, 10);
            economicAt_ = now;
            economicCeiling_ = economicTarget >= economicCeiling_
                ? clamp(economicTarget, 0, economicCeiling_ + kEconomicCeilingRampUpWPerSecond * dt)
                : clamp(economicTarget, economicCeiling_ - kEconomicCeilingRampDownWPerSecond * dt, maximum);
            // Fresh PV loss and a shrinking available-energy ceiling remain restrictive immediately.
            if (in.futureValid) economicCeiling_ = std::min(economicCeiling_, economicTarget);
            out.ceilingW = int(economicCeiling_);
            out.strategy = "discharge"; out.reason = in.futureValid ? in.futureReason : "expensive";
            return approve(out);
        }
        economicActive_ = false;
        if (!chargeAvailable) {
            // Consume available PV without reserving power for an inaccessible battery.
            if (!std::isfinite(in.householdW)) return stop(out, "meters_missing");
            piActive_ = false; integral_ = 0;
            out.pvOnly = true; out.reserveAllowed = false; out.strategy = "pv_only";
            out.ceilingW = int(clamp(chargerW, 0, clamp(in.householdW, 0, maximum)));
            out.reason = "pv_only_charge_disabled";
            return approve(out);
        }
        out.strategy = "preserve";
        out.reason = reserveOffCharging ? "reserve_off_charge" :
            (reserveOnCharging ? "reserve_on_charge" : (!out.reserveAllowed ? "reserve" :
            (in.futureValid ? in.futureReason :
             (band_ == Unknown ? "price_missing" : (band_ == Cheap ? "cheap" : "comfort")))));
        out.chargeTargetValid = true;
        // Reserve and economic charging share one target; neither counts the same energy twice.
        out.chargeTargetW = std::max(reserveChargeW,
            in.futureValid ? std::max(0.0f, in.futureChargeTargetW) : 0.0f);
        if (reserveOffCharging) {
            piActive_ = false; integral_ = 0;
            out.ceilingW = 0;
            return approve(out);
        }
        const float upper = clamp(chargerW * kDischargeEfficiency, 0, maximum);
        const float feedforward = clamp((chargerW - out.chargeTargetW) * kDischargeEfficiency, 0, upper);
        const float error = (in.batteryW - out.chargeTargetW) * kDischargeEfficiency;
        if (!piActive_) {
            piActive_ = true; integral_ = 0;
            ceiling_ = clamp(float(heldCommandW), 0, feedforward);
            lastTick_ = lastSampleAt_ = now;
            lastPiSequence_ = in.bmsSequence;
        }
        const float dt = clamp(float(uint32_t(now - lastTick_)) * 0.001f, 0, 10);
        lastTick_ = now;
        if (lastPiSequence_ != in.bmsSequence) {
            const float sampleDt = clamp(float(uint32_t(now - lastSampleAt_)) * 0.001f, 0, 10);
            lastSampleAt_ = now; lastPiSequence_ = in.bmsSequence;
            const bool demandLimited = float(heldCommandW) < ceiling_;
            const float next = clamp(integral_ + kPvKiPerSecond * error * sampleDt, -maximum, maximum);
            const float candidate = feedforward + kPvKp * error + next;
            // No positive windup while the house PID requests less than this ceiling.
            if ((!demandLimited || error < 0) &&
                !((candidate > upper && error > 0) || (candidate < 0 && error < 0))) integral_ = next;
        }
        const float target = clamp(feedforward + kPvKp * error + integral_, 0, upper);
        // A falling PV ceiling takes effect immediately; increases remain bounded.
        ceiling_ = target < ceiling_ ? target : clamp(target, 0, ceiling_ + kEconomicCeilingRampUpWPerSecond * dt);
        out.ceilingW = int(clamp(ceiling_, 0, maximum));
        return approve(out);
    }

private:
    CurrentDecision lastApproved_;
    bool approved_ = false;
    CurrentDecision approve(CurrentDecision out) {
        lastApproved_ = out; approved_ = true; return out;
    }
    enum Band { Unknown, Cheap, Comfort, Expensive };
    Band band_ = Unknown;
    bool reserveKnown_ = false, reserveAllowed_ = false;
    bool qualifying_ = false, qualified_ = false, piActive_ = false;
    uint8_t pairs_ = 0;
    uint32_t recoveryAt_ = 0, bmsSequence_ = 0, chargerSequence_ = 0;
    uint32_t lastTick_ = 0, lastSampleAt_ = 0, lastPiSequence_ = 0;
    float integral_ = 0, ceiling_ = 0, economicCeiling_ = 0;
    bool economicActive_ = false, pvOnly_ = false, batteryOnly_ = false, chargeAvailable_ = false, dischargeAvailable_ = false;
    uint32_t economicAt_ = 0;

    static float clamp(float value, float low, float high) {
        return value < low ? low : (value > high ? high : value);
    }
    void classify(const PreparedSettings &cfg, bool valid, float price) {
        if (!valid) { band_ = Unknown; return; }
        const float high = cfg.priceComfortEurPerKwh + cfg.priceDeadbandEurPerKwh;
        const float low = cfg.priceComfortEurPerKwh - cfg.priceDeadbandEurPerKwh;
        const float hysteresis = cfg.priceDeadbandEurPerKwh / 3.0f;
        if (band_ == Expensive && price > high - hysteresis) return;
        if (band_ == Cheap && price < low + hysteresis) return;
        band_ = price >= high ? Expensive : (price <= low ? Cheap : Comfort);
    }
    bool qualify(const CurrentInputs &in, uint32_t now, bool chargerOnly = false, bool batteryOnly = false) {
        if (qualified_) return true;
        if (!qualifying_) {
            qualifying_ = true; recoveryAt_ = now; pairs_ = 0;
            bmsSequence_ = in.bmsSequence; chargerSequence_ = in.chargerSequence;
        } else if (chargerOnly ? in.chargerSequence != chargerSequence_ :
                   (batteryOnly ? in.bmsSequence != bmsSequence_ :
                    in.bmsSequence != bmsSequence_ && in.chargerSequence != chargerSequence_)) {
            bmsSequence_ = in.bmsSequence; chargerSequence_ = in.chargerSequence;
            if (pairs_ < kRecoveryNewSamples) ++pairs_;
        }
        qualified_ = pairs_ >= kRecoveryNewSamples && uint32_t(now - recoveryAt_) >= kRecoveryQualificationMs;
        return qualified_;
    }
    // Keep the prior bounded permission/reserve latch while restarting dynamics from live held output.
    void pauseDynamics() {
        piActive_ = economicActive_ = false;
        integral_ = ceiling_ = economicCeiling_ = 0;
        lastTick_ = lastSampleAt_ = economicAt_ = 0;
        lastPiSequence_ = 0;
    }
    void resetDynamics() {
        approved_ = false;
        qualifying_ = qualified_ = false; pairs_ = 0;
        pauseDynamics();
    }
    CurrentDecision stop(CurrentDecision out, const char *reason) {
        resetDynamics(); out.minimumOverrideAllowed = false; out.direct = true; out.reason = reason;
        return out;
    }
};

} // namespace control
