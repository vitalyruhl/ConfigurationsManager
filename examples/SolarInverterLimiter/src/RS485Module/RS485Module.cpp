#include "RS485Module/RS485Module.h"
#include "logging/logging.h"

byte byte0 = 36;
byte byte1 = 86;
byte byte2 = 0;
byte byte3 = 33;
byte byte4 = 0;
byte byte5 = 0;
byte byte6 = 128;
byte byte7 = 8;

byte serialpacket[8];
HardwareSerial *RS485serial = &Serial;
RS485Packet packet;

void RS485begin()
{
    sl->Printf("starting RS485Module:Init()").Debug();
    sl->Printf("rs485settings.enableRS485 = %d", rs485settings.enableRS485).Debug();

    if (rs485settings.enableRS485 == false)
    {
        sl->Printf("RS485Module: RS485 communication disabled.").Info();
        return;
    }

    serialpacket[0] = byte0;
    serialpacket[1] = byte1;
    serialpacket[2] = byte2;
    serialpacket[3] = byte3;
    serialpacket[4] = byte4;
    serialpacket[5] = byte5;
    serialpacket[6] = byte6;
    serialpacket[7] = byte7;

    if (rs485settings.useExtraSerial)
    {
        RS485serial = &Serial2;
        RS485serial->begin(rs485settings.baudRate, SERIAL_8N1, rs485settings.rxPin, rs485settings.txPin);
        pinMode(rs485settings.dePin, OUTPUT);
        digitalWrite(rs485settings.dePin, LOW);
    }
    else
    {
        RS485serial = &Serial; // Optional fallback
        RS485serial->begin(rs485settings.baudRate, SERIAL_8N1);
    }
}

void sendToRS485Packet(uint16_t demand)
{
    // todo:Fix it --> dont send the header, there is an logical error!!!
    // 21:00:FF:02:64:00 instead of
    // 24:56:00:21:02:FE:80:09
    packet.power = demand;
    sl->Printf("sendToRS485Packet::sendToRS485: %d", demand).Debug();

    if (rs485settings.enableRS485 == false)
    {
        sl->Printf("sendToRS485Packet: RS485 communication disabled.").Debug();
        sll->Printf("RS485 is disabled!").Debug();
        return;
    }

    uint16_t sum = (packet.header >> 8) + (packet.header & 0xFF) +
                   (packet.command >> 8) + (packet.command & 0xFF) +
                   (packet.power >> 8) + (packet.power & 0xFF);

    packet.checksum = 256 - (sum & 0xFF);

    digitalWrite(rs485settings.dePin, HIGH); // Aktivate send mode
    delayMicroseconds(100);
    RS485serial->write((uint8_t *)&packet, sizeof(packet));
    RS485serial->flush(); // wait for send to finish
    delayMicroseconds(100);
    digitalWrite(rs485settings.dePin, LOW); // aktivate recive mode

    sl->Printf("--> RS485: Headder:%04X, Command:%04X, Power:%04X, Checksum:%02X", packet.header, packet.command, packet.power, packet.checksum).Debug();
}

void sendToRS485(uint16_t demand)
{
    // sl->Printf("RS485Module::sendToRS485: %d", demand).Info();

    if (rs485settings.enableRS485 == false)
    {
        sl->Printf("sendToRS485: RS485 communication disabled.").Debug();
        sll->Printf("RS485 is disabled!").Debug();
        return;
    }

    // -- Compute serial packet to inverter (just the 3 bytes that change) --
    byte4 = int(demand / 256); // (2 byte watts as short integer xaxb)
    if (byte4 < 0 or byte4 > 256)
    {
        byte4 = 0;
    }
    byte5 = int(demand) - (byte4 * 256); // (2 byte watts as short integer xaxb)
    if (byte5 < 0 or byte5 > 256)
    {
        byte5 = 0;
    }
    byte7 = (264 - byte4 - byte5); // checksum calculation
    if (byte7 > 256)
    {
        byte7 = 8;
    }

    serialpacket[4] = byte4;
    serialpacket[5] = byte5;
    serialpacket[7] = byte7;

    digitalWrite(rs485settings.dePin, HIGH); // Aktivate send mode
    delayMicroseconds(100);
    RS485serial->write(serialpacket, 8);
    RS485serial->flush(); // wait for send to finish
    delayMicroseconds(100);
    digitalWrite(rs485settings.dePin, LOW); // aktivate recive mode

    // sl->Printf("--> RS485: Headder:%02X,%02X,%02X, Command:%02X, Power:%02X,%02X Byte6:%02X Checksum:%02X",
    //            serialpacket[0], serialpacket[1], serialpacket[2],
    //            serialpacket[3],
    //            serialpacket[4], serialpacket[5],
    //            serialpacket[6],
    //            serialpacket[7])
    //     .Debug();

    // sl->Printf("--> RS485: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
    //            serialpacket[0], serialpacket[1], serialpacket[2],
    //            serialpacket[3],
    //            serialpacket[4], serialpacket[5],
    //            serialpacket[6],
    //            serialpacket[7])
    //     .Info();
}

String reciveFromRS485()
{
    String recivedData;
    while (RS485serial->available())
    {
        uint8_t byte = RS485serial->read();
        char hexByte[4];
        snprintf(hexByte, sizeof(hexByte), "%02X ", byte);
        recivedData += hexByte;
    }
    // sl->Printf("RS485Module: Recived HEX: %s", recivedData.c_str()).Debug();
    return recivedData;
}

RS485Packet reciveFromRS485Packet()
{
    RS485Packet incoming;
    if (RS485serial->available() >= sizeof(RS485Packet))
    {
        RS485serial->readBytes((char *)&incoming, sizeof(RS485Packet));
    }
    return incoming;
}
