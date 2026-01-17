#pragma once
#include <Arduino.h>
#include "settings.h"

namespace Relays {

inline bool isValidPin(int pin){
    return pin >= 0 && pin <= 39; // basic range check for ESP32 GPIO
}

inline void initPins(){
    int boilerPin = boilerSettings.relayPin.get();
    bool boilerLow = boilerSettings.activeLow.get();

    if(isValidPin(boilerPin)){
        digitalWrite(boilerPin, boilerLow ? HIGH : LOW); // inactive
        pinMode(boilerPin, OUTPUT);
    }
}

inline void setBoiler(bool on){
    int boilerPin = boilerSettings.relayPin.get();
    if(!isValidPin(boilerPin)) return;
    bool boilerLow = boilerSettings.activeLow.get();
    if(boilerLow){
        digitalWrite(boilerPin, on ? LOW : HIGH);
    } else {
        digitalWrite(boilerPin, on ? HIGH : LOW);
    }
}

inline bool getBoiler(){
    int boilerPin = boilerSettings.relayPin.get();
    if(!isValidPin(boilerPin)) return false;
    bool boilerLow = boilerSettings.activeLow.get();
    int val = digitalRead(boilerPin);
    return boilerLow ? (val == LOW) : (val == HIGH);
}


} // namespace Relays
