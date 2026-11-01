#pragma once
#include "PanicSummary.h"
#include "telemetry/TelemetryPublisher.h"

namespace sbt::diagnostics {
// Capture is boot-owned; tick is called only by the existing MQTT wire owner.
// A successful QoS0 write is local acceptance, not a broker acknowledgement.
class PanicSummaryPublisher {
 char payload_[telemetry::payloadLimit+1]{};
 bool captured_=false,ready_=false,sent_=false;
 uint8_t attempts_=0;uint32_t attemptedAt_=0;
 char root_[telemetry::rootLimit+1]{};
 bool cleanupRetry_=false;uint32_t cleanupAt_=0;
public:
 bool capture(const PanicSummary& summary){
  if(captured_)return false;
  captured_=true;ready_=formatPanicSummary(summary,payload_,sizeof(payload_));return ready_;
 }
 // A retained historical report belongs to its broker/root, never a different endpoint.
 bool retireRoot(telemetry::Transport& wire,uint32_t now=0){
  if(!root_[0])return true;
  if(cleanupRetry_&&uint32_t(now-cleanupAt_)<250)return false;
  char topic[telemetry::topicLimit+1];
  if(!wire.connected()||!telemetry::buildTopic(root_,"Diagnostics/Panic",topic,sizeof(topic)))return false;
  if(!wire.publish(topic,"",true)){cleanupRetry_=true;cleanupAt_=now;return false;}
  abandonEndpoint();return true;
 }
 void abandonEndpoint(){root_[0]=0;sent_=false;attempts_=0;cleanupRetry_=false;}
 void tick(uint32_t now,const char* root,telemetry::Transport& wire){
  if(!ready_||!wire.connected()||!root||!telemetry::validRoot(root,std::strlen(root)))return;
  if(root_[0]&&std::strcmp(root_,root)){retireRoot(wire,now);return;}
  if(!root_[0])std::memcpy(root_,root,std::strlen(root)+1);
  if(sent_||attempts_>=12)return;
  if(attempts_&&uint32_t(now-attemptedAt_)<5000)return;
  char topic[telemetry::topicLimit+1];
  if(!telemetry::buildTopic(root,"Diagnostics/Panic",topic,sizeof(topic)))return;
  attemptedAt_=now;++attempts_;sent_=wire.publish(topic,payload_,true);
 }
};
}
