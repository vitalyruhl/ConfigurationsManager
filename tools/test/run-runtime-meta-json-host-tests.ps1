param([string]$ArduinoJsonInclude = '')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$ArduinoJsonInclude) { $ArduinoJsonInclude = Join-Path $repo '.pio/libdeps/usb/ArduinoJson/src' }
$outDir = Join-Path $repo '.local/hybrid-json-output'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exe = Join-Path $outDir 'runtime-meta-json-test.exe'
& 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -Wno-deprecated-declarations -I (Join-Path $repo 'src') -I $ArduinoJsonInclude (Join-Path $repo 'test/host/runtime_meta_json_test.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Runtime metadata JSON host compile failed.' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Runtime metadata JSON host assertions failed.' }
