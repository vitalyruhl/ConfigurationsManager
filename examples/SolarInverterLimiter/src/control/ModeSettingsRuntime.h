#pragma once
#include "ConfigManager.h"
#include "ModeProfile.h"

namespace control {

// CM presentation adapters queue requests only. CM's synchronous Web-task save
// must never persist or activate an operating mode; the main loop owns both.
class ModeSettingsRuntime {
    enum class Field { Mode, Threshold, Share };
    struct Pending {
        bool mode=false,threshold=false,share=false;
        ModeSettings values;
        uint32_t expiresAt=0;
    } pending_;
    portMUX_TYPE pendingMux_=portMUX_INITIALIZER_UNLOCKED;
    SemaphoreHandle_t uiMutex_=nullptr;
    ModeTransaction transaction_;
    const char *storageReason_="startup";

    template<class T> class DeferredSetting : public Config<T> {
        ModeSettingsRuntime &owner_;
        Field field_;
    public:
        DeferredSetting(ModeSettingsRuntime &owner,Field field,const ConfigOptions<T> &options)
            :Config<T>(options),owner_(owner),field_(field) {}
        void load(Preferences &) override {} // One versioned profile is the sole storage source.
        void save(Preferences &) override {} // Never write from CM's Web task.
        bool fromJSON(const JsonVariant &value) override {
            if constexpr(std::is_same_v<T,String>) {
                if(!value.is<const char *>()) return false;
                OperatingMode mode;
                return parseOperatingMode(value.as<const char *>(),mode) && owner_.queueMode(mode);
            } else if constexpr(std::is_same_v<T,float>) {
                if(!value.is<float>()) return false;
                const float threshold=value.as<float>();
                if(!std::isfinite(threshold) || threshold<=0 || threshold>=100) return false;
                portENTER_CRITICAL(&owner_.pendingMux_);
                owner_.pending_.threshold=true; owner_.pending_.values.chargeThreshold=threshold;
                portEXIT_CRITICAL(&owner_.pendingMux_); return true;
            } else {
                if(!value.is<int>()) return false;
                const int share=value.as<int>();
                if(share<0 || share>100) return false;
                portENTER_CRITICAL(&owner_.pendingMux_);
                owner_.pending_.share=true; owner_.pending_.values.inverterSharePercent=share;
                portEXIT_CRITICAL(&owner_.pendingMux_); return true;
            }
        }
        void setDefault() override {
            if constexpr(std::is_same_v<T,String>) owner_.queueMode(OperatingMode::Automatic);
            else {
                portENTER_CRITICAL(&owner_.pendingMux_);
                if(field_==Field::Threshold) {owner_.pending_.threshold=true;owner_.pending_.values.chargeThreshold=95;}
                else {owner_.pending_.share=true;owner_.pending_.values.inverterSharePercent=0;}
                portEXIT_CRITICAL(&owner_.pendingMux_);
            }
        }
        void toJSON(JsonObject &object) const override {
            if(owner_.uiMutex_) xSemaphoreTake(owner_.uiMutex_,portMAX_DELAY);
            Config<T>::toJSON(object);
            if(owner_.uiMutex_) xSemaphoreGive(owner_.uiMutex_);
        }
        void confirm(const T &value) { Config<T>::set(value); this->modified=false; }
    };
    DeferredSetting<String> mode_{*this,Field::Mode,ConfigOptions<String>{.key="opMode",.name="Operating Mode (automatic / household_follow / charge_priority / pv_follow)",.category="Limiter",.defaultValue=String("automatic"),.sortOrder=30}};
    DeferredSetting<float> threshold_{*this,Field::Threshold,ConfigOptions<float>{.key="opChargeSoc",.name="Charge Priority Threshold (%)",.category="Limiter",.defaultValue=95.0f,.sortOrder=31}};
    DeferredSetting<int> share_{*this,Field::Share,ConfigOptions<int>{.key="opPvShare",.name="Charge Priority Inverter Share (%)",.category="Limiter",.defaultValue=0,.sortOrder=32}};
    void confirmUi() {
        if(uiMutex_) xSemaphoreTake(uiMutex_,portMAX_DELAY);
        const auto &cfg=transaction_.confirmed();
        mode_.confirm(String(operatingModeName(cfg.mode)));
        threshold_.confirm(cfg.chargeThreshold); share_.confirm(cfg.inverterSharePercent);
        if(uiMutex_) xSemaphoreGive(uiMutex_);
    }
    bool persist(const ModeSettings &settings) {
        Preferences prefs;
        if(!prefs.begin("SILModes",false)) return false;
        const bool ok=persistModeProfile(settings,
            [&](const uint8_t *data,size_t n) { return prefs.putBytes("staged",data,n); },
            [&](uint8_t *data,size_t n) { return prefs.getBytesLength("staged")==n ? prefs.getBytes("staged",data,n) : size_t(0); },
            [&](const uint8_t *data,size_t n) { return prefs.putBytes("profile",data,n); });
        prefs.end(); return ok;
    }
public:
    void attachTo(ConfigManagerClass &cfg) {cfg.addSetting(&mode_);cfg.addSetting(&threshold_);cfg.addSetting(&share_);}
    void begin() {
        if(!uiMutex_) uiMutex_=xSemaphoreCreateMutex();
        ModeSettings settings; uint8_t bytes[kModeProfileBytes]={}; Preferences prefs;
        storageReason_="profile_missing";
        if(prefs.begin("SILModes",true)) {
            const size_t n=prefs.getBytesLength("profile");
            if(n) {
                storageReason_="profile_invalid";
                if(n==sizeof(bytes) && prefs.getBytes("profile",bytes,n)==n && decodeModeProfile(bytes,n,settings))
                    storageReason_="ok";
            }
            prefs.end();
        }
        transaction_.initialize(settings); confirmUi();
    }
    const ModeSettings &confirmed() const {return transaction_.confirmed();}
    const char *storageReason() const {return storageReason_;}
    bool queueMode(OperatingMode mode,uint32_t expiresAt=0) {
        if(mode>OperatingMode::PvFollow) return false;
        portENTER_CRITICAL(&pendingMux_);
        pending_.mode=true;pending_.values.mode=mode;pending_.expiresAt=expiresAt;
        portEXIT_CRITICAL(&pendingMux_);return true;
    }
    void discardPendingMode() {
        portENTER_CRITICAL(&pendingMux_);
        if(pending_.expiresAt) pending_.mode=false; // Preserve independent UI requests.
        portEXIT_CRITICAL(&pendingMux_);
    }
    bool loop(uint32_t now) {
        Pending pending;
        portENTER_CRITICAL(&pendingMux_);pending=pending_;pending_=Pending{};portEXIT_CRITICAL(&pendingMux_);
        ModeSettings requested=confirmed();
        if(pending.mode && (!pending.expiresAt || (now>=1704067200UL && pending.expiresAt>now && pending.expiresAt-now<=60)))
            requested.mode=pending.values.mode;
        if(pending.threshold) requested.chargeThreshold=pending.values.chargeThreshold;
        if(pending.share) requested.inverterSharePercent=pending.values.inverterSharePercent;
        if(sameModeSettings(requested,confirmed())) return false;
        if(!transaction_.request(requested)) return false;
        const bool changed=transaction_.process([&](const ModeSettings &settings) {return persist(settings);});
        storageReason_=changed?"ok":"save_failed";
        if(changed) confirmUi();
        return changed;
    }
};
} // namespace control
