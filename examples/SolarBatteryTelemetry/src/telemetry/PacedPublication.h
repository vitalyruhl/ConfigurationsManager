#pragma once
#include "TelemetryPublisher.h"
namespace telemetry {
// One ordinary wire attempt per elapsed grant, with no catch-up credit.
// Lifecycle retirement and urgent control results have independent admission.
class PacedPublication {
 uint32_t attemptedAt_=0;bool started_=false;unsigned cursor_=0;
 class OneAttempt final:public Transport {
  Transport& wire_;
 public:
  bool attempted=false;
  explicit OneAttempt(Transport& wire):wire_(wire){}
  bool connected()const override{return wire_.connected();}
  bool publish(const char*t,const char*p,bool retained)override{
   if(attempted)return false;attempted=true;return wire_.publish(t,p,retained);
  }
  void disconnect()override{wire_.disconnect();}void configure(const char*root)override{wire_.configure(root);}
 };
public:
 template<class Visit>void tick(uint32_t now,Transport& wire,Visit visit){
  if(!wire.connected()||(started_&&uint32_t(now-attemptedAt_)<100))return;
  OneAttempt bounded(wire);
  for(unsigned i=0;i<6;++i){
   const auto slot=(cursor_+i)%6;visit(slot,bounded);
   if(bounded.attempted){cursor_=(slot+1)%6;started_=true;attemptedAt_=now;return;}
  }
 }
};
}
