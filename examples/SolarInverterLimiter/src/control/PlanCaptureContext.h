#pragma once
#include "PreparedControl.h"
#include <cmath>

namespace control {
// Diagnostic continuity only: unknown charger telemetry cannot clear a previously known fault.
class PlanCaptureFaults {
public:
    void observe(bool chargerKnown, bool chargerFault) { if (chargerKnown) charger_ = chargerFault; }
    bool blocked(bool genuineBmsFault) const { return charger_ || genuineBmsFault; }
private:
    bool charger_ = false;
};
// Exact semantic context, independent of structure padding and live sample freshness.
struct PlanCaptureContext {
    PreparedSettings settings;
    int maximumW = 0, minimumW = 0, offsetW = 0, sourceTimeoutS = 0;
    float priceFactor = 0, currentPrice = 0;
    uint32_t prices = 0, pv = 0; // Receiver-local content identities; public packet revisions remain unchanged.
    bool limiter = false, transport = false, negativeMin = false, negativeZero = false, pvValid = false;
    void observePrice(bool fresh, float measured, const PlanCaptureContext *previous) {
        currentPrice = fresh || !previous ? measured : previous->currentPrice;
    }
    bool hardInvalid(bool knownSourceFault) const { return !settings.valid || knownSourceFault; }
    bool sameHard(const PlanCaptureContext &o) const {
        const auto eq = [](float a, float b) { return a == b || (std::isnan(a) && std::isnan(b)); };
        return settings.valid == o.settings.valid && settings.idleOutputW == o.settings.idleOutputW &&
            eq(settings.reserveOffSocPercent, o.settings.reserveOffSocPercent) &&
            eq(settings.reserveOnSocPercent, o.settings.reserveOnSocPercent) &&
            settings.mosManualMode == o.settings.mosManualMode && settings.inverterEnabled == o.settings.inverterEnabled &&
            eq(settings.priceComfortEurPerKwh, o.settings.priceComfortEurPerKwh) &&
            eq(settings.priceDeadbandEurPerKwh, o.settings.priceDeadbandEurPerKwh) &&
            eq(settings.priceHysteresisEurPerKwh, o.settings.priceHysteresisEurPerKwh) &&
            maximumW == o.maximumW && minimumW == o.minimumW && offsetW == o.offsetW && sourceTimeoutS == o.sourceTimeoutS &&
            eq(priceFactor, o.priceFactor) &&
            limiter == o.limiter && transport == o.transport && negativeMin == o.negativeMin &&
            negativeZero == o.negativeZero && pvValid == o.pvValid;
    }
    bool same(const PlanCaptureContext &o) const {
        const bool equalPrice = currentPrice == o.currentPrice || (std::isnan(currentPrice) && std::isnan(o.currentPrice));
        return sameHard(o) && equalPrice && prices == o.prices && pv == o.pv;
    }
};
} // namespace control
