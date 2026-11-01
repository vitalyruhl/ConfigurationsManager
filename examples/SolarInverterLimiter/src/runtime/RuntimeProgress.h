#pragma once
#include <cstdint>

namespace sil {
enum class RuntimeStage : uint32_t { Boot, Wifi, Mqtt, Sbt, Publish, Control, Sensors, Config, Idle };
struct ProgressRecord {
    uint32_t magic, complement, bootCount;
    RuntimeStage stage;
    uint32_t stageAtMs, controlAtMs, loops;
};
struct PreviousProgress {
    bool valid = false;
    RuntimeStage stage = RuntimeStage::Boot;
    uint32_t stageAtMs = 0, controlAtMs = 0;
};
// Volatile RTC writes remain observable even with LTO and a reset inside the next call.
class RuntimeProgress {
public:
    explicit RuntimeProgress(volatile ProgressRecord &record) : record_(record) {}
    PreviousProgress begin(uint32_t now, bool preserve = true) {
        PreviousProgress previous;
        previous.valid = preserve && record_.magic == kMagic && record_.complement == ~kMagic &&
            uint32_t(record_.stage) <= uint32_t(RuntimeStage::Idle);
        uint32_t boots = 1;
        if (previous.valid) {
            previous.stage = record_.stage;
            previous.stageAtMs = record_.stageAtMs;
            previous.controlAtMs = record_.controlAtMs;
            boots = record_.bootCount + 1;
            if (!boots) boots = 1;
        }
        record_.magic = kMagic; record_.complement = ~kMagic; record_.bootCount = boots;
        record_.stage = RuntimeStage::Boot; record_.stageAtMs = now;
        record_.controlAtMs = 0; record_.loops = 0;
        lastReport_ = now; maxStageMs_ = maxLoopGapMs_ = lastLoop_ = 0; loopSeen_ = false;
        peakStage_ = lastSlowStage_ = RuntimeStage::Boot;
        peakStageEndedAtMs_ = lastSlowStageMs_ = lastSlowStageEndedAtMs_ = 0;
        lastSlowLoopMs_ = lastSlowLoopEndedAtMs_ = 0;
        return previous;
    }
    void enter(RuntimeStage next, uint32_t now) {
        const uint32_t elapsed = now - record_.stageAtMs;
        if (record_.stage != RuntimeStage::Boot && record_.stage != RuntimeStage::Idle) {
            if (elapsed > maxStageMs_) {
                maxStageMs_ = elapsed; peakStage_ = record_.stage; peakStageEndedAtMs_ = now;
            }
            if (elapsed >= kSlowThresholdMs) {
                lastSlowStage_ = record_.stage; lastSlowStageMs_ = elapsed; lastSlowStageEndedAtMs_ = now;
            }
        }
        record_.stageAtMs = now;
        record_.stage = next;
    }
    void loop(uint32_t now) {
        if (loopSeen_) {
            const uint32_t elapsed = now - lastLoop_;
            if (elapsed > maxLoopGapMs_) maxLoopGapMs_ = elapsed;
            if (elapsed >= kSlowThresholdMs) { lastSlowLoopMs_ = elapsed; lastSlowLoopEndedAtMs_ = now; }
        }
        loopSeen_ = true; lastLoop_ = now;
        record_.loops = record_.loops + 1;
    }
    void controlCompleted(uint32_t now) { record_.controlAtMs = now; }
    bool reportDue(uint32_t now) {
        if (now - lastReport_ < 5000) return false;
        lastReport_ = now; return true;
    }
    uint32_t maxStageMs() const { return maxStageMs_; }
    uint32_t maxLoopGapMs() const { return maxLoopGapMs_; }
    RuntimeStage peakStage() const { return peakStage_; }
    uint32_t peakStageEndedAtMs() const { return peakStageEndedAtMs_; }
    uint32_t controlAgeMs(uint32_t now) const { return now - record_.controlAtMs; }
    uint32_t loops() const { return record_.loops; }
    RuntimeStage lastSlowStage() const { return lastSlowStage_; }
    uint32_t lastSlowStageMs() const { return lastSlowStageMs_; }
    uint32_t lastSlowStageEndedAtMs() const { return lastSlowStageEndedAtMs_; }
    uint32_t lastSlowLoopMs() const { return lastSlowLoopMs_; }
    uint32_t lastSlowLoopEndedAtMs() const { return lastSlowLoopEndedAtMs_; }
private:
    static constexpr uint32_t kMagic = 0x53494c44u;
    static constexpr uint32_t kSlowThresholdMs = 500;
    volatile ProgressRecord &record_;
    uint32_t lastReport_ = 0, lastLoop_ = 0, maxStageMs_ = 0, maxLoopGapMs_ = 0;
    RuntimeStage peakStage_ = RuntimeStage::Boot;
    // Current-boot observations only; the retained RTC record layout is unchanged.
    RuntimeStage lastSlowStage_ = RuntimeStage::Boot;
    uint32_t peakStageEndedAtMs_ = 0, lastSlowStageMs_ = 0, lastSlowStageEndedAtMs_ = 0;
    uint32_t lastSlowLoopMs_ = 0, lastSlowLoopEndedAtMs_ = 0;
    bool loopSeen_ = false;
};
inline const char *stageName(RuntimeStage stage) {
    switch (stage) {
    case RuntimeStage::Boot: return "boot";
    case RuntimeStage::Wifi: return "wifi";
    case RuntimeStage::Mqtt: return "mqtt";
    case RuntimeStage::Sbt: return "sbt";
    case RuntimeStage::Publish: return "publish";
    case RuntimeStage::Control: return "control";
    case RuntimeStage::Sensors: return "sensors";
    case RuntimeStage::Config: return "config";
    case RuntimeStage::Idle: return "idle";
    }
    return "unknown";
}
static_assert(sizeof(ProgressRecord) <= 32, "RTC progress record must stay bounded");
} // namespace sil
