// [MOCKED!] Existing HTTP shell; compile the real disabled OTA header.
class AsyncWebServer;
#include "ota/OTAManager.h"
static_assert(CM_ENABLE_OTA == 0, "This check requires disabled OTA");
