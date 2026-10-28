$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir = Join-Path $repo ('.local/ota-http-reboot-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outDir | Out-Null
try {
  $driver = [IO.File]::ReadAllText((Join-Path $repo 'src/ota/OTAManager.cpp'))
  $methods = @()
  foreach ($name in @('handle','sendHttpSuccess','handleOTAUpload','setCallbacks')) {
    $method = [regex]::Match($driver, ('(?ms)^void ConfigManagerOTA::' + $name + '\([^)]*\) \{.*?^\}'))
    if (!$method.Success) { throw "Production OTA method missing: $name" }
    $methods += $method.Value
  }
  [IO.File]::WriteAllText((Join-Path $outDir 'ota-http-reboot-methods.inc'), ($methods -join "`n"))
  $config = [IO.File]::ReadAllText((Join-Path $repo 'src/ConfigManager.h'))
  $client = [regex]::Match($config, '(?ms)^  void handleClient\(\) \{.*?^  \}')
  if (!$client.Success -or $client.Value -notmatch 'handleOTA\(\)') { throw 'Application-loop OTA servicing contract missing.' }
  $otaWrapper = [regex]::Match($config, '(?ms)^  void handleOTA\(\) \{.*?^  \}')
  if (!$otaWrapper.Success -or $otaWrapper.Value -notmatch 'otaManager.handle\(\)') { throw 'OTA wrapper servicing contract missing.' }
  $compiler = 'C:/Program Files/LLVM/bin/clang++.exe'
  foreach ($name in @('ota_http_reboot_test','http_ota_reboot_request_test','http_reboot_preparation_test','ota_lifecycle_test')) {
    $exe = Join-Path $outDir ($name + '.exe')
    & $compiler -std=c++17 -Wall -Wextra -Werror -I $outDir -I (Join-Path $repo 'src') (Join-Path $repo ('test/host/' + $name + '.cpp')) -o $exe
    if ($LASTEXITCODE -ne 0) { throw "OTA host compilation failed: $name" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "OTA host checks failed: $name" }
  }
  & $compiler -std=c++17 -Wall -Wextra -Werror -DCM_ENABLE_OTA=0 -fsyntax-only -I (Join-Path $repo 'test/host/http_stubs') -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/ota_disabled_compile_test.cpp')
  if ($LASTEXITCODE -ne 0) { throw 'Disabled OTA header compile failed.' }
  $disabled = Join-Path $outDir 'ota-disabled-atomic.exe'
  & $compiler -std=c++17 -Wall -Wextra -Werror -DCM_ENABLE_OTA=0 -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/http_ota_reboot_request_test.cpp') -o $disabled
  if ($LASTEXITCODE -ne 0) { throw 'Disabled OTA atomic helper link failed.' }
  & $disabled
  if ($LASTEXITCODE -ne 0) { throw 'Disabled OTA atomic helper checks failed.' }
  $marked = Join-Path $outDir 'ota-reboot-markers.exe'
  & $compiler -std=c++17 -Wall -Wextra -Werror -DCM_TEST_RESTART_PHASES=1 -I $outDir -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/ota_http_reboot_test.cpp') -o $marked
  if ($LASTEXITCODE -ne 0) { throw 'OTA phase marker compile failed.' }
  & $marked
  if ($LASTEXITCODE -ne 0) { throw 'OTA phase marker assertions failed.' }
  Write-Output 'Disabled OTA header/atomic helper and optional phase markers passed.'
} finally {
  $resolved = (Resolve-Path -LiteralPath $outDir).Path
  $expected = [IO.Path]::GetFullPath((Join-Path $repo '.local'))
  if ([IO.Path]::GetDirectoryName($resolved) -ne $expected) { throw 'Refusing cleanup outside exact local output directory.' }
  Remove-Item -LiteralPath $resolved -Recurse -Force
}
