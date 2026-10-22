#pragma once
#include <cstdint>
namespace cm {
// One board-wide owner. Call configure/heartbeat/destruction from the application loop.
// OTA progress may call heartbeat only when executing synchronously in that same loop.
class LoopWatchdog {
 uint32_t generation_=0;
public:
 LoopWatchdog();
 ~LoopWatchdog();
 LoopWatchdog(const LoopWatchdog&)=delete;
 LoopWatchdog& operator=(const LoopWatchdog&)=delete;
 bool configure(uint32_t timeoutMs); // 0 disables; otherwise 1000..3600000 ms.
 bool heartbeat(); // First actual beat arms; false reports timer creation/start failure.
 bool isRunning()const;
 static bool consumeRestartMarker();
};
}
