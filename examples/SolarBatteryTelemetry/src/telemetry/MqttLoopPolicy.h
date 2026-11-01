#pragma once
#include "TelemetryPublisher.h"
namespace telemetry {
enum class CleanupResult { Retired, DeferredSameEndpoint, UnconfirmedOldEndpoint };
inline CleanupResult disconnectForReconfigure(Publisher& publisher,Transport& wire,bool endpointChanged) {
 const bool retired=publisher.retireCurrent(wire);
 wire.disconnect();
 if(retired)return CleanupResult::Retired;
 // Never falsely discharge an old broker's obligation by sending its tombstone to a new broker.
 if(endpointChanged){publisher.abandonOldEndpoint();return CleanupResult::UnconfirmedOldEndpoint;}
 return CleanupResult::DeferredSameEndpoint;
}
template<class Paused,class NetworkLoop>
void runPreparedMqttLoop(Publisher& publisher,uint32_t now,Transport& wire,bool configured,Paused paused,NetworkLoop networkLoop,unsigned stepBudget=4){
 if(paused()){publisher.setPaused(true);return;}
 publisher.setPaused(false);publisher.tick(now,wire,stepBudget);
 // Starting may arrive from Async OTA while the preceding bounded publication tick runs.
 if(configured&&!paused())networkLoop();
}
}
