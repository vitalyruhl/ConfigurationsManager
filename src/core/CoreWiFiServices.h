#pragma once

#include <Arduino.h>
#include <Ticker.h>
#include <time.h>

#include "CoreSettings.h"

namespace cm
{

// Header-only helper to keep examples smaller.
//
// Provides a simple "WiFi services" state machine that can be called from
// ConfigManager's global WiFi hooks (onWiFiConnected/onWiFiDisconnected/onWiFiAPMode).
//
// Responsibilities:
// - Initialize OTA once after first connect (if enabled).
// - Run an immediate NTP sync on connect and schedule periodic resync via Ticker.
// - Detach the NTP ticker on disconnect/AP.
//
// Notes:
// - This helper intentionally does NOT log to Serial to keep it reusable.
// - Only one instance should be used in a sketch (ticker callback uses a static pointer).
class CoreWiFiServices
{
public:
    void onConnected(ConfigManagerClass &cfg,
                     const char *otaHostname,
                     const CoreSystemSettings &system,
                     const CoreNtpSettings &ntp)
    {
        if (!servicesActive)
        {
            if (system.allowOTA.get() && !cfg.getOTAManager().isInitialized())
            {
                cfg.setupOTA(otaHostname, system.otaPassword.get().c_str());
            }

            servicesActive = true;
        }

        startNtp(ntp);
    }

    void onDisconnected()
    {
        stopNtp();
        servicesActive = false;
    }

    void onAPMode()
    {
        onDisconnected();
    }

    bool isActive() const
    {
        return servicesActive;
    }

    void stopNtp()
    {
        ntpSyncTicker.detach();
    }

private:
    void startNtp(const CoreNtpSettings &ntp)
    {
        activeInstance = this;
        ntpSettings = &ntp;

        doNtpSync(ntp);

        ntpSyncTicker.detach();

        int intervalSec = ntp.frequencySec.get();
        if (intervalSec < 60)
        {
            intervalSec = 3600;
        }

        ntpSyncTicker.attach(intervalSec, &CoreWiFiServices::ntpTickerThunk);
    }

    static void ntpTickerThunk()
    {
        if (activeInstance == nullptr || activeInstance->ntpSettings == nullptr)
        {
            return;
        }

        activeInstance->doNtpSync(*activeInstance->ntpSettings);
    }

    void doNtpSync(const CoreNtpSettings &ntp)
    {
        configTzTime(ntp.tz.get().c_str(), ntp.server1.get().c_str(), ntp.server2.get().c_str());
    }

private:
    Ticker ntpSyncTicker;
    bool servicesActive = false;

    const CoreNtpSettings *ntpSettings = nullptr;

    inline static CoreWiFiServices *activeInstance = nullptr;
};

} // namespace cm
