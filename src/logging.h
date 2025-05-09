#ifndef LOGGING_H
#define LOGGING_H

#pragma once
#include <Arduino.h>
#include <SigmaLoger.h>


// Declare extern variables
extern SigmaLoger *sl;
extern SigmaLogLevel level;

// Function declarations
void SetupStartDisplay();
const char *sl_timestamp();
void SerialLoggerPublisher(SigmaLogLevel level, const char *message);

#endif // LOGGING_H
