param([Alias('InitFailureOnly')][switch]$ModeFailureOnly)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir = Join-Path $repo ('.local/wifi-stack-recovery-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outDir | Out-Null
try {
    $driver = [IO.File]::ReadAllText((Join-Path $repo 'src/wifi/WiFiManager.cpp'))
    $methods = @()
    foreach ($name in @('startStackReset_', 'advanceStackReset_')) {
        $method = [regex]::Match($driver, ('(?ms)^(?:void|bool) ConfigManagerWiFi::' + $name + '\(\) \{.*?^\}'))
        if (!$method.Success) { throw "Production stack recovery method missing: $name" }
        $methods += $method.Value
    }
    [IO.File]::WriteAllText((Join-Path $outDir 'wifi-stack-recovery.inc'), ($methods -join "`n"))
    $framework = Join-Path $env:USERPROFILE '.platformio/packages/framework-arduinoespressif32/libraries/WiFi/src/WiFiGeneric.cpp'
    $arduino = [IO.File]::ReadAllText($framework)
    $bootPolicy = [regex]::Match($arduino, '(?ms)^\s*if\(!WiFiGenericClass::useStaticBuffers\(\)\)\s*\{.*?^\s*\}')
    if (!$bootPolicy.Success) { throw 'Installed Arduino boot buffer policy missing.' }
    $reference = 'static wifi_init_config_t arduinoBootBufferConfig() { wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();' + $bootPolicy.Value + "`nreturn cfg; }"
    [IO.File]::WriteAllText((Join-Path $outDir 'arduino-boot-buffer-policy.inc'), $reference)
    $exe = Join-Path $outDir 'wifi-stack-recovery.exe'
    & 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -I $outDir (Join-Path $repo 'test/host/wifi_stack_recovery_test.cpp') -o $exe
    if ($LASTEXITCODE -ne 0) { throw 'Stack recovery host compilation failed.' }
    if ($ModeFailureOnly) { & $exe 'mode-failure-only' } else { & $exe }
    if ($LASTEXITCODE -ne 0) { throw 'Stack recovery host checks failed.' }
} finally {
    $resolved = (Resolve-Path -LiteralPath $outDir).Path
    $expected = [IO.Path]::GetFullPath((Join-Path $repo '.local'))
    if ([IO.Path]::GetDirectoryName($resolved) -ne $expected) { throw 'Refusing cleanup outside exact local output directory.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
