#pragma once
#include "ConfigManager.h"
#include "SbtReceiver.h"
#include "SbtRuntimeMetadata.h"
#include "mqtt/MQTTManager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <ctime>

namespace sbt {
class Service {
  public:
    void attach(ConfigManagerClass &cfg, cm::MQTTManager &mqtt) {
        mqtt_ = &mqtt;
        mutex_ = xSemaphoreCreateMutex();
        cfg.addSetting(&inputRoot_);
        // The explicit key preserves NVS storage when changing the UI category.
        mqtt.onMQTTConnect([this]() {
            reset();
            subscribed_ = false;
        });
        mqtt.onMQTTDisconnect([this]() {
            reset();
            subscribed_ = false;
        });
        cfg.getRuntime().registerRuntimeAlarm("SBT charge protection");
        cfg.getRuntime().registerRuntimeAlarm("SBT discharge protection");
        mqtt.onNewMQTTMessage(
            [this, &cfg](const cm::MQTTManager::MqttMessageView &m) {
                if (!mutex_ || !m.topic)
                    return;
                const size_t n = root_.length();
                if (!n || std::strncmp(m.topic, root_.c_str(), n) ||
                    m.topic[n] != '/')
                    return;
                xSemaphoreTake(mutex_, portMAX_DELAY);
                receiver_.receive(m.topic + n + 1,
                                  reinterpret_cast<const char *>(m.payload),
                                  m.length, millis());
                const bool chargeAlarm = receiver_.chargeRestricted();
                const bool dischargeAlarm = receiver_.alarmRestricted();
                xSemaphoreGive(mutex_);
                cfg.getRuntime().setRuntimeAlarmActive("SBT charge protection", chargeAlarm);
                cfg.getRuntime().setRuntimeAlarmActive("SBT discharge protection", dischargeAlarm);
            });
        cfg.getRuntime().addRuntimeProvider(
            "sbt",
            [this](JsonObject &data) {
                if (!mutex_)
                    return;
                xSemaphoreTake(mutex_, portMAX_DELAY);
                receiver_.writeRuntime(data, millis());
                xSemaphoreGive(mutex_);
            },
            4);
        const char *page = "BMS / MPPT 150/35";
        cfg.addLivePage(page, 30);
        cfg.addLiveCard(page, "Source Health", 10);
        cfg.addLiveCard(page, "BMS", 20);
        cfg.addLiveCard(page, "MPPT 150/35", 30);
        cfg.addLiveCard(page, "MOS Control Status", 40);
        if(!registerRuntimeMetadata(cfg.getRuntime())) return;

    }
    void configureTimeout(unsigned seconds) {
        if (!mutex_) return;
        xSemaphoreTake(mutex_, portMAX_DELAY);
        receiver_.setTimeoutSeconds(seconds);
        xSemaphoreGive(mutex_);
    }
    void loop() {
        if (!mqtt_ || !mutex_)
            return;
        const String next = inputRoot_.get();
        if (next != root_) {
            if (subscribed_)
                mqtt_->unsubscribe((root_ + "/#").c_str());
            root_ = next;
            subscribed_ = false;
            reset(true);
        }
        if (!mqtt_->isConnected()) {
            if (subscribed_) {
                reset();
                subscribed_ = false;
            }
            return;
        }
        if (!subscribed_ && validRoot(root_) &&
            uint32_t(millis() - lastSubscribe_) >= 1000) {
            lastSubscribe_ = millis();
            subscribed_ = mqtt_->subscribe((root_ + "/#").c_str());
        }
        if (subscribed_) {
            char payload[256], requestId[33];
            std::snprintf(requestId,sizeof(requestId),"sil-%08lx-%08lx",
                static_cast<unsigned long>(requestNonce_),static_cast<unsigned long>(++requestSequence_));
            // Recheck the current gate/session/fault immediately before consuming the episode.
            xSemaphoreTake(mutex_, portMAX_DELAY);
            const auto utc = std::time(nullptr);
            const bool send = receiver_.takeEmergencyOff(millis(),utc > 0 ? static_cast<uint64_t>(utc) : 0,
                requestId,payload,sizeof(payload));
            xSemaphoreGive(mutex_);
            if (send) {
#if defined(SIL_MOCK_TRANSPORT) && SIL_MOCK_TRANSPORT
                Serial.println("[I] [MOCKED!] BMS emergency TX suppressed");
#else
                mqtt_->publishRaw((root_ + "/" + Receiver::emergencyOffTopic()).c_str(),payload,false);
#endif
            }
        }
    }

    double liveMeasurement(const char *topic, uint32_t now) {
        (void)now;
        if (!mutex_) return NAN;
        xSemaphoreTake(mutex_, portMAX_DELAY);
        const double value = receiver_.cachedNumeric(topic);
        xSemaphoreGive(mutex_);
        return value;
    }

    struct ControlSnapshot {
        bool bmsValid = false, chargerValid = false, chargerStandby = false, bmsWithinGrace = false, sourcesWithinGrace = false, alarmRestricted = false;
        bool genuineBmsFault = false, chargerFaultKnown = false;
        bool batteryPathAvailable = false, batteryChargeAvailable = false, batteryChargeConfirmed = false, softwareMosLockOnly = false, chargerFault = false;
        bool batteryChargeStateKnown = false; // Diagnostic distinction: blocked is not missing telemetry.
        bool batteryDischargeIntentAvailable = false;
        float soc = 0, batteryW = 0, chargerW = 0, chargerVoltageV = 0, batteryVoltageV = 0, fullCapacityAh = NAN;
        uint32_t bmsSequence = 0, chargerSequence = 0;
    };
    struct DiagnosticSnapshot {
        bool online = false, fresh[2] = {}, chargeStateValid = false, protectionValid = false;
        uint32_t sequence[2] = {}, validMask = 0;
        double values[channelCount] = {};
        char reason[2][64] = {{0}, {0}};
        char chargeState[32] = {}, protection[128] = {};
    };
    DiagnosticSnapshot diagnosticSnapshot(uint32_t now) {
        DiagnosticSnapshot s;
        if (!mutex_) return s;
        xSemaphoreTake(mutex_, portMAX_DELAY);
        const auto bms = receiver_.sourceHealth(0);
        const auto victron = receiver_.sourceHealth(1);
        s.online = bms.online || victron.online;
        s.fresh[0] = receiver_.fresh(0, now);
        s.fresh[1] = receiver_.fresh(1, now);
        s.sequence[0] = bms.sequence; s.sequence[1] = victron.sequence;
        std::snprintf(s.reason[0], sizeof(s.reason[0]), "%s", bms.reason);
        std::snprintf(s.reason[1], sizeof(s.reason[1]), "%s",
            receiver_.chargerStandby(now) ? "standby_discharge_mos_off" : victron.reason);
        receiver_.diagnosticValues(now, s.values, s.validMask);
        s.chargeStateValid = receiver_.copyDiagnosticText("victron/charge_state", now, s.chargeState, sizeof(s.chargeState));
        s.protectionValid = receiver_.copyDiagnosticText("bms/protection", now, s.protection, sizeof(s.protection));
        xSemaphoreGive(mutex_);
        return s;
    }
    ControlSnapshot controlSnapshot(uint32_t now) {
        ControlSnapshot s;
        if (!mutex_) return s;
        xSemaphoreTake(mutex_, portMAX_DELAY);
        now = millis(); // Never compare a concurrently received sample with an older loop timestamp.
        s.bmsWithinGrace = receiver_.withinGrace(0, now);
        s.sourcesWithinGrace = s.bmsWithinGrace && receiver_.withinGrace(1, now);
        s.alarmRestricted = receiver_.alarmRestricted();
        s.genuineBmsFault = receiver_.genuineAlarmRestricted();
        s.softwareMosLockOnly = receiver_.softwareMosLockOnly();
        s.batteryPathAvailable = receiver_.batteryPathAvailable(now);
        s.batteryDischargeIntentAvailable = receiver_.batteryDischargeIntentAvailable(now);
        s.batteryChargeAvailable = receiver_.batteryChargeAvailable(now);
        s.batteryChargeConfirmed = receiver_.batteryChargeConfirmed(now);
        s.batteryChargeStateKnown = receiver_.valid("bms/alarm_bits", now) && receiver_.valid("bms/charge_enabled", now);
        s.chargerFaultKnown = receiver_.valid("victron/error_code", now);
        s.chargerFault = s.chargerFaultKnown && receiver_.numeric("victron/error_code") != 0;
        s.bmsValid = receiver_.valid("bms/state_of_charge_pct", now) &&
                     receiver_.valid("bms/power_w", now);
        s.chargerValid = receiver_.valid("victron/battery_voltage_v", now) &&
                         receiver_.valid("victron/charge_current_a", now);
        s.chargerStandby = receiver_.chargerStandby(now);
        if (receiver_.valid("bms/voltage_v", now)) {
            const double voltage = receiver_.numeric("bms/voltage_v");
            if (std::isfinite(voltage) && voltage > 0) s.batteryVoltageV = voltage;
        }
        if (s.bmsValid) {
            s.soc = receiver_.numeric("bms/state_of_charge_pct");
            s.batteryW = receiver_.numeric("bms/power_w");
            s.bmsValid = s.soc >= 0 && s.soc <= 100 && std::isfinite(s.batteryW);
        }
        if (s.chargerValid) {
            const double v = receiver_.numeric("victron/battery_voltage_v");
            const double a = receiver_.numeric("victron/charge_current_a");
            s.chargerVoltageV = v;
            s.chargerW = v * a;
            s.chargerValid = v > 0 && a >= 0 && std::isfinite(s.chargerW);
        }
        if (receiver_.valid("bms/full_capacity_ah", now))
            s.fullCapacityAh = receiver_.numeric("bms/full_capacity_ah");
        s.bmsSequence = receiver_.sequence(0);
        s.chargerSequence = receiver_.sequence(1);
        xSemaphoreGive(mutex_);
        return s;
    }
  private:
    Config<String> inputRoot_{
        ConfigOptions<String>{.key = "SBTInputRoot",
                              .name = "SBT MQTT Input Root",
                              .category = "MQTT-Topics",
                              .defaultValue = String("SolarBatteryTelemetry"),
                              .sortOrder = 1,
                              .categoryPretty = "MQTT Topics",
                              .card = "MQTT-Topics",
                              .cardPretty = "MQTT-Received",
                              .cardOrder = 50}};
    Receiver receiver_;
    cm::MQTTManager *mqtt_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;
    String root_;
    bool subscribed_ = false;
    uint32_t lastSubscribe_ = 0;
    uint32_t requestNonce_ = esp_random(), requestSequence_ = 0;
    void reset(bool rootChanged = false) {
        if (mutex_) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
            if (rootChanged) receiver_.reset();
            else receiver_.disconnect();
            xSemaphoreGive(mutex_);
        }
    }
    static bool validRoot(const String &root) {
        if (root.isEmpty() || root.length() > 63 || root[0] == '/' ||
            root[root.length() - 1] == '/')
            return false;
        for (unsigned i = 0; i < root.length(); ++i) {
            const char c = root[i];
            if (c <= 32 || c >= 127 || c == '+' || c == '#' || c == '"' ||
                c == '\\' || (c == '/' && i && root[i - 1] == '/'))
                return false;
        }
        return true;
    }
};
} // namespace sbt
