#pragma once
#include "BluetoothDispatch.h"
namespace cm { namespace detail {
// Narrow adapter for Config<bool>: only own construction/load may read raw value.
// Ordinary callbacks, radio/application loops and runtime use the atomic snapshot.
template<class Setting,class Storage> class BluetoothEnabledSetting : public Setting {
public:
  template<class Options> BluetoothEnabledSetting(BluetoothDispatch& dispatch,const Options& options)
    : Setting(options),dispatch_(dispatch) {
    dispatch_.setEnabled(Setting::get());
    this->setCallback([this](bool value){dispatch_.setEnabled(value);});
  }
  void load(Storage& storage) override {
    Setting::load(storage);
    // Config::load writes directly without invoking its change callback.
    dispatch_.setEnabled(Setting::get());
  }
private:
  BluetoothDispatch& dispatch_;
};
}}
