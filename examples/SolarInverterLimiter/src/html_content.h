#pragma once
#include <pgmspace.h>

// Minimal stub when embedded WebUI is disabled
const char WEB_HTML[] PROGMEM =
R"HTML(<!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>WebUI disabled</title><style>body{font-family:sans-serif;background:#111;color:#ddd;margin:2rem} .note{padding:1rem;border:1px solid #333;border-radius:8px;background:#1a1a1a}</style></head><body><h3>Embedded WebUI disabled</h3><div class="note">Use external WebUI. APIs are still available on this device.</div></body></html>)HTML";
