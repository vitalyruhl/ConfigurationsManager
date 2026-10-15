param([ValidateSet('all','ota','bluetooth')][string]$Suite = 'all')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir = Join-Path $repo '.local/host-tests'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$compiler = 'C:/Program Files/LLVM/bin/clang++.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw 'Verified clang++ compiler is unavailable.' }
$suites = if ($Suite -eq 'all') { @('ota','bluetooth') } else { @($Suite) }
foreach ($item in $suites) {
  $source = if ($item -eq 'ota') { 'ota_lifecycle_test.cpp' } else { 'bluetooth_transport_test.cpp' }
  $exe = Join-Path $outDir ($item + '.exe')
  & $compiler -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'src') (Join-Path $repo ('test/host/' + $source)) -o $exe
  if ($LASTEXITCODE -ne 0) { throw "Host compile failed: $item" }
  & $exe
  if ($LASTEXITCODE -ne 0) { throw "Host assertions failed: $item" }
}
