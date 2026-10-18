$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir = Join-Path $repo '.local/hybrid-json-output'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$compiler = 'C:/Program Files/LLVM/bin/clang++.exe'
foreach ($verbose in @(0,1)) {
  $exe = Join-Path $outDir "http-heap-$verbose.exe"
  & $compiler -std=c++17 -Wall -Wextra -Werror -DESP32 "-DCM_ENABLE_VERBOSE_LOGGING=$verbose" -I (Join-Path $repo 'test/host/heap_probe_stubs') -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/http_heap_diagnostics_test.cpp') -o $exe
  if ($LASTEXITCODE -ne 0) { throw "HTTP heap diagnostics compile failed (verbose=$verbose)." }
  & $exe
  if ($LASTEXITCODE -ne 0) { throw "HTTP heap diagnostics test failed (verbose=$verbose)." }
}
