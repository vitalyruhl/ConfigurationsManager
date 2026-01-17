#pragma once
#include <Arduino.h>
#include "settings.h"

namespace Relays {

inline bool isValidPin(int pin){
    return pin >= 0 && pin <= 39; // basic range check for ESP32 GPIO
}

inline void initPins(){
    int fanPin = fanSettings.relayPin.get();
    int heaterPin = heaterSettings.relayPin.get();
    bool fanLow = fanSettings.activeLow.get();
    bool heaterLow = heaterSettings.activeLow.get();

    if(isValidPin(fanPin)){
        digitalWrite(fanPin, fanLow ? HIGH : LOW); // inactive
        pinMode(fanPin, OUTPUT);
    }
    if(isValidPin(heaterPin)){
        // initialize to inactive (OFF) respecting polarity
        digitalWrite(heaterPin, heaterLow ? HIGH : LOW);
        pinMode(heaterPin, OUTPUT);
    }
}

inline void setVentilator(bool on){
    int fanPin = fanSettings.relayPin.get();
    if(!isValidPin(fanPin)) return;
    bool fanLow = fanSettings.activeLow.get();
    if(fanLow){
        digitalWrite(fanPin, on ? LOW : HIGH);
    } else {
        digitalWrite(fanPin, on ? HIGH : LOW);
    }
}

inline bool getVentilator(){
    int fanPin = fanSettings.relayPin.get();
    if(!isValidPin(fanPin)) return false;
    bool fanLow = fanSettings.activeLow.get();
    int val = digitalRead(fanPin);
    return fanLow ? (val == LOW) : (val == HIGH);
}

inline void setHeater(bool on){
    int heaterPin = heaterSettings.relayPin.get();
    if(!heaterSettings.enabled.get() || !isValidPin(heaterPin)) return;
    bool heaterLow = heaterSettings.activeLow.get();
    if(heaterLow){
        digitalWrite(heaterPin, on ? LOW : HIGH);
    } else {
        digitalWrite(heaterPin, on ? HIGH : LOW);
    }
}

inline bool getHeater(){
    int heaterPin = heaterSettings.relayPin.get();
    if(!heaterSettings.enabled.get() || !isValidPin(heaterPin)) return false;
    bool heaterLow = heaterSettings.activeLow.get();
    int val = digitalRead(heaterPin);
    return heaterLow ? (val == LOW) : (val == HIGH);
}

} // namespace Relays
