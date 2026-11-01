#pragma once
#include "BmsCommandPolicy.h"
#include "BmsGateMailbox.h"
#include "smoke/IsolatedTestProfile.h"
#include "runtime/AppRuntimeMetadata.h"
#include <atomic>
#include "victron/VictronReadout.h"
namespace bms {
// Reject malformed UI input before persistence; startup also bounds historical stored values.
class DataTimeoutSetting final : public Config<int> {
public:
 DataTimeoutSetting():Config<int>(ConfigOptions<int>{.key="BmsDataTimeout",.name="BMS data timeout (s, 1-9999)",.category="BMS",.defaultValue=60,.sortOrder=3}){}
 bool fromJSON(const JsonVariant& value) override {
  if(!value.is<int>())return false;
  const int seconds=value.as<int>();if(seconds<1||seconds>9999)return false;
  set(seconds);return true;
 }
};
class ControlGateSetting final : public Config<bool> {
 std::recursive_mutex& transaction_;
public:
 explicit ControlGateSetting(std::recursive_mutex& transaction):Config<bool>(ConfigOptions<bool>{.key="BmsControl",.name="BMS control enabled",.category="BMS",.defaultValue=false,.sortOrder=2}),transaction_(transaction){}
 using Config<bool>::save;
 bool fromJSON(const JsonVariant& value) override {std::lock_guard<std::recursive_mutex> lock(transaction_);return Config<bool>::fromJSON(value);}
 void save(Preferences& prefs) override {std::lock_guard<std::recursive_mutex> lock(transaction_);Config<bool>::save(prefs);}
};
// All protocol and radio operations belong to the application loop. UI callbacks only copy settings.
class Readout {
 DataTimeoutSetting dataTimeoutSec_;
 std::atomic<uint32_t> dataTimeoutMs_{60000};
 static uint32_t timeoutMilliseconds(int seconds){return static_cast<uint32_t>(std::clamp(seconds,1,9999))*1000u;}
 std::recursive_mutex gateTransaction_;
 ControlGateSetting controlEnabled_{gateTransaction_};
 std::atomic<bool> enabled_{false},cancelRequested_{false};
 BmsCommandPolicy commands_;MosMailbox requests_;GateMailbox gateRequests_;
 std::mutex gateIoMutex_;bool controlQuery_=false;
 struct ControlView {bool enabled=false,ready=false,pending=false,permissionsKnown=false;uint8_t permissionMask=0;char peer[18]{};const char* readback="unknown";MosResult result{};const char* admission="none";};
 ControlView controlView_{};uint32_t uiRequest_=0;
 victron::ValidatedText mac_;
 victron::ConfigMailbox mailbox_;
 cm::BluetoothManager* bluetooth_=nullptr;
 victron::Configuration active_{};uint32_t seen_=0,epoch_=0,query_=0,sentAt_=0,dueAt_=0;
 bool attached_=false,connected_=false,connectRequested_=false,waiting_=false,readoutHealthy_=false;
 uint8_t next_=3,expected_=0;
 Parser parser_;Frame frame_;Cycle cycle_;
 mutable std::mutex mutex_;Snapshot view_{};const char* reason_="configuration_missing",*viewReason_="configuration_missing";
 // Same-peer transport loss preserves last ACK intent; cancellation still clears uncertain writes.
 void reset(const char* reason,bool clearSample=false){commands_.cancel(reason);if(clearSample)commands_.invalidatePermissions();{std::lock_guard<std::mutex> lock(mutex_);requests_.cancel();}parser_.reset();
  if(clearSample)cycle_.reset();else cycle_.interrupt();
  readoutHealthy_=false;connected_=false;waiting_=false;query_=0;next_=3;reason_=reason;
 }
 void publishView(uint32_t now){
  const bool paused=bluetooth_->isPaused();
  if(paused){cycle_.reset();readoutHealthy_=false;}
  auto next=cycle_.snapshot(now,dataTimeoutMs_.load());
  if(!active_.macValid||paused)next={};
  // Telemetry tolerance must never make a disconnected or stale command source ready.
  const bool commandFresh=connected_&&readoutHealthy_&&next.available&&uint32_t(now-next.receivedAt)<30000;
  std::lock_guard<std::mutex> lock(mutex_);view_=next;viewReason_=next.available?(commandFresh?"ok":"cached"):paused?"paused":reason_;
  requests_.settle(commands_.pending(),now);
  controlView_.enabled=enabled_.load();controlView_.pending=requests_.pending()||commands_.pending()||gateRequests_.pending();controlView_.admission=requests_.reason();
  controlView_.ready=enabled_.load()&&commandFresh&&!paused&&!controlView_.pending;
  controlView_.result=commands_.result();
  std::memcpy(controlView_.peer,active_.mac,sizeof(controlView_.peer));
  controlView_.permissionsKnown=next.available&&next.values[8].valid&&next.values[9].valid;
  controlView_.permissionMask=(next.values[8].number!=0?1:0)|(next.values[9].number!=0?2:0);
  controlView_.readback=!controlView_.permissionsKnown?"unavailable":commandFresh?"actual":"cached";
 }
public:
 explicit Readout(const char* defaultMac):mac_(ConfigOptions<String>{.key="BmsReadMac",.name="BMS Bluetooth MAC",.category="BMS",.defaultValue=String(defaultMac),.sortOrder=1},false){}
 bool attach(ConfigManagerClass& manager,cm::BluetoothManager& bluetooth){
  if(attached_)return true;
  if(mac_.hasError()||manager.addSetting(&mac_)!=&mac_||controlEnabled_.hasError()||manager.addSetting(&controlEnabled_)!=&controlEnabled_||dataTimeoutSec_.hasError()||manager.addSetting(&dataTimeoutSec_)!=&dataTimeoutSec_)return false;
  manager.setCategoryLayoutOverride("BMS","Bluetooth","BMS","Readout",42);
  manager.addSettingsCard("Bluetooth","BMS",42);manager.addSettingsGroup("Bluetooth","BMS","Readout",42);
  manager.addToSettingsGroup(mac_.getKey(),"Bluetooth","BMS","Readout",1);
  manager.addToSettingsGroup(controlEnabled_.getKey(),"Bluetooth","BMS","Readout",2);
  manager.addToSettingsGroup(dataTimeoutSec_.getKey(),"Bluetooth","BMS","Readout",3);
  dataTimeoutSec_.setCallback([this](int value){dataTimeoutMs_.store(timeoutMilliseconds(value));});
  controlEnabled_.setCallback([this](bool value){std::lock_guard<std::mutex> io(gateIoMutex_);enabled_.store(sbt::testprofile::controlAllowed(value));if(!enabled_.load()){cancelRequested_.store(true);if(bluetooth_)bluetooth_->cancelPendingCommands();}});
  mac_.setCallback([this](const String& text){mailbox_.setMac(text.c_str(),text.length());});
  bluetooth_=&bluetooth;
  manager.getRuntime().addRuntimeProvider("bms",[this](JsonObject& data){
   Snapshot copy;const char* reason;{std::lock_guard<std::mutex> lock(mutex_);copy=view_;reason=viewReason_;}
   data["reason"]=reason;data["data_timeout_s"]=dataTimeoutMs_.load()/1000u;
   data["sample_sequence"]=copy.sequence;
   if(copy.available)data["sample_age_ms"]=uint32_t(millis()-copy.receivedAt);else data["sample_age_ms"]=nullptr;
   auto c=control();data["control_enabled"]=c.enabled;data["control_ready"]=c.ready;data["control_pending"]=c.pending;
   data["control_admission"]=c.admission;data["control_result"]=c.result.phase;data["control_reason"]=c.result.reason;data["control_request_id"]=c.result.id;
   data["peer"]=c.peer;data["permissions_known"]=c.permissionsKnown;data["mos_readback"]=c.readback;
   if(c.permissionsKnown){data["requested_charge_enabled"]=(c.permissionMask&1)!=0;data["requested_discharge_enabled"]=(c.permissionMask&2)!=0;}
   else {data["requested_charge_enabled"]=nullptr;data["requested_discharge_enabled"]=nullptr;}
   for(size_t i=0;i<telemetry::channelCount;++i){const auto& c=telemetry::channels[i];if(c.source!=telemetry::Source::Bms)continue;
    const char* key=std::strchr(c.suffix,'/')+1;auto value=copy.values[i];
    if(!value.valid)data[key]=nullptr;else if(c.type==telemetry::FieldType::Boolean)data[key]=value.number!=0;else if(c.type==telemetry::FieldType::Protection){char text[512];if(telemetry::formatProtection(static_cast<uint16_t>(value.number),text,sizeof(text)))data[key]=text;else data[key]=nullptr;}else data[key]=value.number;
    data["types"][key]=telemetry::typeName(c.type);data["support"][key]="jbd_readout";
   }
  },7);
  sbt::metadata::registerBmsCheckboxes(manager.getRuntime(),[this](unsigned i,bool state){MosRequest request;request.target=i==0?MosTarget::Charge:MosTarget::Discharge;request.state=state;request.deadline=millis()+30000;
   {std::lock_guard<std::mutex> lock(mutex_);std::snprintf(request.id,sizeof(request.id),"ui-%lu",static_cast<unsigned long>(++uiRequest_));}
   queueRequest(request);
  });
  attached_=true;return true;
 }
 void afterLoad(){dataTimeoutMs_.store(timeoutMilliseconds(dataTimeoutSec_.get()));enabled_.store(sbt::testprofile::controlAllowed(controlEnabled_.get()));mailbox_.setMac(mac_.get().c_str(),mac_.get().length());loop(millis());}
 void onEvent(const cm::BleEvent& event){
  if(!active_.macValid||std::strcmp(event.peer.text,active_.mac)!=0)return;
  const uint32_t now=millis();
  if(event.type==cm::BleEventType::Connected){reset("waiting_for_response");connected_=true;epoch_=event.generation;dueAt_=now;}
  else if(event.type==cm::BleEventType::Disconnected||event.type==cm::BleEventType::Error){
   reset("disconnected_or_timeout");
   // A failed GATT write may leave the transport connected without a pending query.
   // Reconfigure to cancel that link; otherwise neither layer would retry.
   if(event.type==cm::BleEventType::Error&&event.errorCode==-1003){
    cm::BleAddress peer{};std::memcpy(peer.text,active_.mac,sizeof(peer.text));
    bluetooth_->configureGattPeer(peer,0xff00,0xff01,0xff02);
    epoch_=bluetooth_->generation();connectRequested_=false;dueAt_=now+1000;
   }
  }
  else if(event.type==cm::BleEventType::Notification&&connected_&&waiting_&&event.generation==epoch_&&event.queryId&&uint32_t(now-sentAt_)<2000){
   if(query_&&query_!=event.queryId)return;query_=event.queryId;
   for(size_t i=0;i<event.size;++i)if(parser_.push(event.data[i],frame_)){
    if(frame_.reg!=expected_)continue;
    const bool accepted=frame_.reg==0xe1?frame_.status==0:cycle_.accept(frame_,now);
    if(!accepted)readoutHealthy_=false;else if(frame_.reg==4)readoutHealthy_=true;
    if(controlQuery_)commands_.response(frame_,accepted,cycle_.snapshot(now),now);
    // Complete during the notification callback, retaining CM's exact query identity.
    if(!bluetooth_->completeQuery()){reset("completion_failed");break;}
    waiting_=false;query_=0;parser_.reset();
    if(controlQuery_){next_=3;dueAt_=now+commands_.queryDelayMs();controlQuery_=false;}
    else if(accepted&&expected_==3){next_=4;dueAt_=now+50;}
    else {next_=3;dueAt_=now+5000;reason_=accepted?"stale":"invalid_response";}
    break;
   }
  }
  publishView(now);
 }
 void loop(uint32_t now){
  if(!attached_)return;
  {std::lock_guard<std::recursive_mutex> transaction(gateTransaction_);
  MosRequest gate;bool haveGate=false;{std::lock_guard<std::mutex> lock(mutex_);haveGate=gateRequests_.take(now,gate);}
  if(haveGate){
   commands_.cancel("gate_change");{std::lock_guard<std::mutex> lock(mutex_);requests_.cancel();}
   const bool previous=controlEnabled_.get();const bool expired=int32_t(now-gate.deadline)>=0;
   const bool saved=!expired&&controlEnabled_.save(gate.state)&&!controlEnabled_.needsSave()&&controlEnabled_.get()==gate.state;
   if(!saved)controlEnabled_.set(previous);
   // Saving the same value may not invoke a setting callback; restore effective state explicitly.
   {std::lock_guard<std::mutex> io(gateIoMutex_);enabled_.store(sbt::testprofile::controlAllowed(controlEnabled_.get()));}
   commands_.settingResult(gate,saved?"applied":"rejected",saved?"setting_saved":expired?"expired":"setting_save_failed");
  }
  }
  victron::Configuration config;
  if(mailbox_.take(seen_,config)){
   active_=config;reset(config.macValid?"connecting":"configuration_missing",true);connectRequested_=false;
   if(config.macValid){cm::BleAddress peer{};std::memcpy(peer.text,config.mac,sizeof(peer.text));peer.type=0;
    bluetooth_->configureGattPeer(peer,0xff00,0xff01,0xff02);
   }else bluetooth_->clearGattPeer();
   epoch_=bluetooth_->generation();dueAt_=now+100;
  }
  if(epoch_!=bluetooth_->generation()){
   epoch_=bluetooth_->generation();reset("reconnecting");connectRequested_=false;dueAt_=now+250;
  }
  if(cancelRequested_.exchange(false)||bluetooth_->isPaused()){
   commands_.cancel(bluetooth_->isPaused()?"ota_or_paused":"cancelled_or_disabled");
   std::lock_guard<std::mutex> lock(mutex_);requests_.cancel();
  }
  if(!enabled_.load())commands_.cancel("disabled");
  commands_.tick(now);
  if(waiting_&&controlQuery_&&expected_==0xe1&&!commands_.pending()&&!bluetooth_->isPaused()){
   // Fence an outstanding command on cancellation; never retry an uncertain write.
   cm::BleAddress peer{};std::memcpy(peer.text,active_.mac,sizeof(peer.text));bluetooth_->configureGattPeer(peer,0xff00,0xff01,0xff02);
   reset("control_cancelled");epoch_=bluetooth_->generation();connectRequested_=false;dueAt_=now+250;
  }
  if(!waiting_&&!commands_.pending()){
   MosRequest request;bool have=false;{std::lock_guard<std::mutex> lock(mutex_);have=requests_.take(request);}
   if(have){commands_.submit(request,now,enabled_.load(),connected_&&readoutHealthy_&&!bluetooth_->isPaused()&&cycle_.snapshot(now,std::min(dataTimeoutMs_.load(),uint32_t{30000})).available);dueAt_=now;}
  }
  if(active_.macValid&&!bluetooth_->isPaused()){
   if(!connected_&&!connectRequested_&&int32_t(now-dueAt_)>=0)connectRequested_=bluetooth_->requestConnect();
   if(waiting_&&uint32_t(now-sentAt_)>=2000)reset("response_timeout");
   if(connected_&&!waiting_&&int32_t(now-dueAt_)>=0){uint8_t command[9]{};size_t size=0;
    const bool control=commands_.pending();bool prepared=false;
    if(control)prepared=commands_.command(command,size);
    else {uint8_t read[7];prepared=readCommand(next_,read);std::memcpy(command,read,7);size=7;}
    bool admitted=false;
    {std::lock_guard<std::mutex> io(gateIoMutex_);admitted=prepared&&(!control||sbt::testprofile::controlAllowed(enabled_.load()))&&bluetooth_->writeCommand(command,size);}
    if(admitted){
     if(control&&commands_.expected()==0xe1)commands_.markWriteSent();
     parser_.reset();expected_=control?commands_.expected():next_;controlQuery_=control;waiting_=true;sentAt_=now;query_=0;
    }else if(control){commands_.cancel("write_admission_failed");dueAt_=now+100;}
   }
  }
  publishView(now);
 }
 bool queueRequest(const MosRequest& request){
  std::lock_guard<std::mutex> lock(mutex_);
  if constexpr(sbt::testprofile::isolated){controlView_.admission="isolated_test";return false;}
  if(request.emergency&&(!view_.available||!view_.values[14].valid||(uint32_t(view_.values[14].number)&0x0fff)==0)){controlView_.admission="rejected_no_emergency";return false;}
  const auto now=millis();const bool fresh=view_.available&&uint32_t(now-view_.receivedAt)<std::min(dataTimeoutMs_.load(),uint32_t{30000})&&!bluetooth_->isPaused();
  const auto result=requests_.offer(request,now,enabled_.load(),fresh&&controlView_.ready,cancelRequested_.load());
  controlView_.admission=result;controlView_.pending=requests_.pending();
  return !std::strcmp(result,"accepted");
 }
 bool queueGate(const MosRequest& request){
  if constexpr(sbt::testprofile::isolated)return false;
  std::lock_guard<std::recursive_mutex> transaction(gateTransaction_);
  if(request.target!=MosTarget::Gate||int32_t(millis()-request.deadline)>=0)return false;
  {std::lock_guard<std::mutex> lock(mutex_);gateRequests_.offer(request,millis());}
  // OFF fences future transport admission immediately, before deferred persistence.
  if(!request.state){std::lock_guard<std::mutex> io(gateIoMutex_);enabled_.store(false);cancelRequested_.store(true);bluetooth_->cancelPendingCommands();}
  return true;
 }
 void cancelCommands(){
  std::lock_guard<std::recursive_mutex> transaction(gateTransaction_);
  {std::lock_guard<std::mutex> lock(mutex_);gateRequests_.cancel();}
  {std::lock_guard<std::mutex> io(gateIoMutex_);enabled_.store(sbt::testprofile::controlAllowed(controlEnabled_.get()));}
  cancelRequested_.store(true);
 }
 ControlView control()const{std::lock_guard<std::mutex> lock(mutex_);return controlView_;}
 const char* reason()const{std::lock_guard<std::mutex> lock(mutex_);return viewReason_;}
 bool configured()const{return active_.macValid;}
 Snapshot snapshot()const{std::lock_guard<std::mutex> lock(mutex_);return view_;}
};
}
