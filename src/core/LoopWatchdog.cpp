#include "LoopWatchdog.h"
#include "detail/LoopWatchdogState.h"
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <rom/ets_sys.h>
namespace {
constexpr uint32_t kMarker=0x434d4c57u; // CMLW
RTC_NOINIT_ATTR uint32_t restartMagic;
RTC_NOINIT_ATTR uint32_t restartComplement;
// Static storage outlives all instances and already-dispatched timer callbacks.
struct Context {
 cm::detail::LoopWatchdogState state;
 std::atomic_flag lock=ATOMIC_FLAG_INIT;
 std::atomic<uint32_t> owner{0};
 std::atomic<bool> running{false};
 uint32_t nextGeneration=0;
 esp_timer_handle_t timer=nullptr;
 bool failed=false,markerChecked=false;
};
Context& context(){static Context value;return value;}
struct Guard {
 Context& value;
 explicit Guard(Context& c):value(c){while(value.lock.test_and_set(std::memory_order_acquire)) {}}
 ~Guard(){value.lock.clear(std::memory_order_release);}
};
bool consumeMarker(){
 const bool marked=esp_reset_reason()==ESP_RST_SW&&restartMagic==kMarker&&restartComplement==~kMarker;
 restartMagic=0;restartComplement=0;return marked;
}
uint32_t nowMs(){return static_cast<uint32_t>(esp_timer_get_time()/1000);}
void observe(void* argument){
 auto& c=context();
 // Never block the shared esp_timer task behind application work.
 if(c.lock.test_and_set(std::memory_order_acquire))return;
 const auto generation=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(argument));
 if(generation&&c.owner.load(std::memory_order_relaxed)==generation&&c.running.load(std::memory_order_relaxed)&&c.state.expired(nowMs())){
  c.running.store(false,std::memory_order_relaxed);
  restartMagic=kMarker;restartComplement=~kMarker;
  ets_printf("[CM-LOOP-WDT] heartbeat expired; restarting\n");
  esp_restart();
 }
 c.lock.clear(std::memory_order_release);
}
}
cm::LoopWatchdog::LoopWatchdog(){
 auto& c=context();Guard guard(c);
 if(c.owner.load(std::memory_order_relaxed))return;
 if(++c.nextGeneration==0)++c.nextGeneration;
 generation_=c.nextGeneration;c.state.configure(0);c.state.configure(30000);
 c.failed=false;c.owner.store(generation_,std::memory_order_release);
}
cm::LoopWatchdog::~LoopWatchdog(){
 auto& c=context();Guard guard(c);
 if(!generation_||c.owner.load(std::memory_order_relaxed)!=generation_)return;
 c.state.configure(0);c.running.store(false,std::memory_order_relaxed);
 if(c.timer){esp_timer_stop(c.timer);esp_timer_delete(c.timer);c.timer=nullptr;}
 c.owner.store(0,std::memory_order_release);
}
bool cm::LoopWatchdog::configure(uint32_t timeoutMs){
 if(!detail::LoopWatchdogState::validTimeout(timeoutMs))return false;
 auto& c=context();Guard guard(c);
 if(!generation_||c.owner.load(std::memory_order_relaxed)!=generation_)return false;
 c.state.configure(timeoutMs);c.failed=false;
 if(!timeoutMs&&c.running.exchange(false,std::memory_order_relaxed))esp_timer_stop(c.timer);
 return true;
}
bool cm::LoopWatchdog::heartbeat(){
 auto& c=context();Guard guard(c);
 if(!generation_||c.owner.load(std::memory_order_relaxed)!=generation_)return false;
 if(!c.markerChecked){c.markerChecked=true;if(consumeMarker())ets_printf("[CM-LOOP-WDT] previous restart: expired loop heartbeat\n");}
 c.state.heartbeat(nowMs());
 if(!c.state.timeoutMs())return true;
 if(c.running.load(std::memory_order_relaxed))return true;
 if(c.failed)return false;
 if(!c.timer){
  esp_timer_create_args_t args{};args.callback=observe;args.arg=reinterpret_cast<void*>(static_cast<uintptr_t>(generation_));
  args.dispatch_method=ESP_TIMER_TASK;args.name="cm_loop_wdt";args.skip_unhandled_events=true;
  if(esp_timer_create(&args,&c.timer)!=ESP_OK){c.failed=true;ets_printf("[CM-LOOP-WDT] timer creation failed\n");return false;}
 }
 if(esp_timer_start_periodic(c.timer,1000000)!=ESP_OK){c.failed=true;ets_printf("[CM-LOOP-WDT] timer start failed\n");return false;}
 c.running.store(true,std::memory_order_release);return true;
}
bool cm::LoopWatchdog::isRunning()const{auto& c=context();return generation_&&c.owner.load(std::memory_order_acquire)==generation_&&c.running.load(std::memory_order_acquire);}
bool cm::LoopWatchdog::consumeRestartMarker(){auto& c=context();Guard guard(c);return consumeMarker();}
#else
cm::LoopWatchdog::LoopWatchdog()=default;
cm::LoopWatchdog::~LoopWatchdog()=default;
bool cm::LoopWatchdog::configure(uint32_t ms){return ms==0;}
bool cm::LoopWatchdog::heartbeat(){return false;}
bool cm::LoopWatchdog::isRunning()const{return false;}
bool cm::LoopWatchdog::consumeRestartMarker(){return false;}
#endif
