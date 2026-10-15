param([string]$ArduinoJsonInclude = '')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$ArduinoJsonInclude) { $ArduinoJsonInclude = Join-Path $repo '.local/demand-runtime/libdeps/usb/ArduinoJson/src' }
$outDir = Join-Path $repo '.local/demand-runtime'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$compiler = 'C:/Program Files/LLVM/bin/clang++.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw 'Verified clang++ compiler is unavailable.' }
$exe = Join-Path $outDir 'checked-json.exe'
& $compiler -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'src') -I $ArduinoJsonInclude (Join-Path $repo 'test/host/checked_json_test.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Checked JSON host compile failed.' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Checked JSON host assertions failed.' }
