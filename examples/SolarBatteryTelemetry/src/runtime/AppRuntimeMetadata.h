#pragma once
#include <runtime/RuntimeFieldTypes.h>
#include "../telemetry/TelemetryChannels.h"
#include <type_traits>
#include <functional>

namespace sbt::metadata {
// Keep metadata separate from providers and control callbacks.
template<class Meta, class Runtime>
bool field(Runtime& runtime, const char* source, const char* key, const char* label,
           int order, const char* page, const char* card, const char* group,
           const char* unit = "", int precision = 2) {
#if defined(CM_ENABLE_LIVE_UI) && !CM_ENABLE_LIVE_UI
    // Disabled CM Live APIs intentionally return invalid IDs; Settings-only startup is valid.
    (void)runtime; (void)source; (void)key; (void)label; (void)order;
    (void)page; (void)card; (void)group; (void)unit; (void)precision;
    return true;
#else
    Meta meta;
    meta.base.sourceGroup = source;
    meta.base.key = key;
    meta.base.label = label;
    meta.base.order = order;
    meta.base.page = page;
    meta.base.card = card;
    meta.base.group = group;
    if constexpr (std::is_same_v<Meta, RuntimeFloatMeta> || std::is_same_v<Meta, RuntimeIntMeta>) meta.unit = unit;
    if constexpr (std::is_same_v<Meta, RuntimeFloatMeta>) meta.precision = precision;
    return static_cast<bool>(runtime.addRuntimeMeta(meta));
#endif
}

template<class Meta, class Runtime>
bool smokeField(Runtime& runtime, const char* source, const char* key, const char* label, int order) {
    return field<Meta>(runtime, source, key, label, order, "Connectivity", "Connectivity", "Bluetooth");
}

template<class Runtime> bool registerSmokeFields(Runtime& runtime) {
    return smokeField<RuntimeTextMeta>(runtime, "smoke", "scope", "Scope", 1) &&
        smokeField<RuntimeBoolMeta>(runtime, "smoke", "workerReady", "BLE worker ready", 2) &&
        smokeField<RuntimeIntMeta>(runtime, "smoke", "resetReason", "Reset reason", 3) &&
        smokeField<RuntimeBoolMeta>(runtime, "bluetooth", "enabled", "Bluetooth enabled", 10) &&
        smokeField<RuntimeIntMeta>(runtime, "bluetooth", "state", "Transport state", 11) &&
        smokeField<RuntimeBoolMeta>(runtime, "bluetooth", "connected", "GATT connected", 12) &&
        smokeField<RuntimeBoolMeta>(runtime, "bluetooth", "scanning", "Scanning", 13) &&
        smokeField<RuntimeIntMeta>(runtime, "bluetooth", "queueDrops", "Queue drops", 14) &&
        smokeField<RuntimeIntMeta>(runtime, "bluetooth", "lastError", "Last transport error", 15) &&
        smokeField<RuntimeIntMeta>(runtime, "bluetooth", "lateEvents", "Suppressed late events", 16) &&
        smokeField<RuntimeIntMeta>(runtime, "bluetooth", "workerStackHighWaterBytes", "Worker stack free (bytes)", 17);
}

template<class Meta, class Runtime>
bool telemetryField(Runtime& runtime, const char* source, const char* key, const char* label,
                    int order, const char* card = "MQTT", const char* unit = "", int precision = 2) {
    const char* page = std::strcmp(source, "bms") == 0 ? "BMS" : std::strcmp(source, "victron") == 0 ? "Victron" : "Connectivity";
    return field<Meta>(runtime, source, key, label, order, page, std::strcmp(source, "telemetry") == 0 ? "Connectivity" : card, card, unit, precision);
}

template<class Runtime> bool registerTelemetryFields(Runtime& runtime) {
    if (!telemetryField<RuntimeTextMeta>(runtime, "telemetry", "connection", "MQTT connection", 3) ||
        !telemetryField<RuntimeTextMeta>(runtime, "telemetry", "mqttEnabled", "MQTT enabled", 4) ||
        !telemetryField<RuntimeTextMeta>(runtime, "telemetry", "snapshot", "Latest snapshot", 6) ||
        !telemetryField<RuntimeTextMeta>(runtime, "telemetry", "retirement", "Retained status cleanup", 8)) return false;
    for (telemetry::Source source : {telemetry::Source::Bms, telemetry::Source::Victron}) {
        const char* name = telemetry::sourceName(source);
        if (!telemetryField<RuntimeTextMeta>(runtime, name, "reason", "Source", 2, name)) return false;
    }
    for (const char* key : {"control_enabled", "control_ready", "control_pending", "control_admission", "control_result", "control_reason", "control_request_id", "sample_sequence", "sample_age_ms"}) {
        const bool isBool = std::strcmp(key, "control_enabled") == 0 || std::strcmp(key, "control_ready") == 0 || std::strcmp(key, "control_pending") == 0;
        const bool isInt = std::strcmp(key, "sample_sequence") == 0 || std::strcmp(key, "sample_age_ms") == 0;
        const bool registered = isBool ? telemetryField<RuntimeBoolMeta>(runtime, "bms", key, key, 50, "bms") :
            isInt ? telemetryField<RuntimeIntMeta>(runtime, "bms", key, key, 50, "bms") :
            telemetryField<RuntimeTextMeta>(runtime, "bms", key, key, 50, "bms");
        if (!registered) return false;
    }
    if (!telemetryField<RuntimeBoolMeta>(runtime,"bms","permissions_known","Actual MOS states available",51,"bms") ||
        !telemetryField<RuntimeTextMeta>(runtime,"bms","mos_readback","MOS readback",52,"bms") ||
        !telemetryField<RuntimeCheckboxMeta>(runtime,"bms","requested_charge_enabled","Allow charging (actual MOS)",53,"bms") ||
        !telemetryField<RuntimeCheckboxMeta>(runtime,"bms","requested_discharge_enabled","Allow discharging (actual MOS)",54,"bms")) return false;
    int order = 10;
    for (const auto& channel : telemetry::channels) {
        const char* source = telemetry::sourceName(channel.source);
        const char* key = std::strchr(channel.suffix, '/') + 1;
        bool registered = false;
        switch (channel.type) {
        case telemetry::FieldType::Boolean:
            registered = telemetryField<RuntimeBoolMeta>(runtime, source, key, channel.label, order, source);
            break;
        case telemetry::FieldType::Real: {
            const bool cellVolts = std::strcmp(key, "cell_voltage_min_v") == 0 || std::strcmp(key, "cell_voltage_max_v") == 0 || std::strcmp(key, "cell_voltage_delta_v") == 0;
            registered = telemetryField<RuntimeFloatMeta>(runtime, source, key, channel.label, order, source, channel.unit, cellVolts ? 3 : 2);
            break;
        }
        case telemetry::FieldType::Unsigned:
            registered = telemetryField<RuntimeIntMeta>(runtime, source, key, channel.label, order, source, channel.unit);
            break;
        case telemetry::FieldType::State:
        case telemetry::FieldType::Protection:
            registered = telemetryField<RuntimeTextMeta>(runtime, source, key, channel.label, order, source);
            break;
        }
        if (!registered) return false;
        ++order;
    }
    return true;
}

inline constexpr const char* bmsCheckboxKeys[] = {"requested_charge_enabled", "requested_discharge_enabled"};
// The provider owns nullable acknowledged requests; setters only request a guarded change.
template<class Runtime, class Request>
void registerBmsCheckboxes(Runtime& runtime, Request request) {
    for (unsigned i = 0; i < 2; ++i)
        runtime.registerRuntimeCheckbox("bms", bmsCheckboxKeys[i], std::function<bool()>{},
            [request, i](bool state) { request(i, state); });
}
template<class Runtime, class Request>
void registerBmsPairButtons(Runtime& runtime, Request request) {
    for(unsigned mask=0;mask<4;++mask){char key[24];std::snprintf(key,sizeof(key),"set_permissions_%u",mask);
        runtime.registerRuntimeButton("bms",key,[request,mask](){request(mask);});}
}
}
