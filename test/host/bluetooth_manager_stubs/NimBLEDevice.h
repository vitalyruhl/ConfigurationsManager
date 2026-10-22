#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
// [MOCKED!] NimBLE radio boundary. Callbacks retain the production subscription epoch.
class NimBLEAddress {
public:
 NimBLEAddress(std::string,uint8_t) {}
 bool operator!=(const NimBLEAddress&) const { return false; }
};
class NimBLEUUID { public: explicit NimBLEUUID(uint16_t) {} };
class NimBLEAdvertisedDevice {
public:
 NimBLEAddress getAddress() const { return NimBLEAddress("",0); }
 bool haveManufacturerData() const { return true; }
 std::string getManufacturerData() const { return "12"; }
 int8_t getRSSI() const { return -50; }
};
class NimBLEScanCallbacks { public: virtual ~NimBLEScanCallbacks()=default;virtual void onResult(const NimBLEAdvertisedDevice*) {} };
class NimBLEClient;
class NimBLEClientCallbacks { public: virtual ~NimBLEClientCallbacks()=default;virtual void onDisconnect(NimBLEClient*,int) {} };
namespace ble_host {
 inline int connects=0,disconnects=0,writes=0,scanStops=0;
 inline bool connected=false;
}
class NimBLERemoteCharacteristic {
public:
 using Notification=std::function<void(NimBLERemoteCharacteristic*,uint8_t*,size_t,bool)>;
 bool canNotify() const { return true; }
 bool canWrite() const { return true; }
 bool canWriteNoResponse() const { return true; }
 bool subscribe(bool,Notification callback) { notification=std::move(callback);return true; }
 bool writeValue(const uint8_t*,size_t,bool) { ++ble_host::writes;return ble_host::connected; }
 void notify(uint8_t value) { if(notification)notification(this,&value,1,true); }
 Notification notification;
};
class NimBLERemoteService {
public:
 NimBLERemoteCharacteristic* getCharacteristic(NimBLEUUID) { return &characteristic; }
 NimBLERemoteCharacteristic characteristic;
};
class NimBLEClient {
public:
 void setClientCallbacks(NimBLEClientCallbacks* value,bool) { callbacks=value; }
 void setConnectTimeout(int) {}
 void setConnectRetries(int) {}
 bool connect(NimBLEAddress) { ++ble_host::connects;ble_host::connected=true;return true; }
 NimBLERemoteService* getService(NimBLEUUID) { return &service; }
 void cancelConnect() { if(ble_host::cancelConnect)ble_host::cancelConnect(); }
 bool isConnected() const { return ble_host::connected; }
 void disconnect() { ++ble_host::disconnects;ble_host::connected=false;if(callbacks)callbacks->onDisconnect(this,0); }
 NimBLEClientCallbacks* callbacks=nullptr;NimBLERemoteService service;
};
class NimBLEScan {
public:
 void setScanCallbacks(NimBLEScanCallbacks*,bool) {}
 void setActiveScan(bool) {}
 void setMaxResults(int) {}
 void start(int,bool,bool) {}
 void stop() { ++ble_host::scanStops; }
};
class NimBLEDevice {
public:
 static void init(const char*) {}
 static NimBLEScan* getScan() { return &scan; }
 static NimBLEClient* createClient() { return &client; }
 inline static NimBLEScan scan;inline static NimBLEClient client;
};
