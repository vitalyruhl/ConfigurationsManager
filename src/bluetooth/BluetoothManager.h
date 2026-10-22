#pragma once
#include "ConfigManager.h"
#include "BluetoothTypes.h"
#include "detail/BleWrite.h"
#include "detail/BoundedBleQueue.h"
#include "detail/BluetoothScheduler.h"
#include "detail/BluetoothDispatch.h"
#include "detail/BluetoothEnabledSetting.h"
#include "detail/BluetoothCommandFence.h"
// Explicit consumer dependency: h2zero/NimBLE-Arduino@2.5.1.
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <cstring>
#include <functional>
#include <mutex>

namespace cm {
// Static lifetime only. Except pause/resume/cancelPendingCommands, public methods run in the application loop.
class BluetoothManager {
public:
  using EnabledSetting=detail::BluetoothEnabledSetting<Config<bool>,Preferences>;
  struct Settings {
    explicit Settings(detail::BluetoothDispatch& dispatch)
      : enabled(dispatch,ConfigOptions<bool>{.key="BluetoothEnable", .name="Enable Bluetooth", .category="Bluetooth", .defaultValue=false, .sortOrder=1}){}
    EnabledSetting enabled;
  };
  BluetoothManager() : settings_(dispatch_), scheduler_(*this), scanCallbacks_(*this), clientCallbacks_(*this) {}
  BluetoothManager(const BluetoothManager&) = delete;
  BluetoothManager& operator=(const BluetoothManager&) = delete;
  Settings& settings() { return settings_; }
  void attach(ConfigManagerClass& cm) {
    if (!scheduler_.attachOnce()) return;
    cm_ = &cm;
    cm.addSetting(&settings_.enabled);
    addBluetoothSettingsToSettingsGroup(cm,"Bluetooth","Bluetooth","Bluetooth",40);
    addBluetoothRuntimeProviderToGUI(cm);
  }
  void addBluetoothSettingsToSettingsGroup(ConfigManagerClass& cm, const char* page, const char* group, int order) {
    addBluetoothSettingsToSettingsGroup(cm,page,page,group,order);
  }
  void addBluetoothSettingsToSettingsGroup(ConfigManagerClass& cm, const char* page, const char* card, const char* group, int order) {
    cm.addSettingsPage(page,order); cm.addSettingsCard(page,card,order); cm.addSettingsGroup(page,card,group,order);
    cm.addToSettingsGroup(settings_.enabled.getKey(),page,card,group,order);
  }
  void addBluetoothRuntimeProviderToGUI(ConfigManagerClass& cm, const char* group="bluetooth", int providerOrder=4, int baseOrder=10) {
    (void)baseOrder;
    if (runtimeRegistered_) return;
    runtimeRegistered_ = true;
    cm.getRuntime().addRuntimeProvider(group,[this](JsonObject& data) {
      data["enabled"] = dispatch_.enabled(); data["state"] = static_cast<int>(scheduler_.state());
      data["connected"] = scheduler_.connected(); data["scanning"] = scheduler_.scanning();
      data["queueDrops"] = events_.drops() + commands_.drops() + oversized_.load(); data["reconnects"] = reconnects_.load();
      data["lastError"] = lastError_.load(); data["lateEvents"] = lateEvents_.load();
      data["workerStackHighWaterBytes"] = highWaterBytes_.load();
    },providerOrder);
  }
  bool begin() {
    if (!cm_) return false;
    if (worker_) return true;
    BluetoothManager* expected=nullptr;
    if (!owner_.compare_exchange_strong(expected,this) && expected!=this) return false;
    enabledCached_ = dispatch_.enabled();
    if (!queueConfiguration()) return false;
    // ESP-IDF FreeRTOS xTaskCreate stack depth and high-water units are bytes.
    if (xTaskCreate(workerEntry,"cm-ble",8192,this,1,&worker_) != pdPASS) {
      worker_ = nullptr; owner_=nullptr; lastError_ = -1002; return false;
    }
    return true;
  }
  bool configureAdvertisementPeer(const BleAddress& address) {
    if (!valid(address)) return false;
    desired_.advertisement = address; desired_.hasAdvertisement = true;
    return !worker_ || queueConfiguration();
  }
  bool configureGattPeer(const BleAddress& address, uint16_t service, uint16_t notify, uint16_t writeUuid) {
    if (!valid(address) || !service || !notify || !writeUuid) return false;
    desired_.gatt=address; desired_.service=service; desired_.notify=notify; desired_.write=writeUuid; desired_.hasGatt=true;
    return !worker_ || queueConfiguration();
  }
  // Remove only this source; the other peer remains configured.
  bool clearAdvertisementPeer() { desired_.hasAdvertisement=false;return !worker_ || queueConfiguration(); }
  bool clearGattPeer() { desired_.hasGatt=false;return !worker_ || queueConfiguration(); }
  bool requestConnect() {
    if (!worker_ || !enabledCached_ || !desired_.hasGatt || isPaused()) return false;
    detail::BleCommand cmd{};cmd.type=detail::BleCommandType::Connect;cmd.generation=generation();
    return enqueue(cmd);
  }
  bool writeCommand(const uint8_t* data, size_t size) {
    if (!worker_ || !data || !size || size>CM_BLE_TX_MAX_BYTES || !scheduler_.connected() || isPaused()) return false;
    detail::BleCommand cmd{};
    return detail::makeWriteCommand(cmd,data,size,generation()) && enqueue(cmd);
  }
  // Thread-safe. Fence buffered commands; an already-started backend write completes before return.
  // The existing generation-change lifecycle recovers the link without changing configuration or pause ownership.
  void cancelPendingCommands() { commandFence_.cancel(scheduler_); }
  // Acknowledge only a fully validated response, never a partial notification.
  bool completeQuery() {
    if(!worker_ || !scheduler_.queryPending() || isPaused())return false;
    detail::BleCommand cmd{};
    return dispatch_.makeCompletion(generation(),cmd) && enqueue(cmd);
  }
  void requestPause(BlePauseReason reason) { scheduler_.requestPause(reason); }
  void requestResume(BlePauseReason reason) { scheduler_.requestResume(reason); }
  bool isPaused() const { return scheduler_.isPaused(); }
  uint32_t generation() const { return scheduler_.generation(); }
  void setEventHandler(std::function<void(const BleEvent&)> handler) { handler_=std::move(handler); }
  void loop(uint32_t nowMs) {
    (void)nowMs;
    if (worker_ && dispatch_.enabled()!=enabledCached_) {
      enabledCached_=dispatch_.enabled(); queueConfiguration();
    }
    // Bounded work even if radio callbacks continuously produce events.
    BleEvent event{};
    for (size_t i=0;i<16 && events_.pop(event);++i) {
      if (dispatch_.enabled()!=enabledCached_) {
        enabledCached_=dispatch_.enabled(); queueConfiguration();
      }
      if (overflow_.load() || !scheduler_.accepts(event.generation) || !enabledCached_) { ++lateEvents_;continue; }
      if(!dispatch_.beginDelivery(event,scheduler_.queryId(),scheduler_.queryPending())) {++lateEvents_;continue;}
      auto handler=handler_; // A handler may replace itself or reconfigure safely.
      if (handler) handler(event);
      dispatch_.endDelivery();
    }
  }
private:
  class ScanCallbacks : public NimBLEScanCallbacks {
  public:
    explicit ScanCallbacks(BluetoothManager& owner):owner_(owner){}
    void onResult(const NimBLEAdvertisedDevice* device) override { owner_.advertisement(device); }
  private: BluetoothManager& owner_;
  };
  class ClientCallbacks : public NimBLEClientCallbacks {
  public:
    explicit ClientCallbacks(BluetoothManager& owner):owner_(owner){}
    void onDisconnect(NimBLEClient*,int reason) override {
      owner_.radioConnected_=false;
      owner_.emit(BleEventType::Disconnected,nullptr,0,owner_.linkGeneration_.load(),reason);
    }
  private: BluetoothManager& owner_;
  };
  static bool valid(const BleAddress& address) {
    if (address.type>3 || address.text[17]!=0) return false;
    for (size_t i=0;i<17;++i) {
      char c=address.text[i];
      if (i%3==2) {if(c!=':') return false;}
      else if (!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'))) return false;
    }
    return true;
  }
  bool enqueue(const detail::BleCommand& cmd) {
    if(commands_.push(cmd)) return true;
    overflow_=true; return false;
  }
  bool queueConfiguration() {
    auto config=desired_;config.enabled=enabledCached_;
    scheduler_.invalidate();
    dispatch_.submitConfiguration(config);return true;
  }
  void emit(BleEventType type,const uint8_t* data,size_t size,uint32_t epoch,int error=0,int8_t rssi=0,bool advertisement=false) {
    if (!scheduler_.accepts(epoch)) {++lateEvents_;return;}
    if(size>272 || (size && !data)) {overflow_=true;++oversized_;return;}
    BleEvent event{};event.type=type;event.receivedMs=millis();event.generation=epoch;event.errorCode=error;event.rssi=rssi;
    if(type==BleEventType::Notification)event.queryId=scheduler_.queryPending()?scheduler_.queryId():0;
    {
      std::unique_lock<std::mutex> lock(peerMutex_,std::try_to_lock);
      if(!lock.owns_lock()){overflow_=true;return;}
      event.peer=advertisement?workerConfig_.advertisement:workerConfig_.gatt;
    }
    event.size=size;if(size)memcpy(event.data,data,size);
    if(!events_.push(event))overflow_=true;
  }
  void advertisement(const NimBLEAdvertisedDevice* device) {
    const auto epoch=scanGeneration_.load();
    if(!scheduler_.accepts(epoch)){++lateEvents_;return;}
    BleAddress expected{};
    {
      std::unique_lock<std::mutex> lock(peerMutex_,std::try_to_lock);
      if(!lock.owns_lock()){overflow_=true;return;}
      if(!workerConfig_.hasAdvertisement)return;
      expected=workerConfig_.advertisement;
    }
    if(device->getAddress()!=NimBLEAddress(std::string(expected.text),expected.type) || !device->haveManufacturerData())return;
    // NimBLE returns manufacturer bytes INCLUDING the two-byte company ID.
    auto bytes=device->getManufacturerData();
    if(bytes.empty())return;
    emit(BleEventType::Advertisement,reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size(),epoch,0,device->getRSSI(),true);
  }
  void ensureRadio() {
    if(radioInitialized_)return;
    NimBLEDevice::init("");
    auto* scanner=NimBLEDevice::getScan();scanner->setScanCallbacks(&scanCallbacks_,true);
    scanner->setActiveScan(false);scanner->setMaxResults(0);
    client_=NimBLEDevice::createClient();
    if(client_){client_->setClientCallbacks(&clientCallbacks_,false);client_->setConnectTimeout(2000);client_->setConnectRetries(0);}
    radioInitialized_=true;
  }
  // Backend operations below are used only by the one worker through BluetoothScheduler.
  void scan() {ensureRadio();scanGeneration_=generation();NimBLEDevice::getScan()->start(1000,false,true);}
  void stopScan() {if(radioInitialized_)NimBLEDevice::getScan()->stop();}
  bool connect() {
    ensureRadio();++reconnects_;linkGeneration_=generation();writeCharacteristic_=nullptr;
    if(!client_ || !client_->connect(NimBLEAddress(std::string(workerConfig_.gatt.text),workerConfig_.gatt.type)))return false;
    radioConnected_=true;
    auto* service=client_->getService(NimBLEUUID(workerConfig_.service));
    auto* notify=service?service->getCharacteristic(NimBLEUUID(workerConfig_.notify)):nullptr;
    writeCharacteristic_=service?service->getCharacteristic(NimBLEUUID(workerConfig_.write)):nullptr;
    const auto epoch=linkGeneration_.load();
    if(!scheduler_.accepts(epoch) || !notify || !notify->canNotify() || !writeCharacteristic_ ||
       (!writeCharacteristic_->canWrite() && !writeCharacteristic_->canWriteNoResponse()) ||
       !notify->subscribe(true,[this,epoch](NimBLERemoteCharacteristic*,uint8_t* data,size_t size,bool) {
         emit(BleEventType::Notification,data,size,epoch);
       })) {cancel();return false;}
    return true;
  }
  void cancel() {
    writeCharacteristic_=nullptr;
    if(client_){client_->cancelConnect();if(client_->isConnected())client_->disconnect();}
    radioConnected_=false;
  }
  bool write(const uint8_t* data,size_t size) {
    return detail::writeCharacteristic(writeCharacteristic_,data,size);
  }
  static void workerEntry(void* value) {static_cast<BluetoothManager*>(value)->run();}
  void run() {
    uint32_t observed=generation();
    for(;;) {
      uint32_t now=millis();
      if(overflow_.load()) {
        scheduler_.invalidate();scheduler_.cancel();commands_.clear();events_.clear();
        overflow_=false;dispatch_.requestReapply();
        lastError_=-1001;emit(BleEventType::Error,nullptr,0,generation(),-1001);
        scheduler_.disconnected(now);
      }
      const auto current=generation();
      if(observed!=current) {
        scheduler_.cancel();scheduler_.disconnected(now);
        // A cancellation during cleanup must remain pending for the next pass.
        observed=current;
      }
      auto applyConfiguration=[this](const detail::BleConfiguration& config) {
        std::lock_guard<std::mutex> lock(peerMutex_);workerConfig_=config;
      };
      dispatch_.dispatchConfiguration(scheduler_,now,applyConfiguration);
      detail::BleCommand cmd{};
      for(size_t i=0;i<8 && !dispatch_.configurationPending() && commands_.pop(cmd);++i) {
        if(cmd.generation!=generation() || dispatch_.configurationPending())continue;
        if(cmd.type==detail::BleCommandType::Connect) {
          scheduler_.requestConnect(now);
          if(scheduler_.connected())emit(BleEventType::Connected,nullptr,0,generation());
        } else if(cmd.type==detail::BleCommandType::CompleteQuery) {
          scheduler_.completeQuery(cmd.queryId,cmd.generation);
        } else if(commandFence_.writeIfCurrent(scheduler_,cmd,now)==detail::BleWriteResult::Failed) {
          lastError_=-1003;emit(BleEventType::Error,nullptr,0,generation(),-1003);
        }
      }
      if(dispatch_.configurationPending()) {scheduler_.cancel();vTaskDelay(pdMS_TO_TICKS(10));continue;}
      if(scheduler_.connected() && !radioConnected_)scheduler_.disconnected(now);
      const auto attempts=reconnects_.load();
      auto beforeTick=generation();
      scheduler_.tick(now);
      if(generation()!=beforeTick && !isPaused()) {
        lastError_=-1005;emit(BleEventType::Error,nullptr,0,generation(),-1005);
      }
      if(reconnects_.load()!=attempts && scheduler_.connected())emit(BleEventType::Connected,nullptr,0,generation());
      if(reconnects_.load()!=attempts && !scheduler_.connected() && !isPaused()) {
        lastError_=-1004;emit(BleEventType::Error,nullptr,0,generation(),-1004);
      }
      highWaterBytes_=uxTaskGetStackHighWaterMark(nullptr);
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  friend class detail::BluetoothScheduler<BluetoothManager>;
  inline static std::atomic<BluetoothManager*> owner_{nullptr};
  ConfigManagerClass* cm_=nullptr;
  detail::BluetoothDispatch dispatch_;
  Settings settings_;
  detail::BluetoothScheduler<BluetoothManager> scheduler_;
  detail::BoundedBleQueue<BleEvent,16> events_;
  detail::BoundedBleQueue<detail::BleCommand,8> commands_;
  detail::BleConfiguration desired_{},workerConfig_{};
  std::mutex peerMutex_;
  detail::BluetoothCommandFence commandFence_;
  ScanCallbacks scanCallbacks_;ClientCallbacks clientCallbacks_;
  NimBLEClient* client_=nullptr;
  NimBLERemoteCharacteristic* writeCharacteristic_=nullptr;
  TaskHandle_t worker_=nullptr;
  std::function<void(const BleEvent&)> handler_;
  bool enabledCached_=false,runtimeRegistered_=false,radioInitialized_=false;
  std::atomic<bool> overflow_{false},radioConnected_{false};
  std::atomic<uint32_t> scanGeneration_{0},linkGeneration_{0},lateEvents_{0},oversized_{0},reconnects_{0},highWaterBytes_{0};
  std::atomic<int> lastError_{0};
};
} // namespace cm
