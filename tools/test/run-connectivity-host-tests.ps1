$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir = Join-Path $repo '.local/connectivity-host'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$compiler = 'C:/Program Files/LLVM/bin/clang++.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw 'Verified clang++ compiler is unavailable.' }
$source = [IO.File]::ReadAllText((Join-Path $repo 'src/wifi/WiFiManager.cpp'))
$methods = @('pauseConnectivityForSynchronousOta', 'updateConnectivity_', 'setConnectivityCheckTarget', 'checkAutoReboot', 'update')
$production = foreach ($name in $methods) {
 $method = [regex]::Match($source, ('(?s)(?:void|bool) ConfigManagerWiFi::' + $name + '\([^)]*\) \{.*?\n\}'))
 if (!$method.Success) { throw "Production connectivity method not found: $name" }
 $method.Value
}
[IO.File]::WriteAllText((Join-Path $outDir 'connectivity-manager.inc'), ($production -join "`n"))
$socket = [IO.File]::ReadAllText((Join-Path $repo 'src/wifi/ConnectivitySocket.cpp'))
$socket = [regex]::Replace($socket, '(?m)^#include [^\r\n]+\r?\n', '')
[IO.File]::WriteAllText((Join-Path $outDir 'connectivity-socket.inc'), $socket)
foreach ($test in @('connectivity_test.cpp','connectivity_legacy_grace_test.cpp','connectivity_socket_test.cpp')) {
 $exe = Join-Path $outDir ([IO.Path]::GetFileNameWithoutExtension($test) + '.exe')
 & $compiler -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'src') -I $outDir (Join-Path $repo ('test/host/' + $test)) -o $exe
 if ($LASTEXITCODE -ne 0) { throw "Connectivity host compile failed: $test" }
 & $exe
 if ($LASTEXITCODE -ne 0) { throw "Connectivity host assertions failed: $test" }
}
