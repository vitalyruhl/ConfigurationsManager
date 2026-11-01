#include "ThreadSemaphoreDiagnostics.h"

#if SBT_ENABLE_HEAP_DIAGNOSTICS && defined(ESP32)
#include <atomic>
#include <arch/sys_arch.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <rom/ets_sys.h>

namespace {
struct TaskIdentity {
  TaskHandle_t handle;
  char name[16];
  bool isr;
};
struct Failure {
  unsigned sequence;
  size_t size;
  uint32_t caps;
  void* caller;
  TaskIdentity task;
  char api[24];
};
// Fixed storage; concurrent/recursive entries are dropped rather than waiting.
Failure history[4]{};
unsigned failureCount=0;
std::atomic<unsigned> dropped{0};
std::atomic_flag historyBusy=ATOMIC_FLAG_INIT;

template<size_t N> void copyText(char (&target)[N],const char* source) {
  size_t i=0;
  if(source) for(;i+1<N && source[i];++i) target[i]=source[i];
  target[i]='\0';
}
TaskIdentity currentTask() {
  TaskIdentity task{};
  task.isr=xPortInIsrContext();
  // Task-name access is qualified for task context only, not ISR use.
  if(!task.isr) {
    task.handle=xTaskGetCurrentTaskHandle();
    copyText(task.name,task.handle?pcTaskGetName(task.handle):"unknown");
  } else copyText(task.name,"isr");
  return task;
}
void reportNull(void* caller) {
  const auto task=currentTask();
  const bool locked=!historyBusy.test_and_set(std::memory_order_acquire);
  ets_printf("[SEM-NULL] task=%p name=%s caller=%p isr=%u history=%s dropped=%u\n",
             task.handle,task.name,caller,static_cast<unsigned>(task.isr),
             locked?"ready":"busy",dropped.load(std::memory_order_relaxed));
  if(!locked) return;
  for(const auto& failure:history) {
    if(!failure.sequence) continue;
    ets_printf("[SEM-HEAP] n=%u size=%u caps=0x%x api=%s task=%p name=%s caller=%p isr=%u sameTask=%u\n",
               failure.sequence,static_cast<unsigned>(failure.size),static_cast<unsigned>(failure.caps),
               failure.api,failure.task.handle,failure.task.name,failure.caller,
               static_cast<unsigned>(failure.task.isr),
               static_cast<unsigned>(!task.isr && !failure.task.isr && task.handle && task.handle==failure.task.handle));
  }
  historyBusy.clear(std::memory_order_release);
}
} // namespace

void sbt::diagnostics::recordAllocationFailure(size_t size,uint32_t caps,const char* api,void* caller) {
  if(historyBusy.test_and_set(std::memory_order_acquire)) {
    dropped.fetch_add(1,std::memory_order_relaxed);
    return;
  }
  auto& failure=history[failureCount%4];
  failure.sequence=++failureCount;
  failure.size=size;
  failure.caps=caps;
  failure.caller=caller;
  failure.task=currentTask();
  copyText(failure.api,api?api:"unknown");
  historyBusy.clear(std::memory_order_release);
}

extern "C" sys_sem_t* __real_sys_thread_sem_get();
extern "C" __attribute__((noinline)) sys_sem_t* __wrap_sys_thread_sem_get() {
  // Do not inspect/prewarm TLS: the real getter may itself allocate the semaphore.
  sys_sem_t* result=__real_sys_thread_sem_get();
  if(!result) reportNull(__builtin_return_address(0));
  return result;
}
#endif
