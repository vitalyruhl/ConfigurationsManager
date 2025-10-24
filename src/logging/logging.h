#ifndef LOGGING_H
#define LOGGING_H

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <stdarg.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>
#include <queue>
#include <Ticker.h>
#include "SigmaLoger.h"

// Declare extern variables
extern Adafruit_SSD1306 display;
extern SigmaLoger *sl;
extern SigmaLoger *sll;
extern SigmaLogLevel currentLogLevel;

// Function declarations
void LoggerSetupSerial();
void SetupStartDisplay();
const char *sl_timestamp();
void SerialLoggerPublisher(SigmaLogLevel messageLevel, const char *message);
void LCDLoggerPublisher(SigmaLogLevel messageLevel, const char *message);
void LCDLoggerPublisherBuffered(SigmaLogLevel messageLevel, const char *message);
void LCDUpdate();
void splitIntoLines(const String& msg, size_t lineLength, std::vector<String>& out);

#endif // LOGGING_H
