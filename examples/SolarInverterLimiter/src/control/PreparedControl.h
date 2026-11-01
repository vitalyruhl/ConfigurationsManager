#pragma once

#include <stdint.h>

// Preparation constants only. These values do not drive the active controller.
namespace control {

constexpr float kEconomicCeilingRampUpWPerSecond = 50.0f;
constexpr float kEconomicCeilingRampDownWPerSecond = 50.0f;
constexpr uint32_t kRecoveryQualificationMs = 5000UL;
constexpr uint8_t kRecoveryNewSamples = 2;
constexpr uint32_t kSourceStaleMs = 30000UL;
constexpr uint32_t kGridAcStaleMs = 20000UL;
constexpr uint32_t kAmbientStaleMs = 60000UL;
constexpr uint32_t kPendingCheckMs = 1000UL;
constexpr uint32_t kAlarmReminderMs = 300000UL;
constexpr uint32_t kReplanMs = 1800000UL;
constexpr uint32_t kTariffSlotMs = 900000UL;
constexpr uint8_t kLookAheadHours = 36;
constexpr uint8_t kExpensiveFallbackHours = 3;
constexpr uint8_t kAutomaticMosRetries = 0;
constexpr float kDischargeEfficiency = 0.90f;
constexpr float kRoundTripEfficiency = 0.90f;
constexpr float kWearEurPerKwh = 0.02f;
constexpr float kMinimumBenefitEurPerKwh = 0.03f;
constexpr float kBatteryFanOnC = 40.0f;
constexpr float kAmbientFanOnC = 35.0f;
constexpr float kTemperatureHysteresisK = 3.0f;
constexpr float kCellHighWarningV = 4.10f;
constexpr float kCellHighBlockV = 4.15f;
constexpr float kCellLowWarningV = 3.10f;
constexpr float kCellLowBlockV = 3.00f;
constexpr float kCellHysteresisV = 0.05f;

// Commissioning candidates require sensor and installed-limit qualification.
// Keep them out of active protection until that work is explicitly completed.
constexpr float kCandidateChargeHighWarningC = 40.0f;
constexpr float kCandidateChargeHighBlockC = 45.0f;
constexpr float kCandidateDischargeHighWarningC = 50.0f;
constexpr float kCandidateDischargeHighBlockC = 55.0f;
constexpr float kCandidateChargeLowWarningC = 5.0f;
constexpr float kCandidateChargeLowBlockC = 2.0f;
constexpr float kCandidateDischargeLowWarningC = -10.0f;
constexpr float kCandidateDischargeLowBlockC = -15.0f;

struct PreparedSettings {
    int idleOutputW = 500;
    float reserveOffSocPercent = 2.0f;
    float reserveOnSocPercent = 10.0f;
    bool mosManualMode = true;
    bool inverterEnabled = true;
    float priceComfortEurPerKwh = 0.25f;
    float priceDeadbandEurPerKwh = 0.06f;
    float priceHysteresisEurPerKwh = 0.02f;
    bool valid = false;
};

struct RuntimeState {
    bool reserveStateKnown = false;
    bool economicDischargeAllowed = false;
    bool recalculationDirty = false;
};

struct RuntimeContext {
    const PreparedSettings *settings = nullptr;
    const RuntimeState *state = nullptr;
};

// Inactive module boundaries for the accepted control diagrams. These methods
// intentionally do nothing and are not connected to the runtime or actuators.
class MeasurementQualifier {
  public:
    void update(const RuntimeContext &) {}
};

class FanCoordinator {
  public:
    void update(const RuntimeContext &) {}
};

class EnergyBudget {
  public:
    void update(const RuntimeContext &) {}
};

class RecoveryCoordinator {
  public:
    void update(const RuntimeContext &) {}
};

class PricePlanner {
  public:
    void onSettingsChanged() {}
    void update(const RuntimeContext &) {}
};

class ReserveController {
  public:
    void onSettingsChanged() {}
    void update(const RuntimeContext &) {}
};

class ProtectionCoordinator {
  public:
    void update(const RuntimeContext &) {}
};

class ChargeTargetController {
  public:
    void update(const RuntimeContext &) {}
};

class OutputCeilingArbiter {
  public:
    void update(const RuntimeContext &) {}
};

class CommandTracker {
  public:
    void update(const RuntimeContext &) {}
};

} // namespace control
