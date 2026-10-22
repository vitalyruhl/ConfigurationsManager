#include "bluetooth/BluetoothManager.h"
#include <iostream>
#include <stdexcept>
#include <string>

static void require(bool condition,const char* message) { if(!condition)throw std::runtime_error(message); }

int main(int argc,char** argv) {
 using namespace cm;
 using namespace ble_host;
 const std::string scenario=argc>1?argv[1]:"tail";
 const bool cleanup=scenario=="cleanup",timeout=scenario=="timeout",pause=scenario=="pause";
 // Static lifetime matches the manager contract; each scenario runs in its own process.
 static BluetoothManager manager;
 ConfigManagerClass config;
 manager.attach(config);manager.settings().enabled.set(true);
 BleAddress peer{"AA:BB:CC:DD:EE:FF",0};
 require(manager.configureGattPeer(peer,1,2,3),"configure GATT");
 require(manager.configureAdvertisementPeer(peer),"configure advertisements");
 require(manager.begin(),"start worker");
 const uint8_t bytes[]={1,2};
 const int freshReadPass=pause?7:((cleanup || timeout)?6:5);
 int delivered=0;bool secondCancellation=false;uint32_t settledEpoch=0;
 manager.setEventHandler([&](const BleEvent& event) {
  if(event.type==BleEventType::Notification) {
   require(event.size==1 && event.data[0]==42,"fresh notification payload");
   require(manager.completeQuery(),"fresh response completes its query");++delivered;
  }
 });
 watermark=[&] {
  ++iteration;
  if(iteration==3) {
   if(pause) {
    manager.requestPause(BlePauseReason::Ota);
    manager.requestPause(BlePauseReason::Configuration);
   } else if(!timeout)manager.cancelPendingCommands();
  }
 };
 cancelConnect=[&] {
  if(cleanup && iteration==3 && !secondCancellation) {
   secondCancellation=true;manager.cancelPendingCommands();
  }
 };
 delay=[&] {
  manager.loop(now);
  if(iteration==1)require(manager.requestConnect(),"queue initial connection");
  if(iteration==2)require(manager.writeCommand(bytes,2),"queue initial query");
  if(iteration==3) {
   require(writes==1,"initial query reaches radio");
   if(!timeout) {
    // The old subscription must never deliver after cancellation or pause.
    NimBLEDevice::client.service.characteristic.notify(99);manager.loop(now);
    require(delivered==0,"old subscription is fenced");
   }
  }
  if(iteration==4)require(!connected,"cancellation or timeout must disconnect on the next worker pass");
  if(cleanup && iteration==5)
   require(secondCancellation && scanStops==2,"a second cancellation during cleanup must stop the newly started scan");
  if(pause && iteration==5) {
   const auto epoch=manager.generation();
   manager.requestResume(BlePauseReason::Ota);
   require(manager.isPaused(),"configuration pause survives OTA resume");
   manager.requestResume(BlePauseReason::Configuration);
   require(!manager.isPaused() && manager.generation()==epoch,"resume retains the reconciled epoch");
  }
  if(iteration==freshReadPass) {
   require(connects==2,"cancelled query reconnects with a fresh subscription");
   require(manager.writeCommand(bytes,2),"queue fresh query after recovery");
   settledEpoch=manager.generation();
  }
  if(iteration==freshReadPass+1) {
   require(writes==2,"fresh query reaches radio");
   NimBLEDevice::client.service.characteristic.notify(42);manager.loop(now);
   require(delivered==1,"fresh subscription delivers a response");
  }
  if(iteration==freshReadPass+2) {
   require(connects==2 && manager.generation()==settledEpoch,"recovery settles without another invalidation or reconnect");
   throw StopWorker{};
  }
  // Keep the first query young and inspect cleanup before natural scan expiry.
  if(iteration==1)now=10;
  else if(iteration==2)now=4990;
  else if(iteration==3)now=timeout?6990:5000;
  else if(timeout)now=iteration==4?7000:9000;
  else if(pause)now=iteration==4?5010:(iteration==5?5020:9020);
  else if(cleanup)now=iteration==4?5010:7010;
  else now=6000;
 };
 try { entry(entryValue); }
 catch(const StopWorker&) {
  std::cout<<"[MOCKED!] Real BluetoothManager worker "<<scenario<<" recovery passed\n";return 0;
 }
 catch(const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
 return 1;
}
