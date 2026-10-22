#pragma once
#include <cstdint>
#include <functional>
#include <utility>
// [MOCKED!] Configuration/UI dependencies; BluetoothManager and transport state remain real.
struct Preferences {};
struct JsonObject { struct Value { template<class T> void operator=(const T&) {} }; Value operator[](const char*) { return {}; } };
template<class T> struct ConfigOptions { const char* key; const char* name; const char* category; T defaultValue; int sortOrder; };
template<class T> class Config {
public:
 explicit Config(const ConfigOptions<T>& options):value_(options.defaultValue),key_(options.key) {}
 virtual ~Config()=default;
 virtual void load(Preferences&) {}
 T get() const { return value_; }
 const char* getKey() const { return key_; }
 void setCallback(std::function<void(T)> callback) { callback_=std::move(callback); }
 void set(T value) { value_=value;if(callback_)callback_(value); }
private:
 T value_;const char* key_;std::function<void(T)> callback_;
};
struct ConfigManagerClass {
 struct Runtime { template<class F> void addRuntimeProvider(const char*,F,int) {} } runtime;
 template<class T> void addSetting(T*) {}
 void addSettingsPage(const char*,int) {}
 void addSettingsCard(const char*,const char*,int) {}
 void addSettingsGroup(const char*,const char*,const char*,int) {}
 void addToSettingsGroup(const char*,const char*,const char*,const char*,int) {}
 Runtime& getRuntime() { return runtime; }
};
namespace ble_host {
 inline uint32_t now=0;inline int iteration=0;
 inline std::function<void()> watermark,delay,cancelConnect;
 inline void (*entry)(void*)=nullptr;inline void* entryValue=nullptr;
 struct StopWorker {};
}
inline uint32_t millis() { return ble_host::now; }
