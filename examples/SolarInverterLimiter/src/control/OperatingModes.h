#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace control {
enum class OperatingMode : uint8_t { Automatic=0, HouseholdFollow=1, ChargePriority=2, PvFollow=3 };
struct ModeSettings { OperatingMode mode = OperatingMode::Automatic; float chargeThreshold = 95; int inverterSharePercent = 0; };
inline bool parseOperatingMode(const char *text, OperatingMode &mode) {
    if (!text) return false;
    if (!std::strcmp(text,"automatic")) mode=OperatingMode::Automatic;
    else if (!std::strcmp(text,"household_follow")) mode=OperatingMode::HouseholdFollow;
    else if (!std::strcmp(text,"charge_priority")) mode=OperatingMode::ChargePriority;
    else if (!std::strcmp(text,"pv_follow")) mode=OperatingMode::PvFollow;
    else return false;
    return true;
}
inline const char *operatingModeName(OperatingMode mode) {
    switch (mode) {
        case OperatingMode::Automatic: return "automatic";
        case OperatingMode::HouseholdFollow: return "household_follow";
        case OperatingMode::ChargePriority: return "charge_priority";
        case OperatingMode::PvFollow: return "pv_follow";
    }
    return "invalid";
}
inline bool validModeSettings(const ModeSettings &s) {
    return s.mode <= OperatingMode::PvFollow && std::isfinite(s.chargeThreshold) &&
        s.chargeThreshold > 0 && s.chargeThreshold < 100 &&
        s.inverterSharePercent >= 0 && s.inverterSharePercent <= 100;
}
inline bool sameModeSettings(const ModeSettings &a,const ModeSettings &b) {
    return a.mode == b.mode && a.chargeThreshold == b.chargeThreshold && a.inverterSharePercent == b.inverterSharePercent;
}
struct ModeInputs {
    bool metersValid=false,bmsValid=false,dischargeAllowed=false,chargeKnown=false,chargeAllowed=false;
    // genuineFault is a directional discharge restriction, not a charge-only alarm.
    bool settingsValid=true,inverterEnabled=true,transportEnabled=true,physicalDischargeOff=false,genuineFault=false,chargerFault=false,chargerValid=false;
    float householdW=0,soc=0,chargerW=0,reserveOff=2,reserveOn=10;
    int correctionW=0,maximumW=0;
};
struct ModeDecision { bool handled=true; int watts=0; const char *effective="hold",*reason="startup"; };
class OperatingModePolicy {
    bool full_ = false, reserveAllowed_ = false;
public:
    ModeDecision update(const ModeSettings &cfg,const ModeInputs &in) {
        ModeDecision out;
        auto stop = [&](const char *reason) { out.reason=reason; return out; };
        if (!in.settingsValid || !validModeSettings(cfg) || !std::isfinite(in.reserveOff) || !std::isfinite(in.reserveOn) ||
            in.reserveOff < 0 || in.reserveOff >= in.reserveOn || in.reserveOn > 100) return stop("settings");
        const bool bms = in.bmsValid && std::isfinite(in.soc) && in.soc >= 0 && in.soc <= 100;
        if (cfg.mode != OperatingMode::ChargePriority) full_=false;
        // Source state keeps advancing while output gates are closed. Otherwise
        // an OFF/meter gap could hide a reserve or full-charge release crossing.
        if (bms) {
            if (in.soc <= in.reserveOff) reserveAllowed_=false;
            else if (in.soc >= in.reserveOn) reserveAllowed_=true;
            if (cfg.mode == OperatingMode::ChargePriority) {
                if (in.soc < cfg.chargeThreshold) full_=false;
                if (in.soc >= 100) full_=true;
            }
        } else if (cfg.mode == OperatingMode::ChargePriority) full_=false;
        if (!in.inverterEnabled) return stop("inverter_off");
        if (!in.transportEnabled) return stop("transport_off");
        if (!in.metersValid || !std::isfinite(in.householdW)) return stop("meters_missing");
        if (in.maximumW <= 0) return stop("maximum_zero");
        if (in.genuineFault) return stop("source_alarm");
        if (in.chargerFault) return stop("charger_fault");
        if (cfg.mode == OperatingMode::Automatic) {
            if (!bms) return stop("bms_unverified");
            out.handled=false; out.effective="automatic"; out.reason="automatic"; return out;
        }
        const double corrected = double(in.householdW) + double(in.correctionW);
        const int demand = int(std::max(0.0,std::min(corrected,double(in.maximumW))));
        if (cfg.mode == OperatingMode::PvFollow) {
            // This request spends current measured PV only. Reserve or absent
            // discharge permission does not grant or consume stored energy.
            if (!in.chargerValid || !std::isfinite(in.chargerW) || in.chargerW < 0)
                return stop("pv_unverified");
            out.watts=int(std::min(double(demand),double(in.chargerW)*0.9));
            out.effective="pv_follow";
            out.reason=bms ? "pv_follow" : "pv_bms_unverified";
            return out;
        }
        if (bms) {
            // Only fresh physical MOS feedback permits uncapped direct PV demand.
            if (!in.physicalDischargeOff && (!in.dischargeAllowed || !reserveAllowed_))
                return stop(!in.dischargeAllowed ? "discharge_blocked" : "reserve");
        }
        if (cfg.mode == OperatingMode::ChargePriority) {
            if (!bms) { full_=false; return stop("soc_unknown"); }
            if (in.chargeKnown && !in.chargeAllowed) {
                out.reason="charge_unavailable_follow";
            } else if (!full_) {
                if (in.soc < cfg.chargeThreshold || cfg.inverterSharePercent == 0)
                    return stop("charge_priority");
                if (!in.chargerValid || !std::isfinite(in.chargerW) || in.chargerW < 0)
                    return stop("pv_unverified");
                out.watts=int(std::min(double(demand),double(in.chargerW)*0.9*cfg.inverterSharePercent/100.0));
                out.effective="charge_priority"; out.reason="charge_pv_share"; return out;
            } else out.reason="full_household_follow";
        } else out.reason=bms ? "household_follow" : "manual_bms_unverified";
        out.watts=demand; out.effective="household_follow"; return out;
    }
};
struct ModeCommand { OperatingMode mode=OperatingMode::Automatic; uint32_t expiresAt=0; };
// This deliberately small JSON grammar rejects duplicate/unknown keys, escapes,
// embedded NUL, fractions and trailing content. Commands are valid for at most 60 s.
inline bool parseModeCommand(const char *data,size_t length,uint32_t now,ModeCommand &out) {
    if (!data || !length || length > 128 || now < 1704067200UL) return false;
    size_t p=0; bool modeSeen=false,expirySeen=false; ModeCommand candidate;
    auto ws=[&]() { while(p<length && (data[p]==' ' || data[p]=='\n' || data[p]=='\r' || data[p]=='\t')) ++p; };
    auto token=[&](char *dst,size_t capacity) {
        ws(); if(p>=length || data[p++]!='"') return false;
        size_t used=0;
        while(p<length && data[p]!='"') {
            if (data[p]<' ' || data[p]=='\\' || used+1>=capacity) return false;
            dst[used++]=data[p++];
        }
        if(p>=length) return false;
        ++p; dst[used]=0; return true;
    };
    ws(); if(p>=length || data[p++]!='{') return false;
    for(unsigned field=0;field<2;++field) {
        char key[16]; if(!token(key,sizeof(key))) return false;
        ws(); if(p>=length || data[p++]!=':') return false;
        if(!std::strcmp(key,"mode") && !modeSeen) {
            char value[24]; if(!token(value,sizeof(value)) || !parseOperatingMode(value,candidate.mode)) return false;
            modeSeen=true;
        } else if(!std::strcmp(key,"expires_at") && !expirySeen) {
            ws(); if(p>=length || data[p]<'1' || data[p]>'9') return false;
            uint64_t value=0;
            while(p<length && data[p]>='0' && data[p]<='9') {
                value=value*10+unsigned(data[p++]-'0'); if(value>UINT32_MAX) return false;
            }
            candidate.expiresAt=uint32_t(value); expirySeen=true;
        } else return false;
        ws(); if(field==0 && (p>=length || data[p++]!=',')) return false;
    }
    ws(); if(p>=length || data[p++]!='}') return false;
    ws(); if(p!=length || !modeSeen || !expirySeen || candidate.expiresAt<=now || candidate.expiresAt-now>60) return false;
    out=candidate; return true;
}
class ModeTransaction {
    ModeSettings confirmed_;
    ModeSettings pending_;
    bool pendingKnown_=false;
    uint32_t pendingExpiry_=0;
public:
    const ModeSettings &confirmed() const { return confirmed_; }
    void initialize(ModeSettings settings) { confirmed_=validModeSettings(settings)?settings:ModeSettings{}; pendingKnown_=false; }
    bool request(ModeSettings settings,uint32_t expiresAt=0) {
        if(!validModeSettings(settings)) return false;
        pending_=settings; pendingExpiry_=expiresAt; pendingKnown_=true; return true;
    }
    template<class Persist> bool process(Persist persist,uint32_t now=0) {
        if(!pendingKnown_) return false;
        pendingKnown_=false;
        if(pendingExpiry_ && (now<1704067200UL || now>=pendingExpiry_ || pendingExpiry_-now>60)) return false;
        if(sameModeSettings(pending_,confirmed_) || !persist(pending_)) return false;
        confirmed_=pending_; return true;
    }
};
}
