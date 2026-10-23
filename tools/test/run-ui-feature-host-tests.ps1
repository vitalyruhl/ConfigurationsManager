param([string]$ArduinoJsonInclude = '', [string]$Compiler = 'C:/Program Files/LLVM/bin/clang++.exe')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$ArduinoJsonInclude) { $ArduinoJsonInclude = Join-Path $repo '.pio/libdeps/usb/ArduinoJson/src' }
$out = Join-Path $repo '.local/ui-feature-host'
New-Item -ItemType Directory -Force -Path $out | Out-Null
foreach ($settings in 0,1) {
    $exe = Join-Path $out "runtime-0$settings.exe"
    & $Compiler -std=c++17 -DCM_ENABLE_LIVE_UI=0 "-DCM_ENABLE_SETTINGS_UI=$settings" -include (Join-Path $repo 'test/host/CompactHostString.h') -I (Join-Path $repo 'test/host') -I (Join-Path $repo 'src') -I $ArduinoJsonInclude (Join-Path $repo 'test/host/ui_feature_runtime_test.cpp') (Join-Path $repo 'src/runtime/RuntimeManager.cpp') (Join-Path $repo 'src/runtime/RuntimeMetadata.cpp') -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Runtime compile failed in mode 0$settings." }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "Runtime contracts failed in mode 0$settings." }
}
& python (Join-Path $repo 'test/ui_feature_tools_test.py')
if ($LASTEXITCODE -ne 0) { throw 'Feature tooling contracts failed.' }
$otaExe = Join-Path $out 'ota-lifecycle.exe'
& $Compiler -std=c++17 -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/ota_lifecycle_test.cpp') -o $otaExe
if ($LASTEXITCODE -ne 0) { throw 'OTA lifecycle compile failed.' }
& $otaExe
if ($LASTEXITCODE -ne 0) { throw 'OTA lifecycle contracts failed.' }
$header = Join-Path $repo 'src/html_content.h'
$headerText = [IO.File]::ReadAllText($header)
$live = [regex]::Match($headerText, '#define CM_WEBUI_LIVE_UI ([01])').Groups[1].Value
$settings = [regex]::Match($headerText, '#define CM_WEBUI_SETTINGS_UI ([01])').Groups[1].Value
if (!$live -or !$settings) { throw 'Generated embedded UI feature markers missing.' }
$headerArgs = @('-std=c++17', '-fsyntax-only', '-x', 'c++', '-include', 'cstdint', '-include', 'cstddef', '-I', (Join-Path $repo 'test/host/http_stubs'), '-I', (Join-Path $repo 'src'), "-DCM_ENABLE_SETTINGS_UI=$settings", $header)
& $Compiler @headerArgs "-DCM_ENABLE_LIVE_UI=$live"
if ($LASTEXITCODE -ne 0) { throw 'Matching embedded UI flags failed.' }
$oppositeLive = 1 - [int]$live
$rejection = & $Compiler @headerArgs "-DCM_ENABLE_LIVE_UI=$oppositeLive" 2>&1
$rejected = $LASTEXITCODE -ne 0
$rejection | Set-Content (Join-Path $out 'header-mismatch.log')
if (!$rejected -or ($rejection -join ' ') -notmatch 'Embedded WebUI feature flags do not match this build') { throw 'Mismatched embedded UI was not rejected.' }
& $Compiler @headerArgs "-DCM_ENABLE_LIVE_UI=$oppositeLive" -DCM_EMBED_WEBUI=0
if ($LASTEXITCODE -ne 0) { throw 'Disabled embedding must not require matching UI flags.' }
Write-Output 'Embedded UI matching/mismatch/disabled-embedding contracts passed.'
