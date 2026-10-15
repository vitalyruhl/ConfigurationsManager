#include "bluetooth/detail/BleWrite.h"
#include "bluetooth/detail/BluetoothScheduler.h"
#include "bluetooth/detail/BoundedBleQueue.h"
#include "ota/detail/OtaLifecycleState.h"
#include "bluetooth/detail/BluetoothDispatch.h"
#include "bluetooth/detail/BluetoothEnabledSetting.h"
#include <thread>
#include <cassert>
#include <cstring>
#include <iostream>
struct Backend {
 int scans=0, stops=0, connects=0, cancels=0, writes=0;
 bool connected=false;
 void scan(){++scans;} void stopScan(){++stops;}
 bool connect(){++connects;return connected;}
 void cancel(){++cancels;connected=false;}
 bool write(const uint8_t*,size_t){++writes;return connected;}
};
struct StoredEnable {bool value=false;};
struct SettingStorageFake {
 explicit SettingStorageFake(bool initial):value(initial){}
 virtual ~SettingStorageFake()=default;
 virtual void load(StoredEnable& storage){value=storage.value;}
 bool get()const{return value;}
 void setCallback(std::function<void(bool)> callback){changed=callback;}
 void set(bool next){value=next;if(changed)changed(next);}
 bool value;std::function<void(bool)> changed;
};
int main(){
 using namespace cm;
 detail::BoundedBleQueue<BleEvent,16> q;
 BleEvent e{}; e.size=2;e.data[0]=42;
 assert(q.push(e));e.data[0]=9;
 BleEvent out{};assert(q.pop(out)&&out.data[0]==42);
 for(int i=0;i<16;++i)assert(q.push(e));
 assert(!q.push(e)&&q.drops()==1);q.clear();assert(!q.pop(out));
 detail::BoundedBleQueue<detail::BleCommand,8> commands;
 detail::BleCommand cmd{};for(int i=0;i<8;++i)assert(commands.push(cmd));
 assert(!commands.push(cmd));
 Backend b;detail::BluetoothScheduler<Backend> s(b);
 assert(s.attachOnce()&&!s.attachOnce());
 s.configure(false,true,true,0);s.tick(0);assert(s.state()==BleState::Disabled&&b.scans==0);
 s.configure(true,false,false,0);s.tick(0);assert(!s.requestConnect(0)&&b.connects==0);
 s.configure(true,true,true,0);s.tick(0);assert(b.scans==1);
 s.tick(999);assert(b.stops==0);s.tick(1000);assert(b.stops==1);
 s.tick(4999);assert(b.scans==1);s.tick(5000);assert(b.scans==2);
 assert(s.requestConnect(5000));assert(b.connects==1);
 s.tick(5999);assert(b.connects==1);s.tick(6000);assert(b.connects==2);
 b.connected=true;s.tick(8000);assert(s.connected());
 const uint8_t bytes[]={1,2};assert(s.write(bytes,2,8000));assert(!s.write(bytes,2,8001));
 auto query=s.queryId();
 assert(!s.completeQuery(query+1,s.generation()));assert(s.queryPending());
 assert(s.completeQuery(query,s.generation()));assert(!s.queryPending());
 assert(s.write(bytes,2,8001));assert(!s.completeQuery(query,s.generation()));
 auto pending=s.queryId();assert(!s.completeQuery(pending,s.generation()+1));
 s.tick(10000);assert(s.queryPending());auto beforeTimeout=s.generation();
 s.tick(10001);assert(!s.queryPending()&&!s.connected()&&s.generation()!=beforeTimeout);
 b.connected=true;s.requestConnect(10002);
 assert(s.write(bytes,2,10003));s.disconnected(10004);assert(!s.queryPending());
 auto old=s.generation();s.invalidate();assert(s.generation()!=old&&!s.accepts(old));
 auto beforePause=s.generation();s.requestPause(BlePauseReason::Ota);s.requestResume(BlePauseReason::Ota);
 assert(!s.accepts(beforePause));
 s.requestPause(BlePauseReason::Ota);s.requestPause(BlePauseReason::Configuration);s.tick(10003);
 assert(s.state()==BleState::Paused&&b.cancels>0);
 s.requestResume(BlePauseReason::Ota);assert(s.isPaused());
 s.requestResume(BlePauseReason::Configuration);assert(!s.isPaused());
 s.configure(true,true,true,0xfffffff0u);s.tick(0xfffffff0u);auto scans=b.scans;
 s.tick(0x000003d8u);assert(b.scans==scans);s.tick(0x00001378u);assert(b.scans==scans+1);
 // Backoff has the exact configured 30-second cap.
 Backend retry;detail::BluetoothScheduler<Backend> rs(retry);rs.configure(true,false,true,0);
 rs.requestConnect(0);uint32_t at=0;
 for(uint32_t delay : {1000u,2000u,4000u,8000u,16000u,30000u,30000u}) {
   auto before=retry.connects;rs.tick(at+delay-1);assert(retry.connects==before);
   at+=delay;rs.tick(at);assert(retry.connects==before+1);
 }
 Backend wrap;wrap.connected=true;detail::BluetoothScheduler<Backend> ws(wrap);
 ws.configure(true,false,true,0xfffffff0u);wrap.connected=true;ws.requestConnect(0xfffffff0u);
 assert(ws.write(bytes,2,0xfffffff0u));ws.tick(0x000007bfu);assert(ws.queryPending());
 ws.tick(0x000007c0u);assert(!ws.queryPending());
 // The same production OTA helper drives the same scheduler pause mask.
 cm::detail::OtaLifecycleState ota([&](const OtaLifecycleEvent& event){
   if(event.stage==OtaStage::Starting || event.stage==OtaStage::Succeeded)s.requestPause(BlePauseReason::Ota);
   else s.requestResume(BlePauseReason::Ota);
 });
 s.requestPause(BlePauseReason::Configuration);
 assert(ota.tryStart(OtaTransport::Http));auto upload=ota.activeSessionId();
 ota.finish(upload+1,OtaStage::Failed,0);assert(s.isPaused());
 ota.finish(upload,OtaStage::Failed,1);assert(s.isPaused());
 s.requestResume(BlePauseReason::Configuration);assert(!s.isPaused());
 assert(ota.tryStart(OtaTransport::Arduino));upload=ota.activeSessionId();
 ota.finish(upload,OtaStage::Succeeded,0);ota.finish(upload,OtaStage::Aborted,0);
 assert(s.isPaused());
 // Pause raised in a callback during connect must suppress its completion.
 struct PausingBackend : Backend { std::function<void()> pause;bool connect(){pause();return true;} } pb;
 detail::BluetoothScheduler<PausingBackend> ps(pb);pb.pause=[&]{ps.requestPause(BlePauseReason::Ota);};
 ps.configure(true,false,true,0);ps.requestConnect(0);assert(!ps.connected()&&ps.isPaused());
 // The production handoff survives disposable event-generation changes.
 detail::BluetoothDispatch dispatch;
 Backend controlBackend;detail::BluetoothScheduler<Backend> control(controlBackend);
 detail::BleCommand configuration{};configuration.enabled=true;configuration.hasGatt=true;
 configuration.gatt.text[0]='A';configuration.generation=control.invalidate();dispatch.submitConfiguration(configuration);
 auto apply=[&](const detail::BleCommand& value){configuration=value;};
 assert(dispatch.dispatchConfiguration(control,0,apply));
 assert(!dispatch.configurationPending());
 for(bool enabled : {false,true}) {
   auto next=configuration;next.enabled=enabled;next.gatt.text[0]=enabled?'C':'B';next.generation=control.invalidate();
   dispatch.submitConfiguration(next);control.requestPause(BlePauseReason::Ota);control.tick(10);
   assert(dispatch.dispatchConfiguration(control,10,apply));
   control.requestResume(BlePauseReason::Ota);control.tick(20);
   assert(configuration.enabled==enabled && configuration.gatt.text[0]==(enabled?'C':'B'));
   assert(!dispatch.configurationPending());
   if(!enabled)assert(control.state()==BleState::Disabled && !control.scanning());
 }
 auto older=configuration;older.gatt.text[0]='D';dispatch.submitConfiguration(older);
 auto newer=configuration;newer.gatt.text[0]='E';
 dispatch.dispatchConfiguration(control,30,[&](const detail::BleCommand& value){
   configuration=value;dispatch.submitConfiguration(newer);
 });
 assert(dispatch.configurationPending());
 dispatch.dispatchConfiguration(control,31,apply);
 assert(!dispatch.configurationPending() && configuration.gatt.text[0]=='E');
 // The production module-local load adapter forwards loadAll's silent storage write.
 detail::BluetoothEnabledSetting<SettingStorageFake,StoredEnable> persisted(dispatch,false);
 for(bool value : {false,true,false}) {
   StoredEnable storage{value};persisted.load(storage);assert(dispatch.enabled()==value);
 }
 persisted.set(true);assert(dispatch.enabled());persisted.set(false);assert(!dispatch.enabled());
 // Enabled callbacks and readers share only the production atomic handoff.
 dispatch.setEnabled(false);assert(!dispatch.enabled());
 std::thread settingWriter([&]{for(int i=0;i<10000;++i)dispatch.setEnabled((i&1)!=0);dispatch.setEnabled(true);});
 for(int i=0;i<10000;++i)(void)dispatch.enabled();
 settingWriter.join();assert(dispatch.enabled());
 // A queued old response cannot acknowledge the next query in the same generation.
 Backend qb;detail::BluetoothScheduler<Backend> queries(qb);
 queries.configure(true,false,true,0);qb.connected=true;queries.requestConnect(0);
 assert(queries.write(bytes,2,1));
 BleEvent q1{};q1.type=BleEventType::Notification;q1.generation=queries.generation();q1.queryId=queries.queryId();
 assert(dispatch.beginDelivery(q1,queries.queryId(),queries.queryPending()));
 detail::BleCommand complete{};assert(dispatch.makeCompletion(queries.generation(),complete));
 assert(queries.completeQuery(complete.queryId,complete.generation));assert(queries.write(bytes,2,2));
 assert(dispatch.makeCompletion(queries.generation(),complete));
 assert(complete.queryId==q1.queryId && !queries.completeQuery(complete.queryId,complete.generation));
 dispatch.endDelivery();
 assert(!dispatch.beginDelivery(q1,queries.queryId(),queries.queryPending()));
 assert(!dispatch.makeCompletion(queries.generation(),complete));
 auto q2=q1;q2.queryId=queries.queryId();
 assert(dispatch.beginDelivery(q2,queries.queryId(),queries.queryPending()));
 assert(dispatch.makeCompletion(queries.generation(),complete));
 assert(queries.completeQuery(complete.queryId,complete.generation));dispatch.endDelivery();
 // [MOCKED!] Characteristic properties exercise the production write selector.
 struct Characteristic {bool acknowledged,unacknowledged,response=false;int calls=0;
  bool canWrite()const{return acknowledged;}bool canWriteNoResponse()const{return unacknowledged;}
  bool writeValue(const uint8_t*,size_t,bool r){response=r;++calls;return true;}};
 Characteristic ack{true,false},noAck{false,true},neither{false,false};
 assert(detail::writeCharacteristic(&ack,bytes,2)&&ack.response);
 assert(detail::writeCharacteristic(&noAck,bytes,2)&&!noAck.response);
 assert(!detail::writeCharacteristic(&neither,bytes,2)&&!neither.calls);
 assert(!detail::writeCharacteristic<Characteristic>(nullptr,bytes,2));
 // Removing GATT preserves advertisement scanning and forbids reconnect.
 queries.configure(true,true,false,50);queries.tick(51);assert(queries.scanning());
 assert(!queries.requestConnect(52)&&!queries.connected());
 std::cout<<"Bluetooth transport checks passed\n";
}
