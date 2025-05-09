#include <logging.h>

SigmaLoger *sl = new SigmaLoger(512, SerialLoggerPublisher, sl_timestamp);
SigmaLogLevel level = SIGMALOG_WARN;

const char *sl_timestamp() {
    static char timestamp[16];
    sprintf(timestamp, "{ts=%.3f} ::", millis() / 1000.0);
    return timestamp;
}

void SerialLoggerPublisher(SigmaLogLevel level, const char *message) {
    Serial.printf("MAIN: [%d] %s\r\n", level, message);
}