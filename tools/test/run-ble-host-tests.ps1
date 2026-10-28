param([ValidateSet('all','ota','bluetooth')][string]$Suite = 'all')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir = Join-Path $repo '.local/host-tests'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$compiler = 'C:/Program Files/LLVM/bin/clang++.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw 'Verified clang++ compiler is unavailable.' }
function Invoke-HostCheck([string]$Name, [string]$Source, [string[]]$Defines = @()) {
  $exe = Join-Path $outDir ($Name + '.exe')
  & $compiler -std=c++17 -Wall -Wextra -Werror @Defines -I (Join-Path $repo 'src') (Join-Path $repo ('test/host/' + $Source)) -o $exe
  if ($LASTEXITCODE -ne 0) { throw "Host compile failed: $Name" }
  & $exe
  if ($LASTEXITCODE -ne 0) { throw "Host assertions failed: $Name" }
}
$suites = if ($Suite -eq 'all') { @('ota','bluetooth') } else { @($Suite) }
foreach ($item in $suites) {
  $source = if ($item -eq 'ota') { 'ota_lifecycle_test.cpp' } else { 'bluetooth_transport_test.cpp' }
  Invoke-HostCheck $item $source
  if ($item -eq 'bluetooth') {
    Invoke-HostCheck 'bluetooth-command-fence' 'bluetooth_command_fence_test.cpp'
    Invoke-HostCheck 'bluetooth-memory' 'bluetooth_memory_test.cpp'
    Invoke-HostCheck 'bluetooth-manager-epoch' 'bluetooth_manager_epoch_test.cpp' @('-I', (Join-Path $repo 'test/host/bluetooth_manager_stubs'))
    foreach ($scenario in @('cleanup','timeout','pause')) {
      & (Join-Path $outDir 'bluetooth-manager-epoch.exe') $scenario
      if ($LASTEXITCODE -ne 0) { throw "Host assertions failed: bluetooth-manager-epoch $scenario" }
    }
    Invoke-HostCheck 'bluetooth-coexistence' 'bluetooth_coexistence_test.cpp' @('-DCM_ENABLE_WIFI=1', '-I', (Join-Path $repo 'test/host/bluetooth_manager_stubs'))
    & (Join-Path $outDir 'bluetooth-coexistence.exe') gatt
    if ($LASTEXITCODE -ne 0) { throw 'Host assertions failed: bluetooth-coexistence GATT' }
    Invoke-HostCheck 'bluetooth-tx9'  $source @('-DCM_BLE_TX_MAX_BYTES=9')
    Invoke-HostCheck 'bluetooth-memory-tx9' 'bluetooth_memory_test.cpp' @('-DCM_BLE_TX_MAX_BYTES=9')
  }
}
