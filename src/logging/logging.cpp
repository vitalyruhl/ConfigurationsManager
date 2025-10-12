#include "logging/logging.h"
#include "settings.h"
#include <Wire.h>

Adafruit_SSD1306 display(4);
SigmaLoger *sl = new SigmaLoger(512, SerialLoggerPublisher, sl_timestamp);
SigmaLoger *sll = new SigmaLoger(512, LCDLoggerPublisherBuffered, nullptr);
SigmaLogLevel level = SIGMALOG_WARN;

#include <Ticker.h>

Ticker lcdTicker;
String lcdLogBuffer; // buffer
bool lcdUpdatePending = false;
float lcdUpdateTime = 2.0; // time to show the message on the display
std::queue<std::vector<String>> lcdMessageQueue;
std::vector<String> lcdCurrentMessage;
size_t lcdCurrentLine = 0;

void cbMyConfigLogger(const char *msg)
{
    sl->Debug(msg);
}

void LoggerSetupSerial()
{
    Serial.begin(115200);
    while (!Serial)
    {
        delay(50); // wait for serial port to connect. Needed for native USB port only
    }
    sl->Debug("Serial started!");
    sll->Debug("LCD started!");
    ConfigManagerClass::setLogger(cbMyConfigLogger); // Set the logger for the ConfigManager
}

void SetupStartDisplay()
{
    display.begin(SSD1306_SWITCHCAPVCC, i2cSettings.displayAddr.get());
    display.clearDisplay();
    display.drawRect(0, 0, 128, 25, WHITE);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(10, 5);
    display.println("Starting!");
    display.display();
    lcdTicker.attach(lcdUpdateTime, LCDUpdate);
}

const char *sl_timestamp()
{
    static char timestamp[16];
    sprintf(timestamp, "{ts=%.3f} ::", millis() / 1000.0);
    return timestamp;
}

void SerialLoggerPublisher(SigmaLogLevel level, const char *message)
{
    Serial.printf("MAIN: [%d] %s\r\n", level, message);
}

void LCDLoggerPublisher(SigmaLogLevel level, const char *message)
{
    display.fillRect(0, 25, 128, 8, BLACK);
    display.setCursor(0, 25);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.print(message);
    display.display();
}

void LCDLoggerPublisherBuffered(SigmaLogLevel level, const char *message)
{
    // lcdLogBuffer = message;
    // lcdUpdatePending = true;

    std::vector<String> newMessage;
    splitIntoLines(String(message), 21, newMessage);
    lcdMessageQueue.push(newMessage);
}
void splitIntoLines(const String &msg, size_t lineLength, std::vector<String> &out)
{
    out.clear();
    for (size_t i = 0; i < msg.length(); i += lineLength)
    {
        out.push_back(msg.substring(i, i + lineLength));
    }
}

void LCDUpdate()
{
    if (lcdCurrentLine >= lcdCurrentMessage.size())
    {
        // aktuelle Nachricht fertig, nächste holen
        if (!lcdMessageQueue.empty())
        {
            lcdCurrentMessage = lcdMessageQueue.front();
            lcdMessageQueue.pop();
            lcdCurrentLine = 0;
        }
        else
        {
            return; // nichts zu tun
        }
    }

    // aktuelle Zeile anzeigen
    display.fillRect(0, 25, 128, 8, BLACK);
    display.setCursor(0, 25);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.print(lcdCurrentMessage[lcdCurrentLine]);
    display.display();

    lcdCurrentLine++;
}
