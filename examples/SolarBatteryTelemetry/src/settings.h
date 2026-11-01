#pragma once

// WiFi/UI/OTA, passive Victron Instant Readout and read-only MQTT telemetry.
#define APP_VERSION "1.5.1"
#define VERSION_DATE "2026-10-31"

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#define SMOKE_LOCAL_SECRETS 1
#else
#include "secret/secrets.example.h"
#define SMOKE_LOCAL_SECRETS 0
#endif

// Optional on existing local secret files; empty values leave readout unconfigured.
#ifndef VICTRON_READOUT_MAC
#define VICTRON_READOUT_MAC ""
#endif
#ifndef VICTRON_READOUT_KEY
#define VICTRON_READOUT_KEY ""
#endif

#ifndef BMS_READOUT_MAC
#define BMS_READOUT_MAC ""
#endif
