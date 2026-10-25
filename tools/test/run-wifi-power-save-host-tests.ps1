$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir = Join-Path $repo ('.local/wifi-power-save-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
try {
$source = [IO.File]::ReadAllText((Join-Path $repo 'src/ConfigManager.h'))
$driver = [IO.File]::ReadAllText((Join-Path $repo 'src/wifi/WiFiManager.cpp'))
$header = [IO.File]::ReadAllText((Join-Path $repo 'src/wifi/WiFiManager.h'))
$core = [IO.File]::ReadAllText((Join-Path $repo 'src/core/CoreSettings.h'))
function Extract([string]$text, [string]$pattern) {
  $match = [regex]::Match($text, $pattern)
  if (!$match.Success) { throw "Production WiFi power-save contract missing: $pattern" }
  return $match.Value
}
$diag = Extract $header '(?ms)^struct WiFiPowerSaveDiagnostics \{.*?^};'
[IO.File]::WriteAllText((Join-Path $outDir 'wifi-power-diagnostics.inc'), $diag)
$methods = @()
foreach ($name in @('setPowerSave','requireBleCoexistence','retryBleCoexistence','applyPowerSave_','getPowerSaveDiagnostics')) {
  $methods += Extract $driver ('(?ms)^(?:void|WiFiPowerSaveDiagnostics) ConfigManagerWiFi::' + $name + '\([^)]*\)(?: const)? \{.*?^\}')
}
[IO.File]::WriteAllText((Join-Path $outDir 'wifi-power-driver.inc'), ($methods -join "`n"))
$load = Extract $source '(?ms)^  void load\(Preferences& prefs\) override \{.*?^  \}'
$parse = Extract $source '(?ms)^  bool fromJSON\(const JsonVariant& jsonValue\) override \{.*?^  \}'
[IO.File]::WriteAllText((Join-Path $outDir 'wifi-power-config.inc'), ($load + "`n" + $parse))
$setting = Extract $core '(?ms)^class WiFiPowerSaveSetting.*?^};'
[IO.File]::WriteAllText((Join-Path $outDir 'wifi-power-setting.inc'), ('namespace cm { namespace CoreCategories { inline constexpr char WiFi[] = "WiFi"; }' + $setting + '}'))
$api = Extract $source '(?ms)^  void setWiFiPowerSave\(bool enabled\) \{.*?^  \}'
foreach ($name in @('initializeWiFiHealthConfig','applyWiFiHealthConfig','requestWiFiBleCoexistence')) {
  $api += "`n" + (Extract $source ('(?ms)^  void ' + $name + '\(\) \{.*?^  \}'))
}
$api += "`n" + (Extract $source '(?ms)^  bool isWiFiBleCoexistenceReady\(\) const \{.*?^  \}')
[IO.File]::WriteAllText((Join-Path $outDir 'wifi-power-api.inc'), $api)
# Check actual lifecycle call sites as well as testing the unchanged extracted helper above.
if ([regex]::Matches($driver, 'applyPowerSave_\(\);').Count -ne 6 -or $driver.Contains('WiFi.setSleep(WIFI_PS_MIN_MODEM)')) { throw 'All connect/reset/retry paths must use the central sleep policy.' }
$reset = Extract $driver '(?ms)^void ConfigManagerWiFi::startStackReset_\(\) \{.*?^\}'
if ($reset.IndexOf('bleCoexistenceReady_ = false;') -lt 0 -or $reset.IndexOf('bleCoexistenceReady_ = false;') -gt $reset.IndexOf('WiFi.disconnect')) { throw 'Reset must invalidate readiness before touching the driver.' }
$init = Extract $source '(?ms)^  void initializeWiFiHealthConfig\(\) \{.*?^  \}'
if (!$init.Contains('WiFiPowerSave') -or !$init.Contains('applyWiFiHealthConfig();')) { throw 'Boot must load stored sleep and drain newer requests before connecting.' }
$jsonInclude = Join-Path $repo '.pio/ble-validation/libdeps/usb/ArduinoJson/src'
$compiler = 'C:/Program Files/LLVM/bin/clang++.exe'
foreach ($default in @(0,1)) {
  $exe = Join-Path $outDir "wifi-power-$default.exe"
  & $compiler -std=c++20 -Wall -Wextra -Werror "-DCM_WIFI_POWER_SAVE_DEFAULT=$default" "-DEXPECTED_DEFAULT=$default" -DCM_ENABLE_LIVE_UI=0 -I (Join-Path $repo 'src') -I $jsonInclude -I $outDir (Join-Path $repo 'test/host/wifi_power_save_test.cpp') -o $exe
  if ($LASTEXITCODE -ne 0) { throw 'Power-save host compilation failed.' }
  & $exe
  if ($LASTEXITCODE -ne 0) { throw 'Power-save host assertions failed.' }
}
$flagSource = Join-Path $outDir 'flag.cpp'
[IO.File]::WriteAllText($flagSource, '#include "ConfigManagerConfig.h"')
foreach ($invalid in @(-1,2)) {
  & $compiler -fsyntax-only "-DCM_WIFI_POWER_SAVE_DEFAULT=$invalid" -I (Join-Path $repo 'src') $flagSource 2> (Join-Path $outDir 'invalid.log')
  if ($LASTEXITCODE -eq 0) { throw 'Invalid power-save build flag was accepted.' }
  if (!(Select-String -LiteralPath (Join-Path $outDir 'invalid.log') -SimpleMatch 'CM_WIFI_POWER_SAVE_DEFAULT must be 0 or 1')) { throw 'Unexpected invalid-flag compile failure.' }
}
Write-Output 'WiFi power-save flags 0/1 and invalid values: passed.'
} finally {
  $resolved = (Resolve-Path -LiteralPath $outDir).Path
  $expected = [IO.Path]::GetFullPath((Join-Path $repo '.local'))
  if ([IO.Path]::GetDirectoryName($resolved) -ne $expected) { throw 'Refusing cleanup outside exact local output directory.' }
  Remove-Item -LiteralPath $resolved -Recurse -Force
}
# Expected invalid-flag compiler failures must not leak into the successful runner exit.
exit 0
