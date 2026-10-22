$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir = Join-Path $repo '.local/wifi-health-config'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $repo 'src/ConfigManager.h'))
$reset = [regex]::Match($source, '(?s)(\[this\]\(\) \{(?:(?!\[this\]).)*?\}),\s*// reset callback')
if (!$reset.Success) { throw 'Production reset callback not found.' }
$production = @('void resetDefaults() { auto callback = ' + $reset.Groups[1].Value + '; callback(); }')
foreach ($name in @('queueWiFiHealthSetting','queueWiFiHealthSettings','loadAll')) {
  $method = [regex]::Match($source, ('(?ms)^  void ' + $name + '\([^)]*\) \{.*?^  \}'))
  if (!$method.Success -and $name -ne 'queueWiFiHealthSettings') { throw "Production method not found: $name" }
  if ($method.Success) { $production += $method.Value }
}
[IO.File]::WriteAllText((Join-Path $outDir 'wifi-health-reset.inc'), ($production -join "`n"))
foreach ($test in @('wifi_health_config_test.cpp','wifi_health_reset_test.cpp')) {
  $exe = Join-Path $outDir ([IO.Path]::GetFileNameWithoutExtension($test) + '.exe')
  & 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'src') -I $outDir (Join-Path $repo ('test/host/' + $test)) -o $exe
  if ($LASTEXITCODE -ne 0) { throw "WiFi health config host compile failed: $test" }
  & $exe
  if ($LASTEXITCODE -ne 0) { throw "WiFi health config host checks failed: $test" }
}

$coreSource = [IO.File]::ReadAllText((Join-Path $repo 'src/core/CoreSettings.h'))
$coreClass = [regex]::Match($coreSource, '(?ms)^class WiFiRebootTimeoutSetting.*?^};')
$coreMember = [regex]::Match($coreSource, '(?m)^  (?:Config<int>|WiFiRebootTimeoutSetting) rebootTimeoutMin[^\r\n]*')
if (!$coreMember.Success) { throw 'Production WiFi timeout member not found.' }
$coreFixture = "namespace cm { namespace CoreCategories { inline constexpr char WiFi[] = `"WiFi`"; }`n" + $coreClass.Value + "`nstruct CoreWiFiTimeout {`n" + $coreMember.Value + "`n}; }"
[IO.File]::WriteAllText((Join-Path $outDir 'wifi-timeout-core.inc'), $coreFixture)
$baseParser = [regex]::Match($source, '(?ms)^  bool fromJSON\(const JsonVariant& jsonValue\) override \{.*?^  \}')
if (!$baseParser.Success) { throw 'Production Config::fromJSON not found.' }
[IO.File]::WriteAllText((Join-Path $outDir 'wifi-timeout-base.inc'), $baseParser.Value)
$jsonInclude = Join-Path $repo '.pio/ble-validation/libdeps/usb/ArduinoJson/src'
if (!(Test-Path -LiteralPath (Join-Path $jsonInclude 'ArduinoJson.h'))) { throw 'Existing ArduinoJson include is unavailable.' }
$exe = Join-Path $outDir 'wifi-timeout-setting.exe'
& 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++20 -Wall -Wextra -Werror -I $jsonInclude -I $outDir (Join-Path $repo 'test/host/wifi_timeout_setting_test.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'WiFi timeout production setting host compile failed.' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'WiFi timeout production setting host checks failed.' }