$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir = Join-Path $repo '.local/demand-runtime'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$compiler = 'C:/Program Files/LLVM/bin/clang++.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw 'Verified clang++ compiler is unavailable.' }
$exe = Join-Path $outDir 'bounded-json-response.exe'
& $compiler -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'test/host/http_stubs') -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/bounded_json_response_test.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Bounded JSON response host compile failed.' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Bounded JSON response assertions failed.' }
