#ifndef RS485_MODULE_H
#define RS485_MODULE_H

#pragma once

#include <Arduino.h>
#include "settings.h"

// RS485 configuration (default Settings)
// struct RS485_Settings
// {
//     bool useExtraSerial = true; // set to true to use Serial2 for RS485 communication
//     int baudRate = 4800;
//     int rxPin = 18;          // only for Serial2, not used for Serial
//     int txPin = 19;          // only for Serial2, not used for Serial
//     int dePin = 4;           // DE pin for RS485 communication (direction control)
//     bool enableRS485 = true; // set to false to disable RS485 communication
//     // todo: add settings for Inverter eg, headder, checksum, etc.
// };

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

void RS485begin();
void sendToRS485(uint16_t demand);
void sendToRS485Packet(uint16_t demand);
String reciveFromRS485();
RS485Packet reciveFromRS485Packet();

#endif // RS485_MODULE_H
