$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir = Join-Path $repo '.local/host-tests'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
foreach ($markers in 0,1) {
  $exe=Join-Path $outDir ("bluetooth-restart-"+$markers+'.exe')
  & 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror "-DCM_TEST_RESTART_PHASES=$markers" -I (Join-Path $repo 'test/host/bluetooth_manager_stubs') -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/bluetooth_restart_shutdown_test.cpp') -o $exe
  if($LASTEXITCODE -ne 0){throw 'BLE restart host compile failed.'}
  foreach($scenario in @('success','paused','failure','native','host','cleanup','workerless','workerless-active','workerfailed','otherowner','workerless-native','workerless-host','idleworker-native','disabledworker')) {
    & $exe $scenario
    if($LASTEXITCODE -ne 0){throw "BLE restart failed: $scenario markers=$markers"}
  }
}
