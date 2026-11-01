#pragma once
#include <Arduino.h>
#include <ConfigManager.h>
#include "runtime/AppRuntimeMetadata.h"
#include "diagnostics/PanicSummaryPublisher.h"
#include "diagnostics/RuntimePublisher.h"
#include <esp_heap_caps.h>
#include <freertos/task.h>
#include <mqtt/MQTTManager.h>
#include <atomic>
#include "MqttConfigMailbox.h"
#include "MqttLoopPolicy.h"
#include "MqttUiSettings.h"
#include "StatusPublisher.h"
#include "VictronHistoryPublisher.h"
#include "CellPublisher.h"
#include "PacedPublication.h"
#include "BmsCommandEnvelope.h"
#include "bms/BmsReadout.h"
#include <ctime>
namespace telemetry {
class MqttService final : public Transport {
 // UI/persistence and network settings have separate objects and key namespaces.
 std::unique_ptr<MqttUiSettings> ui_;
 cm::MQTTManager& mqtt_=cm::MQTTManager::instance();
 ConfigMailbox mailbox_;
 MqttConfig desired_{};uint32_t seen_=0;
 Publisher publisher_;VictronHistoryPublisher victronHistory_;CellPublisher cells_;PacedPublication paced_;
 sbt::diagnostics::PanicSummaryPublisher panicSummary_;
 sbt::diagnostics::RuntimePublisher runtime_;
 StatusPublisher statuses_;CommandSession session_;bms::Readout* bms_=nullptr;
 bool subscribed_=false;uint32_t subscribeAt_=0,lastResultSequence_=0;
 struct Health {bool initialized=false,available=false;const char* reason="unknown";uint32_t sentAt=0,receivedAt=0,sequence=0;} health_[2];
 void newSession(){statuses_.clearResult();char token[33];for(unsigned i=0;i<4;++i)std::snprintf(token+i*8,9,"%08lx",static_cast<unsigned long>(esp_random()));session_.begin(token);if(bms_){lastResultSequence_=bms_->control().result.sequence;bms_->cancelCommands();}result("","idle","no_request");}
 void subscribeCommands(uint32_t now){subscribeAt_=now;subscribed_=true;for(const auto* suffix:commandSuffixes){char topic[topicLimit+1];subscribed_=buildTopic(publisher_.root(),suffix,topic,sizeof(topic))&&mqtt_.subscribe(topic,0)&&subscribed_;}}
 void result(const char* id,const char* phase,const char* reason){char payload[384];const auto sample=bms_?bms_->snapshot():bms::Snapshot{};
  if(formatCommandResult(id,phase,reason,sample.values.data(),sample.available,payload,sizeof(payload),session_.token()))statuses_.set(3,payload);
 }
 void message(const char* topic,const byte* payload,unsigned int length){
  const auto n=std::strlen(publisher_.root());if(std::strncmp(topic,publisher_.root(),n)||topic[n]!='/')return;
  bms::MosRequest request;const char* reason=session_.accept(topic+n+1,reinterpret_cast<const char*>(payload),length,std::time(nullptr)>0?static_cast<uint64_t>(std::time(nullptr)):0,millis(),request);
  if(std::strcmp(reason,"ok")){result("","rejected",reason);if(!std::strcmp(reason,"session_exhausted"))newSession();return;}
  if(!bms_){result(request.id,"rejected","adapter_unavailable");return;}
  if(request.target==bms::MosTarget::Gate){
   if(otaPaused_.load()||!subscribed_||!bms_->queueGate(request))result(request.id,"rejected","setting_unavailable");
   return;
  }
  const auto state=bms_->control();
  if(otaPaused_.load()||!state.ready||!subscribed_){result(request.id,"rejected",otaPaused_.load()?"ota":!state.enabled?"disabled":state.pending?"busy":"unavailable");return;}
  if(!bms_->queueRequest(request))result(request.id,"rejected",bms_->control().admission);
 }
 void health(unsigned index,bool available,const char* reason,uint32_t receivedAt,uint32_t sequence,uint32_t now){
  auto& state=health_[index];const bool changed=!state.initialized||available!=state.available||std::strcmp(reason,state.reason);
  if(index==1&&available&&(!state.initialized||receivedAt!=state.receivedAt))++state.sequence;
  state.receivedAt=receivedAt;if(index==0)state.sequence=sequence;
  if(!changed&&uint32_t(now-state.sentAt)<5000)return;
  state.initialized=true;state.available=available;state.reason=reason;state.sentAt=now;
  char age[20]="null",payload[256];if(available)std::snprintf(age,sizeof(age),"%lu",static_cast<unsigned long>(uint32_t(now-receivedAt)));
  std::snprintf(payload,sizeof(payload),"{\"available\":%s,\"reason\":\"%s\",\"sample_sequence\":%lu,\"sample_age_ms\":%s}",available?"true":"false",reason,static_cast<unsigned long>(state.sequence),age);statuses_.set(index,payload,true);
 }
 void controlStatus(){if(!bms_)return;const auto state=bms_->control();const bool clockReady=std::time(nullptr)>=1577836800;
  const bool ready=state.ready&&clockReady&&subscribed_&&mqtt_.isConnected()&&!otaPaused_.load();publisher_.setControl(state.enabled,ready);
  char payload[384];
  if(formatControlStatus(state.enabled,ready,state.pending,session_.token(),!state.enabled?"disabled":!clockReady?"clock_unavailable":state.pending?"busy":ready?"ok":"unavailable",
      state.permissionsKnown,state.permissionMask,state.readback,payload,sizeof(payload),state.peer))statuses_.set(2,payload);
  statuses_.clearPermissions(); // Retire historical ACK state as a control/display source.
  if(state.result.sequence!=lastResultSequence_){lastResultSequence_=state.result.sequence;result(state.result.id,state.result.phase,state.result.reason);}
 }

 bool oldEndpointUnconfirmed_=false;
 std::atomic<bool> otaPaused_{false};
 struct View {uint32_t fullRefreshMs=60000;char root[rootLimit+1]="SolarBatteryTelemetry";bool enabled=false,connected=false,rejected=false,delivered=false,cleanupPending=false,oldEndpointUnconfirmed=false;int state=0;};
 std::mutex viewMutex_;View view_;
 void captureText(TextField field,const String& value){
  if constexpr(sbt::testprofile::isolated){
   if(field==TextField::Root){mailbox_.text(field,sbt::testprofile::root,std::strlen(sbt::testprofile::root));return;}
   if(field==TextField::ClientId){mailbox_.text(field,sbt::testprofile::client,std::strlen(sbt::testprofile::client));return;}
  }
  mailbox_.text(field,value.c_str(),value.length());
 }
 void captureInitial() {
  mailbox_.enabled(ui_->enableMQTT.get());captureText(TextField::Server,ui_->server.get());mailbox_.port(ui_->port.get());
  captureText(TextField::Username,ui_->username.get());captureText(TextField::Password,ui_->password.get());
  captureText(TextField::ClientId,ui_->clientId.get());captureText(TextField::Root,ui_->publishTopicBase.get());
  mailbox_.intervals(ui_->publishIntervalSec.get(),ui_->listenIntervalMs.get());
  mailbox_.fullRefreshInterval(ui_->fullRefreshIntervalSec.get());
 }
 void installCallbacks(){
  ui_->enableMQTT.setCallback([this](bool v){mailbox_.enabled(v);});
  ui_->server.setCallback([this](const String& v){captureText(TextField::Server,v);});
  ui_->port.setCallback([this](int v){mailbox_.port(v);});
  ui_->username.setCallback([this](const String& v){captureText(TextField::Username,v);});
  ui_->password.setCallback([this](const String& v){captureText(TextField::Password,v);});
  ui_->clientId.setCallback([this](const String& v){captureText(TextField::ClientId,v);});
  ui_->publishTopicBase.setCallback([this](const String& v){captureText(TextField::Root,v);});
  ui_->publishIntervalSec.setCallback([this](float v){mailbox_.publishInterval(v);});
  ui_->listenIntervalMs.setCallback([this](int v){mailbox_.listenInterval(v);});
  ui_->fullRefreshIntervalSec.setCallback([this](int v){mailbox_.fullRefreshInterval(v);});
 }
 void updateView(){std::lock_guard<std::mutex> guard(viewMutex_);std::memcpy(view_.root,publisher_.root(),std::strlen(publisher_.root())+1);
  view_.enabled=desired_.enabled;view_.connected=mqtt_.isConnected();view_.rejected=desired_.rootRejected||desired_.configRejected||publisher_.rootRejected();
  view_.cleanupPending=publisher_.retirementPending();view_.oldEndpointUnconfirmed=oldEndpointUnconfirmed_;
  view_.fullRefreshMs=desired_.fullRefreshMs;view_.delivered=publisher_.delivered();view_.state=static_cast<int>(mqtt_.getState());}

public:
 bool capturePanicSummary(const sbt::diagnostics::PanicSummary& summary){return panicSummary_.capture(summary);}
 bool attach(ConfigManagerClass& cm,const char* broker,int port,const char* user,const char* password,bool enableDefault){
  if(ui_)return false; // Persistent settings must be constructed and registered only once.
  char client[65];formatClientId(ESP.getEfuseMac(),client,sizeof(client));
  ui_=std::make_unique<MqttUiSettings>(broker,port,user,password,enableDefault,client);
  Preferences prefs;
  if(!prefs.begin("ConfigManager",false))return false;
  const bool migrated=ui_->migrate(prefs);
  prefs.end();
  if(!migrated)return false;
  cm.setCategoryLayoutOverride("MQTT","MQTT","MQTT","MQTT Settings",40);cm.addSettingsPage("MQTT",40);cm.addSettingsGroup("MQTT","MQTT","MQTT Settings",40);
  if(!ui_->attachTo(cm))return false;
  // Deliberately no MQTTManager::attach(): it would expose loop-owned settings to AsyncWeb writes.
  // Standalone public settings/begin/loop retain System-Info/ESP and /WiFi conventions.
  mqtt_.setBufferSize(sbt::diagnostics::mqttBufferBytes);mqtt_.setRetryInterval(10000);mqtt_.setMaxRetries(10);mqtt_.setKeepAlive(30);
  mqtt_.onMQTTConnect([this](){publisher_.onConnected();victronHistory_.onConnected();cells_.onConnected();runtime_.onConnected();statuses_.onConnected();newSession();subscribeCommands(millis());});
  mqtt_.onMQTTDisconnect([this](){session_.clear();subscribed_=false;if(bms_)bms_->cancelCommands();});
  mqtt_.onMessage([this](const char* topic,const byte* payload,unsigned int length){message(topic,payload,length);});
  installCallbacks();
  cm.addLivePage("Connectivity",40);cm.addLivePage("BMS",50);cm.addLivePage("Victron",55);
  for(const char* card:{"MQTT","bms","victron"}){
   const char* page=std::strcmp(card,"bms")==0?"BMS":std::strcmp(card,"victron")==0?"Victron":"Connectivity";
   const char* liveCard=std::strcmp(card,"MQTT")==0?"Connectivity":card;
   cm.addLiveCard(page,liveCard,45);cm.addLiveGroup(page,liveCard,card,45);
  }
  cm.getRuntime().addRuntimeProvider("telemetry",[this](JsonObject& data){
   View v;{std::lock_guard<std::mutex> guard(viewMutex_);v=view_;}
   data["scope"]="Victron readout and guarded JBD MOS controls";data["root"]=String(v.root);
   data["connection"]=v.connected?"online":"offline";data["mqttEnabled"]=v.enabled?"enabled":"disabled";
   data["rootValidation"]=v.rejected?"rejected; last valid configuration retained":"valid";
   data["snapshot"]=v.delivered?"locally sent (QoS0)":"pending";
   data["full_refresh_interval_s"]=v.fullRefreshMs/1000u;
   data["ota"]=otaPaused_.load()?"paused":"ready";
   data["retirement"]=v.oldEndpointUnconfirmed?"previous broker cleanup unconfirmed":v.cleanupPending?"same broker cleanup pending":"complete";
  },6);
  if(!sbt::metadata::registerTelemetryFields(cm.getRuntime()))return false;
  return true;
 }
 // Called once after loadAll and before server/tasks. Config::load intentionally omits callbacks.
 void bindBms(bms::Readout& readout){bms_=&readout;}
 void afterLoad(){
  if constexpr(sbt::testprofile::isolated){ui_->publishTopicBase.set(sbt::testprofile::root);ui_->clientId.set(sbt::testprofile::client);}
  captureInitial();
 }
 void pauseForOta(bool value){otaPaused_.store(value);}
 void updateBms(const bms::Snapshot& sample,const char* reason,uint32_t now){
  cells_.observe(sample,now,static_cast<int64_t>(std::time(nullptr)));
  health(0,sample.available,reason,sample.receivedAt,sample.sequence,now);
  for(std::size_t i=0;i<channelCount;++i)if(channels[i].source==Source::Bms)publisher_.setValue(i,sample.available?sample.values[i]:Value{});
  publisher_.setSourceAvailable(Source::Bms,sample.available);
 }
 void updateVictron(const Value* values,bool available,const char* reason,uint32_t receivedAt,uint32_t now){
  victronHistory_.observe(values,available,receivedAt,now,static_cast<int64_t>(std::time(nullptr)));
  health(1,available,reason,receivedAt,0,now);
  for(std::size_t i=0;i<channelCount;++i)if(channels[i].source==Source::Victron)publisher_.setValue(i,available?values[i]:Value{});
  publisher_.setSourceAvailable(Source::Victron,available);
 }
 void loop(uint32_t now){
  if(otaPaused_.load()){publisher_.setPaused(true);return;}
  publisher_.setPaused(false);
  if(mqtt_.isConnected()&&!subscribed_&&uint32_t(now-subscribeAt_)>=1000)subscribeCommands(now);
  controlStatus();
  MqttConfig next;
  if(mailbox_.take(seen_,next)){
   const bool endpointChanged=desired_.port!=next.port||std::strcmp(desired_.server,next.server);
   const bool reconnect=desired_.enabled!=next.enabled||desired_.port!=next.port||std::strcmp(desired_.server,next.server)||
    std::strcmp(desired_.username,next.username)||std::strcmp(desired_.password,next.password)||std::strcmp(desired_.clientId,next.clientId);
   desired_=next;
   if(reconnect){
    const bool panicRetired=panicSummary_.retireRoot(*this,now);
    if(endpointChanged){if(!panicRetired)oldEndpointUnconfirmed_=true;panicSummary_.abandonEndpoint();}
    const auto cleanup=disconnectForReconfigure(publisher_,*this,endpointChanged);
    if(cleanup==CleanupResult::UnconfirmedOldEndpoint)oldEndpointUnconfirmed_=true;
    // DeferredSameEndpoint remains in Publisher; disable never waits indefinitely for the broker.
   }
   auto& live=mqtt_.settings();
   live.enableMQTT.set(next.enabled);mqtt_.setServer(next.server,next.port);mqtt_.setCredentials(next.username,next.password);mqtt_.setClientId(next.clientId);
   live.publishIntervalSec.set(next.publishMs/1000.0f);live.listenIntervalMs.set(next.listenMs);publisher_.setInterval(next.publishMs);publisher_.setRefreshInterval(next.fullRefreshMs);statuses_.setRefreshInterval(next.fullRefreshMs);cells_.setInterval(next.publishMs);cells_.setRefreshInterval(next.fullRefreshMs);
   runtime_.setRefreshInterval(next.fullRefreshMs);
   if(reconnect&&next.enabled)configure(publisher_.root());
  }
  if(std::strcmp(publisher_.root(),desired_.root))panicSummary_.retireRoot(*this,now);
  publisher_.requestRoot(desired_.root,std::strlen(desired_.root));
  // Resolve root transitions before CM's automatic System-Info publication.
  runPreparedMqttLoop(publisher_,now,*this,desired_.enabled&&desired_.server[0],
   [this](){return otaPaused_.load();},[this](){mqtt_.loop();},0);
  if(otaPaused_.load()||!desired_.enabled||!desired_.server[0]){updateView();return;}
  if(runtime_.due(now)){
   multi_heap_info_t heap{};heap_caps_get_info(&heap,MALLOC_CAP_DEFAULT);
   // ESP-IDF task.h defines this high-water value in bytes, unlike vanilla FreeRTOS.
   runtime_.observe(now,uxTaskGetStackHighWaterMark(nullptr),heap.total_free_bytes,heap.largest_free_block,heap.minimum_free_bytes);
  }
  controlStatus();statuses_.tick(now,publisher_.root(),*this,1);
  paced_.tick(now,*this,[this,now](unsigned slot,Transport& wire){
   if(otaPaused_.load())return;
   if(slot==0)publisher_.tick(now,wire,1);
   else if(slot==1)cells_.tick(now,publisher_.root(),wire);
   else if(slot==2)statuses_.tick(now,publisher_.root(),wire,2);
   else if(slot==3)victronHistory_.tick(now,publisher_.root(),wire);
   else if(slot==4)panicSummary_.tick(now,publisher_.root(),wire);
   else runtime_.tick(now,publisher_.root(),wire);
  });
  updateView();
 }
 bool connected() const override{return mqtt_.isConnected();}
 bool publish(const char* topic,const char* payload,bool retained) override {
  return sbt::testprofile::topicAllowed(topic)&&!otaPaused_.load()&&std::strlen(topic)<=topicLimit&&std::strlen(payload)<=payloadLimit&&mqtt_.publishRaw(topic,payload,retained);
 }
 void disconnect() override{session_.clear();subscribed_=false;if(bms_)bms_->cancelCommands();mqtt_.disconnect();}
 void configure(const char* root) override{
  root=sbt::testprofile::mqttRoot(root);
  char will[topicLimit+1];if(!buildTopic(root,"status",will,sizeof(will)))return;
  mqtt_.settings().publishTopicBase.set(String(root));mqtt_.setLastWill(will,"offline",true,0);
  if(desired_.enabled&&desired_.server[0])mqtt_.begin();
 }
};
}
