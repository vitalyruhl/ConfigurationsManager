param([string]$ArduinoJsonInclude = '')
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$ArduinoJsonInclude) {$ArduinoJsonInclude=Join-Path $repo '.pio/libdeps/usb/ArduinoJson/src'}
$out=Join-Path $repo '.local/compact-runtime-metadata'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$exe=Join-Path $out 'runtime-meta-compatibility.exe'
& 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -DCM_ENABLE_SYSTEM_PROVIDER=0 -Ddevelopment -DCM_RUNTIME_META_TEST_INSTRUMENTATION=1 -include (Join-Path $repo 'test/host/CompactHostString.h') -I (Join-Path $repo 'src') -I $ArduinoJsonInclude (Join-Path $repo 'test/host/runtime_meta_compatibility_test.cpp') (Join-Path $repo 'src/runtime/RuntimeMetadata.cpp') -o $exe
if($LASTEXITCODE -ne 0){throw 'Runtime metadata compatibility compile failed.'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Runtime metadata compatibility assertions failed.'}
