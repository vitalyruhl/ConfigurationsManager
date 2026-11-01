#pragma once
#include "telemetry/TelemetryPublisher.h"
namespace sbt::diagnostics {
// Loop-owned, fixed storage; heap/stack APIs are sampled outside the serializer.
class RuntimePublisher {
 uint32_t capturedAt_=0,retryAt_=0,refreshAt_=0,refreshMs_=60000;
 uint32_t stack_=0,free_=0,largest_=0,minimum_=0;
 bool have_=false,dirty_=false,retry_=false,refreshStarted_=false;
 char root_[telemetry::rootLimit+1]{};
public:
 bool due(uint32_t now)const{return !have_||uint32_t(now-capturedAt_)>=30000;}
 void observe(uint32_t now,uint32_t stack,uint32_t free,uint32_t largest,uint32_t minimum){capturedAt_=now;stack_=stack;free_=free;largest_=largest;minimum_=minimum;have_=dirty_=true;}
 void setRefreshInterval(uint32_t ms){refreshMs_=ms<5000?5000:ms>3600000?3600000:ms;}
 void onConnected(){dirty_=have_;retry_=false;refreshStarted_=false;}
 void tick(uint32_t now,const char* root,telemetry::Transport& wire){
  if(!wire.connected()||!root||!telemetry::validRoot(root,std::strlen(root)))return;
  if(std::strcmp(root_,root)){std::memcpy(root_,root,std::strlen(root)+1);onConnected();}
  if(!refreshStarted_){refreshAt_=now;refreshStarted_=true;}
  else if(uint32_t(now-refreshAt_)>=refreshMs_){dirty_=have_;refreshAt_=now;}
  if(!dirty_||(retry_&&uint32_t(now-retryAt_)<250))return;
  char topic[telemetry::topicLimit+1],payload[256];
  const int n=std::snprintf(payload,sizeof(payload),"{\"uptime_ms\":%lu,\"loop_stack_min_free_bytes\":%lu,\"heap_free_bytes\":%lu,\"heap_largest_free_block_bytes\":%lu,\"heap_min_free_bytes\":%lu}",static_cast<unsigned long>(capturedAt_),static_cast<unsigned long>(stack_),static_cast<unsigned long>(free_),static_cast<unsigned long>(largest_),static_cast<unsigned long>(minimum_));
  if(n<0||static_cast<size_t>(n)>=sizeof(payload)||!telemetry::buildTopic(root_,"Diagnostics/Runtime",topic,sizeof(topic)))return;
  if(wire.publish(topic,payload,false)){dirty_=false;retry_=false;}else{retry_=true;retryAt_=now;}
 }
};
}
