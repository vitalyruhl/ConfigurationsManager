#ifndef RS485_MODULE_H
#define RS485_MODULE_H

#pragma once

#include <Arduino.h>
#include "ConfigManager.h"

struct RS485_Settings
{
    // Serial2 is used for RS485 communication
    static constexpr bool useExtraSerial = true;

    Config<bool> enableRS485{ConfigOptions<bool>{.key = "RS485En", .name = "Enable RS485", .category = "RS485", .defaultValue = true, .sortOrder = 1}};
    Config<int> baudRate{ConfigOptions<int>{.key = "RS485Baud", .name = "Baud Rate", .category = "RS485", .defaultValue = 4800, .sortOrder = 2}};
    Config<int> rxPin{ConfigOptions<int>{.key = "RS485Rx", .name = "RX Pin", .category = "RS485", .defaultValue = 18, .sortOrder = 3}};
    Config<int> txPin{ConfigOptions<int>{.key = "RS485Tx", .name = "TX Pin", .category = "RS485", .defaultValue = 19, .sortOrder = 4}};
    Config<int> dePin{ConfigOptions<int>{.key = "RS485DE", .name = "DE Pin", .category = "RS485", .defaultValue = 4, .sortOrder = 5}};

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&enableRS485);
        cfg.addSetting(&baudRate);
        cfg.addSetting(&rxPin);
        cfg.addSetting(&txPin);
        cfg.addSetting(&dePin);
    }
};

struct RS485Packet // 19.04.2025 viru - that will not working???
{
    uint16_t header = 0x2456;
    uint16_t command = 0x0021;
    uint16_t power = 0;
    uint8_t checksum = 0;
};


// -- Serial data --
// 19.04.2025 viru - packet will not working??? --> get this from internet
extern byte byte0;
extern byte byte1;
extern byte byte2;
extern byte byte3;
extern byte byte4;
extern byte byte5;
extern byte byte6;
extern byte byte7;

extern byte serialpacket[8];
extern HardwareSerial *RS485serial;
extern RS485Packet packet;
extern RS485_Settings rs485settings;

void RS485begin();
void sendToRS485(uint16_t demand);
void sendToRS485Packet(uint16_t demand);
String reciveFromRS485();
RS485Packet reciveFromRS485Packet();

#endif // RS485_MODULE_H
