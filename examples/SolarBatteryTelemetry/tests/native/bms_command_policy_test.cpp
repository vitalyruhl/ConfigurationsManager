#include "bms/BmsCommandPolicy.h"
#include <cassert>
#include <cstdio>
using namespace bms;

// [MOCKED!] Decoded protocol frames exercise the real portable command policy.
static MosRequest request(MosTarget target,bool state,uint32_t deadline=30000) {
 MosRequest value;value.target=target;value.state=state;value.deadline=deadline;
 std::memcpy(value.id,"policy-test",sizeof("policy-test"));return value;
}
static Frame basic(uint8_t actual=3) {
 Frame value;value.reg=3;value.size=23;value.payload[20]=actual;return value;
}
static void prepare(BmsCommandPolicy& policy,const MosRequest& value,uint32_t now=0,uint8_t actual=3) {
 assert(policy.submit(value,now,true,true));policy.response(basic(actual),true,{},now+1);
 assert(policy.pending()&&policy.expected()==0xe1);
}
static void acknowledge(BmsCommandPolicy& policy,uint32_t now=2) {
 policy.markWriteSent();Frame ack;ack.reg=0xe1;policy.response(ack,true,{},now);
 assert(!policy.pending()&&!std::strcmp(policy.result().phase,"acknowledged"));
 assert(!std::strcmp(policy.result().reason,"ok"));
}
static void initialize(BmsCommandPolicy& policy,bool charge=true,bool discharge=false) {
 auto value=request(MosTarget::Both,charge);value.dischargeState=discharge;
 prepare(policy,value);acknowledge(policy);
}
int main() {
 MosMailbox mailbox;auto input=request(MosTarget::Charge,false);MosRequest dequeued;
 assert(!std::strcmp(mailbox.offer(input,0,false,true,false),"rejected_disabled"));
 assert(!mailbox.take(dequeued));
 assert(!std::strcmp(mailbox.offer(input,0,true,false,false),"rejected_unavailable"));
 assert(!mailbox.take(dequeued));
 assert(!std::strcmp(mailbox.offer(input,0,true,true,true),"rejected_cancelled"));
 assert(!std::strcmp(mailbox.offer(input,30000,true,true,false),"rejected_expired"));
 assert(!std::strcmp(mailbox.offer(input,0,true,true,false),"accepted"));
 assert(mailbox.take(dequeued));
 assert(!std::strcmp(mailbox.offer(input,1,true,true,false),"rejected_busy"));
 mailbox.settle(true);assert(mailbox.pending());mailbox.settle(false);assert(!mailbox.pending());
 assert(!std::strcmp(mailbox.offer(input,502,true,true,false),"accepted"));
 mailbox.cancel();assert(!mailbox.take(dequeued)&&!mailbox.pending());

 BmsCommandPolicy unknown;uint8_t wire[9]{};size_t size=0;
 for(auto target:{MosTarget::Charge,MosTarget::Discharge})for(bool state:{false,true}) {
  assert(unknown.submit(request(target,state),0,true,true));
  unknown.cancel("cancelled");assert(!unknown.pending());
  assert(!unknown.command(wire,size));
 }
 // Explicit requests initialize every possible pair; wire bytes are hand-checked.
 struct PairCase {bool charge,discharge;uint8_t allowed,disabled,checksum;};
 for(const auto& test:{PairCase{false,false,0,3,0x1a},PairCase{true,false,1,2,0x1b},
                        PairCase{false,true,2,1,0x1c},PairCase{true,true,3,0,0x1d}}) {
  BmsCommandPolicy policy;auto value=request(MosTarget::Both,test.charge);value.dischargeState=test.discharge;
  assert(!policy.submit(value,0,false,true));assert(!policy.pending());
  assert(!policy.submit(value,0,true,false));assert(!policy.pending());
  assert(!policy.submit(value,30000,true,true));assert(!policy.pending());
  assert(policy.submit(value,0,true,true));assert(!policy.submit(value,1,true,true));
  assert(policy.command(wire,size)&&size==7&&wire[2]==3);
  policy.response(basic(),true,{},1);assert(!policy.permissionsKnown());
  assert(policy.command(wire,size)&&size==9);
  const uint8_t expected[]={0xdd,0x5a,0xe1,2,0,test.disabled,0xff,test.checksum,0x77};
  assert(!std::memcmp(wire,expected,sizeof(expected)));
  acknowledge(policy);assert(policy.permissionsKnown()&&policy.permissionMask()==test.allowed);
  assert(!policy.command(wire,size));
 }

 // The fresh actual opposite direction wins over any historical ACK pair.
 struct SingleCase {bool initialCharge,initialDischarge;MosTarget target;bool state;uint8_t actual,allowed,disabled;};
 for(const auto& test:{
  SingleCase{true,false,MosTarget::Charge,false,3,2,1},
  SingleCase{false,true,MosTarget::Discharge,false,3,1,2},
  SingleCase{false,false,MosTarget::Charge,true,3,3,0},
  SingleCase{false,false,MosTarget::Discharge,true,3,3,0},
  SingleCase{true,true,MosTarget::Charge,true,0,1,2},
  SingleCase{true,true,MosTarget::Discharge,true,0,2,1}}) {
  BmsCommandPolicy policy;initialize(policy,test.initialCharge,test.initialDischarge);
  prepare(policy,request(test.target,test.state),10,test.actual);
  assert(policy.command(wire,size)&&wire[5]==test.disabled);
  acknowledge(policy,12);assert(policy.permissionsKnown()&&policy.permissionMask()==test.allowed);
 }

 // Invalid or incomplete pre-read never emits a write or changes an acknowledged baseline.
 for(bool accepted:{false,true}) {
  BmsCommandPolicy policy;initialize(policy);
  assert(policy.submit(request(MosTarget::Charge,false),10,true,true));
  auto invalid=basic();if(accepted)invalid.size=22;
  policy.response(invalid,accepted,{},11);
  assert(!policy.pending()&&policy.permissionsKnown()&&policy.permissionMask()==1);
  assert(!std::strcmp(policy.result().reason,"invalid_response"));
 }
 // A contradictory ACK cannot initialize permissions unless a write was actually queued.
 BmsCommandPolicy notSent;auto both=request(MosTarget::Both,true);both.dischargeState=false;
 prepare(notSent,both);Frame ack;ack.reg=0xe1;notSent.response(ack,true,{},2);
 assert(!notSent.permissionsKnown()&&!std::strcmp(notSent.result().reason,"invalid_write_ack"));

 // Uncertain queued writes invalidate an old baseline; pre-write rejection/cancellation does not.
 for(const char* reason:{"disabled","ota","disconnected","write_admission_failed"}) {
  BmsCommandPolicy before;initialize(before);assert(before.submit(request(MosTarget::Charge,false),10,true,true));
  before.cancel(reason);assert(before.permissionsKnown()&&before.permissionMask()==1);
  BmsCommandPolicy prepared;initialize(prepared);prepare(prepared,request(MosTarget::Charge,false),10);
  prepared.cancel(reason);assert(prepared.permissionsKnown()&&prepared.permissionMask()==1);
  BmsCommandPolicy sent;initialize(sent);prepare(sent,request(MosTarget::Charge,false),10);sent.markWriteSent();
  sent.cancel(reason);assert(!sent.permissionsKnown()&&!sent.pending());
 }
 for(unsigned invalid=0;invalid<3;++invalid) {
  BmsCommandPolicy policy;initialize(policy);prepare(policy,request(MosTarget::Charge,false),10);policy.markWriteSent();
  Frame bad;bad.reg=0xe1;if(invalid==0)bad.status=0x80;if(invalid==1)bad.size=1;
  policy.response(bad,invalid!=2,{},12);
  assert(!policy.permissionsKnown()&&!policy.pending());
  assert(!std::strcmp(policy.result().reason,invalid==0?"device_rejected":invalid==1?"invalid_write_ack":"invalid_response"));
 }
 BmsCommandPolicy invalidated;initialize(invalidated);invalidated.invalidatePermissions();
 assert(!invalidated.permissionsKnown()&&invalidated.submit(request(MosTarget::Charge,true),10,true,true));invalidated.cancel("cancelled");
 // Invalidating historical ACK state does not prevent a fresh actual-state request.
 initialize(invalidated);assert(invalidated.submit(request(MosTarget::Charge,false),10,true,true));
 invalidated.invalidatePermissions();invalidated.response(basic(),true,{},11);
 assert(invalidated.pending()&&invalidated.expected()==0xe1);invalidated.cancel("cancelled");
 BmsCommandPolicy ignored;ignored.markWriteSent();assert(!ignored.permissionsKnown());

 // Legacy HA baselines cannot override the fresh actual pre-read.
 BmsCommandPolicy rebooted;auto manual=request(MosTarget::Charge,false);
 manual.baselineKnown=true;manual.baselineMask=1;
 assert(!rebooted.pending()&&!rebooted.permissionsKnown()&&!rebooted.command(wire,size));
 prepare(rebooted,manual,10,3);assert(rebooted.command(wire,size)&&wire[5]==1);
 assert(!rebooted.permissionsKnown());acknowledge(rebooted,12);assert(rebooted.permissionMask()==2);
 // A second click may carry a stale cached pair; preserve the fresh actual opposite bit.
 manual.target=MosTarget::Discharge;manual.state=true;manual.baselineMask=1;
 prepare(rebooted,manual,20,3);assert(rebooted.command(wire,size)&&wire[5]==0);
 acknowledge(rebooted,22);assert(rebooted.permissionMask()==3);
 BmsCommandPolicy uncertainManual;prepare(uncertainManual,manual,10);uncertainManual.markWriteSent();uncertainManual.cancel("expired");
 assert(!uncertainManual.permissionsKnown()&&!uncertainManual.pending()&&!uncertainManual.command(wire,size));

 // Error ACK parsing remains checksum-checked and does not turn into a permission ACK.
 Parser parser;Frame decoded;const uint8_t error[]={0xdd,0xe1,0x80,0,0xff,0x80,0x77};bool parsed=false;
 for(auto byte:error)parsed=parser.push(byte,decoded)||parsed;assert(parsed&&decoded.status==0x80);
 std::puts("MOS actual pre-read, fresh opposite preservation and uncertain writes passed");
}
