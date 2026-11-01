#pragma once
#include "RuntimeProgress.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace sil {
static RTC_NOINIT_ATTR ProgressRecord retainedProgress;
class RuntimeDiagnostics {
public:
    RuntimeDiagnostics() : progress_(retainedProgress) {}
    void begin(const char *version, bool mock) {
        reset_ = uint32_t(esp_reset_reason());
        previous_ = progress_.begin(millis(), reset_ != ESP_RST_POWERON && reset_ != ESP_RST_BROWNOUT);
        Serial.printf("[I] DIAG boot version=%s mac=%s mock=%u reset=%lu boot=%lu prev=%s prev_ms=%lu prev_control_ms=%lu\n",
            version, WiFi.macAddress().c_str(), unsigned(mock), ulong(reset_), ulong(retainedProgress.bootCount),
            previous_.valid ? stageName(previous_.stage) : "unknown", ulong(previous_.stageAtMs), ulong(previous_.controlAtMs));
    }
    void loop() { progress_.loop(millis()); }
    void enter(RuntimeStage stage) { progress_.enter(stage, millis()); }
    void controlCompleted() { progress_.controlCompleted(millis()); }
    template<class Stats> void report(const Stats &stats, uint32_t priceRevision, uint32_t pvRevision) {
        const uint32_t now = millis();
        if (!progress_.reportDue(now)) return;
        const IPAddress ip = WiFi.localIP();
        Serial.printf("[I] DIAG ms=%lu boot=%lu reset=%lu ip=%u.%u.%u.%u gap=%lu peak=%s stage_ms=%lu control_age=%lu heap=%u min=%u block=%u stack=%u integrity=%u rx_to=%lu rx_bad=%lu rx_big=%lu tx_fail=%lu conn_fail=%lu txBackpressure=%lu txPartialWrites=%lu txTimeouts=%lu txOverflows=%lu price_rev=%lu pv_rev=%lu peak_at=%lu loops=%lu slow_stage=%s slow_stage_ms=%lu slow_stage_at=%lu slow_loop_ms=%lu slow_loop_at=%lu\n",
            ulong(now), ulong(retainedProgress.bootCount), ulong(reset_), unsigned(ip[0]), unsigned(ip[1]), unsigned(ip[2]), unsigned(ip[3]), ulong(progress_.maxLoopGapMs()),
            stageName(progress_.peakStage()), ulong(progress_.maxStageMs()), ulong(progress_.controlAgeMs(now)),
            unsigned(ESP.getFreeHeap()), unsigned(ESP.getMinFreeHeap()), unsigned(ESP.getMaxAllocHeap()),
            unsigned(uxTaskGetStackHighWaterMark(nullptr)), unsigned(heap_caps_check_integrity_all(false)),
            ulong(stats.rxTimeouts), ulong(stats.rxMalformed), ulong(stats.rxOversize), ulong(stats.txFailures),
            ulong(stats.connectFailures), ulong(stats.txBackpressure), ulong(stats.txPartialWrites),
            ulong(stats.txTimeouts), ulong(stats.txOverflows), ulong(priceRevision), ulong(pvRevision),
            ulong(progress_.peakStageEndedAtMs()), ulong(progress_.loops()), stageName(progress_.lastSlowStage()),
            ulong(progress_.lastSlowStageMs()), ulong(progress_.lastSlowStageEndedAtMs()),
            ulong(progress_.lastSlowLoopMs()), ulong(progress_.lastSlowLoopEndedAtMs()));
    }
    template<class Stats> String mqttPayload(const Stats &stats) const {
        // Worst-case UINT32 fields and seven-byte stage names: 631 bytes plus NUL.
        char out[640];
        const int written = snprintf(out, sizeof(out), "{\"boot\":%lu,\"resetReason\":%lu,\"previousStage\":\"%s\",\"previousStageMs\":%lu,\"maxLoopMs\":%lu,\"maxStageMs\":%lu,\"peakStage\":\"%s\",\"peakStageEndedAtMs\":%lu,\"controlAgeMs\":%lu,\"loops\":%lu,\"lastSlowStage\":\"%s\",\"lastSlowStageMs\":%lu,\"lastSlowStageEndedAtMs\":%lu,\"lastSlowLoopMs\":%lu,\"lastSlowLoopEndedAtMs\":%lu,\"rxTimeouts\":%lu,\"rxMalformed\":%lu,\"rxOversize\":%lu,\"txFailures\":%lu,\"connectFailures\":%lu,\"txBackpressure\":%lu,\"txPartialWrites\":%lu,\"txTimeouts\":%lu,\"txOverflows\":%lu}",
            ulong(retainedProgress.bootCount), ulong(reset_), previous_.valid ? stageName(previous_.stage) : "unknown",
            ulong(previous_.stageAtMs), ulong(progress_.maxLoopGapMs()), ulong(progress_.maxStageMs()),
            stageName(progress_.peakStage()), ulong(progress_.peakStageEndedAtMs()),
            ulong(progress_.controlAgeMs(millis())), ulong(progress_.loops()), stageName(progress_.lastSlowStage()),
            ulong(progress_.lastSlowStageMs()), ulong(progress_.lastSlowStageEndedAtMs()),
            ulong(progress_.lastSlowLoopMs()), ulong(progress_.lastSlowLoopEndedAtMs()),
            ulong(stats.rxTimeouts), ulong(stats.rxMalformed), ulong(stats.rxOversize), ulong(stats.txFailures),
            ulong(stats.connectFailures), ulong(stats.txBackpressure), ulong(stats.txPartialWrites),
            ulong(stats.txTimeouts), ulong(stats.txOverflows));
        if (written < 0 || static_cast<size_t>(written) >= sizeof(out)) return String("{\"diagnosticError\":\"format\"}");
        return String(out);
    }
private:
    static unsigned long ulong(uint32_t value) { return static_cast<unsigned long>(value); }
    RuntimeProgress progress_;
    PreviousProgress previous_;
    uint32_t reset_ = 0;
};
} // namespace sil
