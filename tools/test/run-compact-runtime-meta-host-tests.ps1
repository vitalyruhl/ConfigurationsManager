$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir = Join-Path $repo '.local/compact-runtime-metadata'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exe = Join-Path $outDir 'compact-runtime-meta-store-test.exe'
& 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -Wno-deprecated-declarations -I (Join-Path $repo 'src') -I (Join-Path $repo '.pio/libdeps/usb/ArduinoJson/src') (Join-Path $repo 'test/host/compact_runtime_meta_store_test.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Compact runtime metadata host compile failed.' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Compact runtime metadata host assertions failed.' }
