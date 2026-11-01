#pragma once
#include "JbdProtocol.h"
namespace bms {
enum class MosTarget { Charge, Discharge, Both, Gate };
// For Both, state is the charge permission and dischargeState is the discharge permission.
struct MosRequest { MosTarget target=MosTarget::Charge;bool state=false,dischargeState=false,baselineKnown=false,emergency=false;uint8_t baselineMask=0;char peer[18]{},id[33]{};uint32_t deadline=0; };
// charge/discharge are pre-write actual FET diagnostics, never software permission feedback.
struct MosResult {char id[33]{};const char* phase="idle";const char* reason="none";bool charge=false,discharge=false;uint32_t sequence=0;};
// Caller holds its mutex for offer/take/settle; taking never releases the in-flight reservation.
class MosMailbox {
 MosRequest request_{};bool queued_=false,reserved_=false,cooling_=false;uint32_t settledAt_=0;const char* reason_="none";
public:
 const char* offer(const MosRequest& request,uint32_t now,bool enabled,bool ready,bool cancelled){
  if(!enabled)return reason_="rejected_disabled";
  if(cancelled)return reason_="rejected_cancelled";
  if(reserved_&&!(queued_&&request_.emergency&&!request.emergency))return reason_="rejected_busy";
  if(cooling_&&uint32_t(now-settledAt_)<500)return reason_="rejected_debounce";
  if(!ready)return reason_="rejected_unavailable";
  if(int32_t(now-request.deadline)>=0)return reason_="rejected_expired";
  request_=request;queued_=reserved_=true;return reason_="accepted";
 }
 bool take(MosRequest& request){if(!queued_)return false;request=request_;queued_=false;return true;}
 void settle(bool active,uint32_t now=0){if(reserved_&&!active&&!queued_){reserved_=false;cooling_=true;settledAt_=now;}}
 void cancel(){if(reserved_)reason_="cancelled";queued_=reserved_=false;}
 bool pending()const{return reserved_;}
 const char* reason()const{return reason_;}
};
class BmsCommandPolicy {
 enum class Phase { Idle, PreRead, Write };
 Phase phase_=Phase::Idle;MosRequest request_{};MosResult result_{};
 uint8_t desired_=0,permissionMask_=0;bool permissionsKnown_=false,writeSent_=false;
 void finish(const char* phase,const char* reason){phase_=Phase::Idle;writeSent_=false;result_.phase=phase;result_.reason=reason;++result_.sequence;}
public:
 bool pending()const{return phase_!=Phase::Idle;}
 const MosResult& result()const{return result_;}
 bool permissionsKnown()const{return permissionsKnown_;}
 // Low bits are acknowledged enable requests, not actual conducting FET states.
 uint8_t permissionMask()const{return permissionMask_;}
 void invalidatePermissions(){permissionsKnown_=false;}
 // The caller marks only a queued MOS write; an unacknowledged write may already have taken effect.
 void markWriteSent(){if(phase_==Phase::Write)writeSent_=true;}
 bool submit(const MosRequest& request,uint32_t now,bool enabled,bool ready){
  if(pending())return false;
  request_=request;std::memcpy(result_.id,request.id,sizeof(result_.id));result_.id[32]=0;
  if(!enabled){finish("rejected","disabled");return false;}
  if(!ready){finish("rejected","source_unavailable");return false;}
  if(int32_t(now-request.deadline)>=0){finish("rejected","expired");return false;}
  if(request.target==MosTarget::Gate){finish("rejected","not_mos");return false;}
  if(request.emergency&&(request.target!=MosTarget::Both||request.state||request.dischargeState)){finish("rejected","emergency_off_only");return false;}
  writeSent_=false;phase_=Phase::PreRead;result_.phase="pending";result_.reason="pre_read";++result_.sequence;return true;
 }
 void cancel(const char* reason){if(pending()){if(writeSent_)invalidatePermissions();finish("failed",reason);}}
 void settingResult(const MosRequest& request,const char* phase,const char* reason){
  std::memcpy(result_.id,request.id,sizeof(result_.id));result_.id[32]=0;finish(phase,reason);
 }
 void tick(uint32_t now){if(pending()&&int32_t(now-request_.deadline)>=0)cancel("expired");}
 uint32_t queryDelayMs()const{return 50u;}
 uint8_t expected()const{return phase_==Phase::Write?0xe1:phase_==Phase::Idle?0:3;}
 bool command(uint8_t (&out)[9],size_t& size)const{
  if(!pending())return false;
  if(phase_!=Phase::Write){uint8_t read[7];if(!readCommand(expected(),read))return false;std::memcpy(out,read,7);size=7;return true;}
  const uint8_t disable=desired_^3;const uint16_t checksum=uint16_t(0-(0xe3+disable));
  const uint8_t bytes[]={0xdd,0x5a,0xe1,2,0,disable,uint8_t(checksum>>8),uint8_t(checksum),0x77};std::memcpy(out,bytes,9);size=9;return true;
 }
 void response(const Frame& frame,bool valid,const Snapshot&,uint32_t now){
  tick(now);if(!pending()||frame.reg!=expected())return;
  if(frame.status){cancel("device_rejected");return;}
  if(!valid){cancel("invalid_response");return;}
  if(phase_==Phase::PreRead){
   if(frame.size<23){cancel("invalid_response");return;}
   if(request_.emergency&&((((unsigned(frame.payload[16])<<8)|frame.payload[17])&0x0fff)==0)){cancel("emergency_cleared");return;}
   result_.charge=(frame.payload[20]&1)!=0;result_.discharge=(frame.payload[20]&2)!=0;
   if(request_.target==MosTarget::Both)desired_=(request_.state?1:0)|(request_.dischargeState?2:0);
   else {
    const uint8_t mask=request_.target==MosTarget::Charge?1:2;
    const uint8_t baseline=frame.payload[20]&3; // The fresh actual pair is authoritative; never restore an old software request.
    desired_=static_cast<uint8_t>((baseline&~mask)|(request_.state?mask:0));
   }
   phase_=Phase::Write;result_.reason="write";++result_.sequence;
  }else if(phase_==Phase::Write){
   if(frame.size||!writeSent_){cancel("invalid_write_ack");return;}
   permissionMask_=desired_;permissionsKnown_=true;finish("acknowledged","ok");
  }
 }
};
}
