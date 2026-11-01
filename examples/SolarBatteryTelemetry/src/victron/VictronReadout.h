#pragma once
#include <Arduino.h>
#include <ConfigManager.h>
#include <bluetooth/BluetoothManager.h>
#include <mbedtls/aes.h>
#include "VictronConfigMailbox.h"
namespace victron {
class ValidatedText final : public Config<String> {
    bool key_;
public:
    ValidatedText(const ConfigOptions<String>& options,bool key):Config<String>(options),key_(key){}
    bool fromJSON(const JsonVariant& value) override {
        if(!value.is<const char*>())return false;
        const String text=value.as<String>();uint8_t key[16]{};
        // An explicit empty field disables readout; malformed input keeps the previous setting.
        if(text.length()&&!(key_?parseKey(text.c_str(),text.length(),key):validMac(text.c_str(),text.length())))return false;
        set(text);return true;
    }
};
class Readout {
    ConfigMailbox mailbox_;
    ValidatedText mac_,key_;
    cm::BluetoothManager* bluetooth_=nullptr;
    Configuration active_{};uint32_t seen_=0;Receiver receiver_;
    mutable std::mutex viewMutex_;Snapshot view_{};bool attached_=false;
    static bool decrypt(const uint8_t* key,const uint8_t* initialNonce,const uint8_t* input,std::size_t size,uint8_t* output){
        mbedtls_aes_context aes;mbedtls_aes_init(&aes);
        uint8_t nonce[16]{},stream[16]{};std::memcpy(nonce,initialNonce,sizeof(nonce));size_t offset=0;
        int status=mbedtls_aes_setkey_enc(&aes,key,128);
        if(status==0)status=mbedtls_aes_crypt_ctr(&aes,size,&offset,nonce,stream,input,output);
        mbedtls_aes_free(&aes);return status==0;
    }
    void updateView(uint32_t now){
        Snapshot next=receiver_.snapshot(now);
        if(active_.ready()&&bluetooth_&&bluetooth_->isPaused()){
            next.available=false;next.reason="paused";for(auto& value:next.values)value={};
        }
        std::lock_guard<std::mutex> lock(viewMutex_);view_=next;
    }
    void receive(const cm::BleEvent& event){
        if(event.type!=cm::BleEventType::Advertisement||!active_.ready()||std::strcmp(event.peer.text,active_.mac)!=0)return;
        receiver_.receive(event.data,event.size,active_.key,event.receivedMs,event.rssi,decrypt);updateView(millis());
    }
public:
    // Settings are constructed exactly once; persisted values override these defaults in loadAll.
    Readout(const char* defaultMac,const char* defaultKey):
        mac_(ConfigOptions<String>{.key="VictronReadMac",.name="Instant Readout MAC",.category="Victron",.defaultValue=String(defaultMac),.sortOrder=1},false),
        key_(ConfigOptions<String>{.key="VictronReadKey",.name="Instant Readout key (32 hex)",.category="Victron",.defaultValue=String(defaultKey),.isPassword=true,.sortOrder=2},true){}
    Readout(const Readout&)=delete;Readout& operator=(const Readout&)=delete;
    bool attach(ConfigManagerClass& manager,cm::BluetoothManager& bluetooth){
        if(attached_)return true;
        if(mac_.hasError()||key_.hasError())return false;
        if(manager.addSetting(&mac_)!=&mac_||manager.addSetting(&key_)!=&key_)return false;
        manager.setCategoryLayoutOverride("Victron","Bluetooth","Victron","Instant Readout",41);
        manager.addSettingsPage("Bluetooth",40);manager.addSettingsCard("Bluetooth","Victron",41);
        manager.addSettingsGroup("Bluetooth","Victron","Instant Readout",41);
        manager.addToSettingsGroup(mac_.getKey(),"Bluetooth","Victron","Instant Readout",1);
        manager.addToSettingsGroup(key_.getKey(),"Bluetooth","Victron","Instant Readout",2);
        mac_.setCallback([this](const String& value){mailbox_.setMac(value.c_str(),value.length());});
        key_.setCallback([this](const String& value){mailbox_.setKey(value.c_str(),value.length());});
        bluetooth_=&bluetooth;
        manager.getRuntime().addRuntimeProvider("victron",[this](JsonObject& data){
            const auto copy=snapshot();data["reason"]=copy.available?"ok":copy.reason;
            for(std::size_t i=0;i<telemetry::channelCount;++i){const auto& channel=telemetry::channels[i];
                if(channel.source!=telemetry::Source::Victron)continue;
                const char* name=std::strchr(channel.suffix,'/')+1;const auto value=copy.values[i];
                if(!value.valid)data[name]=nullptr;
                else if(channel.type==telemetry::FieldType::State)data[name]=telemetry::chargeStates[static_cast<unsigned>(value.number)];
                else data[name]=value.number;
                data["types"][name]=telemetry::typeName(channel.type);data["support"][name]=telemetry::supportName(channel.support);
            }
        },7);
        attached_=true;return true;
    }
    // Must run after loadAll and before the server/worker starts. No raw Config reads thereafter.
    void afterLoad(){mailbox_.setMac(mac_.get().c_str(),mac_.get().length());mailbox_.setKey(key_.get().c_str(),key_.get().length());loop(millis());}
    // Run before BluetoothManager::loop, so reconfiguration invalidates queued old-peer events first.
    void loop(uint32_t now){
        if(!attached_)return;
        Configuration next;
        if(mailbox_.take(seen_,next)){
            active_=next;receiver_.reset(active_.ready()?"waiting_for_advertisement":"configuration_missing_or_invalid");
            if(active_.ready()){
                cm::BleAddress address{};std::memcpy(address.text,active_.mac,sizeof(address.text));
                // SmartSolar Instant Readout uses a random static BLE address, not a GATT connection.
                address.type=1;
                if(!bluetooth_->configureAdvertisementPeer(address)){active_.macValid=false;receiver_.reset("peer_configuration_failed");bluetooth_->clearAdvertisementPeer();}
            }else bluetooth_->clearAdvertisementPeer();
        }
        updateView(now);
    }
    void onEvent(const cm::BleEvent& event){receive(event);}
    // Application-loop/startup access only; UI readers use snapshot().
    bool configured() const{return active_.ready();}
    Snapshot snapshot() const{std::lock_guard<std::mutex> lock(viewMutex_);return view_;}
};
}
