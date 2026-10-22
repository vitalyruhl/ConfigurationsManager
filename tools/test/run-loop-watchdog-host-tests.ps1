$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$parent=Join-Path $repo '.local/loop-watchdog-host'
New-Item -ItemType Directory -Force $parent | Out-Null
$out=Join-Path $parent ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $out | Out-Null
try {
 # [MOCKED!] Minimal public ESP-IDF timer/reset declarations; production adapter is unchanged.
 @'
#pragma once
#include <cstdint>
struct FakeTimer;
using esp_timer_handle_t=FakeTimer*;
constexpr int ESP_OK=0;
constexpr int ESP_TIMER_TASK=0;
struct esp_timer_create_args_t {void (*callback)(void*);void* arg;int dispatch_method;const char* name;bool skip_unhandled_events;};
int64_t esp_timer_get_time();
int esp_timer_create(const esp_timer_create_args_t*,esp_timer_handle_t*);
int esp_timer_start_periodic(esp_timer_handle_t,uint64_t);
int esp_timer_stop(esp_timer_handle_t);
int esp_timer_delete(esp_timer_handle_t);
'@ | Set-Content -Encoding utf8 (Join-Path $out 'esp_timer.h')
 @'
#pragma once
enum esp_reset_reason_t {ESP_RST_POWERON,ESP_RST_SW};
esp_reset_reason_t esp_reset_reason();
void esp_restart();
'@ | Set-Content -Encoding utf8 (Join-Path $out 'esp_system.h')
 '#define RTC_NOINIT_ATTR' | Set-Content -Encoding utf8 (Join-Path $out 'esp_attr.h')
 New-Item -ItemType Directory (Join-Path $out 'rom') | Out-Null
 'int ets_printf(const char*,...);' | Set-Content -Encoding utf8 (Join-Path $out 'rom/ets_sys.h')
 $exe=Join-Path $out 'loop-watchdog.exe'
 & 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -DESP32 -I $out -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/loop_watchdog_test.cpp') (Join-Path $repo 'src/core/LoopWatchdog.cpp') -o $exe
 if($LASTEXITCODE -ne 0){throw 'Loop watchdog host compile failed'}
 & $exe
 if($LASTEXITCODE -ne 0){throw 'Loop watchdog host assertions failed'}
} finally {
 $resolved=[IO.Path]::GetFullPath($out)
 if([IO.Path]::GetDirectoryName($resolved) -ne [IO.Path]::GetFullPath($parent)){throw 'Unexpected cleanup target'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
