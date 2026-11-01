#pragma once
#include "SbtReceiver.h"
#include "runtime/CompactLiveMetadata.h"

namespace sbt {
template<class Runtime> bool registerRuntimeMetadata(Runtime& runtime) {
    const char* page="BMS / MPPT 150/35";
    bool valid=true;
    auto meta = [&runtime,&valid,page](const char* key,const char* label,const char* card,
                                      Kind kind,int order,const char* unit="",int precision=2) {
        if(!valid) return;
        RuntimeFieldId id;
        switch(kind) {
        case Kind::Boolean: id=sil::compactField<RuntimeBoolMeta>(runtime,"sbt",page,card,card,key,label,order);break;
        case Kind::Text: id=sil::compactField<RuntimeTextMeta>(runtime,"sbt",page,card,card,key,label,order);break;
        case Kind::Unsigned: id=sil::compactField<RuntimeIntMeta>(runtime,"sbt",page,card,card,key,label,order,unit);break;
        case Kind::Real:
        case Kind::Unsupported: id=sil::compactField<RuntimeFloatMeta>(runtime,"sbt",page,card,card,key,label,order,unit,precision);break;
        }
        valid=static_cast<bool>(id);
    };

        meta("online", "SBT online", "Source Health", Kind::Boolean, 0);
        meta("bms_fresh", "BMS data fresh", "Source Health", Kind::Boolean, 1);
        meta("bms_reason", "BMS source reason", "Source Health", Kind::Text, 2);
        meta("victron_fresh", "MPPT data fresh", "Source Health", Kind::Boolean, 3);
        meta("victron_reason", "MPPT source reason", "Source Health", Kind::Text, 4);
        meta("bms_age", "BMS data", "Source Health", Kind::Text, 5);
        meta("victron_age", "MPPT data", "Source Health", Kind::Text, 6);
        meta("commands_locked", "Emergency stop blocked", "MOS Control Status", Kind::Boolean, 0);
        meta("emergency_state", "Emergency stop", "MOS Control Status", Kind::Text, 6);
        meta("remote_control_enabled", "SBT control enabled", "MOS Control Status", Kind::Boolean, 1);
        meta("remote_control_ready", "SBT control ready", "MOS Control Status", Kind::Boolean, 2);
        meta("remote_control_pending", "SBT command pending", "MOS Control Status", Kind::Boolean, 3);
        meta("control_reason", "SBT control reason", "MOS Control Status", Kind::Text, 4);
        meta("control_result", "Last SBT control result", "MOS Control Status", Kind::Text, 5);
        // Entries follow the receiver contract, including explicitly unsupported
        // channels which remain null rather than displaying invented values.
        static const char *const labels[] = {
            "Charging", "Discharging", "State of charge", "Battery voltage",
            "Battery current", "Battery power", "Remaining capacity", "Full capacity",
            "Charge MOS enabled", "Discharge MOS enabled", "Maximum temperature",
            "Minimum cell voltage", "Maximum cell voltage", "Cell voltage difference",
            "Alarm bits", "Solar power", "PV voltage (unsupported)",
            "PV current (unsupported)", "Battery voltage", "Charge current",
            "Battery power (unsupported)", "Load current", "Charge state", "Error code",
            "Yield today", "Minimum temperature", "Cell count", "Temperature sensor count",
            "Protection status", "NTC 1 (Top)", "NTC 2 (Bottom)"};
        static const char *const units[] = {
            "", "", "%", "V", "A", "W", "Ah", "Ah", "", "", "\u00B0C",
            "V", "V", "V", "", "W", "V", "A", "V", "A", "W", "A",
            "", "", "Wh", "\u00B0C", "", "", "", "\u00B0C", "\u00B0C"};
        static_assert(sizeof(labels) / sizeof(labels[0]) == channelCount,
                      "Every SBT channel needs a display label");
        static_assert(sizeof(units) / sizeof(units[0]) == channelCount,
                      "Every SBT channel needs a unit");
        for (size_t i = 0; i < channelCount; ++i) {
            // Hide display details only; the complete receiver contract stays available.
            if (i == 3 || i == 4 || i == 5 || i == 6 || i == 7 || i == 10 ||
                i == 11 || i == 12 || i == 13 || i == 16 || i == 17 || i == 20 ||
                i == 25 || i == 26 || i == 27) continue;
            meta(channels[i].key, labels[i], channels[i].source ? "MPPT 150/35" : "BMS",
                 channels[i].kind, int(i) + 10, units[i],
                 channels[i].kind == Kind::Unsigned ? 0 : 2);
        }
    return valid;
}
}
