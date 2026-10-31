param([string]$ArduinoJsonInclude = '')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$ArduinoJsonInclude) { $ArduinoJsonInclude = Join-Path $repo '.local/demand-runtime/libdeps/usb/ArduinoJson/src' }
$outDir = Join-Path $repo '.local/demand-runtime'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$compiler = 'C:/Program Files/LLVM/bin/clang++.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw 'Verified clang++ compiler is unavailable.' }
# Exercise the actual CM heartbeat maintenance body without ESP32/network headers.
$cmSource = [IO.File]::ReadAllText((Join-Path $repo 'src/ConfigManager.h'))
$start = $cmSource.IndexOf('  void wsHeartbeatMaintenance() {')
if ($start -lt 0) { throw 'Production heartbeat method not found.' }
$brace = $cmSource.IndexOf('{', $start)
$depth = 1
$end = $brace + 1
while ($end -lt $cmSource.Length -and $depth -gt 0) {
    if ($cmSource[$end] -eq '{') { $depth++ }
    elseif ($cmSource[$end] -eq '}') { $depth-- }
    $end++
}
if ($depth -ne 0) { throw 'Production heartbeat method is incomplete.' }
[IO.File]::WriteAllText((Join-Path $outDir 'heartbeat-maintenance.inc'), $cmSource.Substring($start, $end - $start), [Text.UTF8Encoding]::new($false))
$runtimeHeader = [IO.File]::ReadAllText((Join-Path $repo 'src/runtime/RuntimeManager.h'))
$acquisition = [regex]::Match($runtimeHeader, '(?s)  std::unique_lock<std::recursive_mutex> acquireRuntimeGeneration\(bool wait = true\) \{.*?\n  \}')
if (!$acquisition.Success) { throw 'Production runtime generation acquisition not found.' }
[IO.File]::WriteAllText((Join-Path $outDir 'runtime-generation.inc'), $acquisition.Value, [Text.UTF8Encoding]::new($false))
$exe = Join-Path $outDir 'runtime-selection.exe'
& $compiler -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'src') -I $outDir -I $ArduinoJsonInclude (Join-Path $repo 'test/host/runtime_selection_test.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Runtime host compile failed.' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Runtime host assertions failed.' }

# Exercise allocation-sensitive serialization and the actual runtime route body.
$runtimeSource = [IO.File]::ReadAllText((Join-Path $repo 'src/runtime/RuntimeManager.cpp'))
$serialization = [regex]::Match($runtimeSource, '(?s)String ConfigManagerRuntime::runtimeValuesToJSON\(const cm::RuntimeSelection& selection\) \{.*?\n\}')
if (!$serialization.Success) { throw 'Production runtime serialization method not found.' }
[IO.File]::WriteAllText((Join-Path $outDir 'runtime-serialization.inc'), $serialization.Value, [Text.UTF8Encoding]::new($false))
$webSource = [IO.File]::ReadAllText((Join-Path $repo 'src/web/WebServer.cpp'))
$routeStart = $webSource.IndexOf('  server->on("/runtime.json",')
$routeEnd = $webSource.IndexOf('  // Runtime metadata endpoint', $routeStart)
if ($routeStart -lt 0 -or $routeEnd -lt 0) { throw 'Production runtime route not found.' }
[IO.File]::WriteAllText((Join-Path $outDir 'runtime-route.inc'), $webSource.Substring($routeStart, $routeEnd - $routeStart), [Text.UTF8Encoding]::new($false))
$serializationExe = Join-Path $outDir 'runtime-serialization.exe'
& $compiler -std=c++17 -fno-elide-constructors -Wall -Wextra -Werror -I $outDir (Join-Path $repo 'test/host/runtime_serialization_test.cpp') -o $serializationExe
if ($LASTEXITCODE -ne 0) { throw 'Runtime serialization host compile failed.' }
& $serializationExe
$serializationResult = $LASTEXITCODE
& $serializationExe route
if ($LASTEXITCODE -ne 0 -or $serializationResult -ne 0) { throw 'Runtime serialization/route assertions failed.' }

# Exercise the actual runtime push body against ACK-stalled public transport APIs.
$start = $cmSource.IndexOf('  bool pushRuntimeSnapshot() {')
$brace = $cmSource.IndexOf('{', $start)
$depth = 1
$end = $brace + 1
while ($end -lt $cmSource.Length -and $depth -gt 0) {
    if ($cmSource[$end] -eq '{') { $depth++ }
    elseif ($cmSource[$end] -eq '}') { $depth-- }
    $end++
}
if ($start -lt 0 -or $depth -ne 0) { throw 'Production runtime push method not found.' }
[IO.File]::WriteAllText((Join-Path $outDir 'runtime-push.inc'), $cmSource.Substring($start, $end - $start), [Text.UTF8Encoding]::new($false))
$backpressureExe = Join-Path $outDir 'runtime-backpressure.exe'
& $compiler -std=c++17 -Wall -Wextra -Werror -DCM_ENABLE_LIVE_UI=1 -I (Join-Path $repo 'test/host/ws_stubs') -I (Join-Path $repo 'src') -I $outDir -I $ArduinoJsonInclude (Join-Path $repo 'test/host/runtime_backpressure_test.cpp') -o $backpressureExe
if ($LASTEXITCODE -ne 0) { throw 'Runtime backpressure host compile failed.' }
& $backpressureExe
if ($LASTEXITCODE -ne 0) { throw 'Runtime backpressure assertions failed.' }

$connectionExe = Join-Path $outDir 'runtime-connection-delivery.exe'
& $compiler -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'test/host/ws_stubs') -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/runtime_connection_delivery_test.cpp') -o $connectionExe
if ($LASTEXITCODE -ne 0) { throw 'Runtime connection delivery compile failed.' }
& $connectionExe
if ($LASTEXITCODE -ne 0) { throw 'Runtime connection delivery assertions failed.' }

$callbackStart = $cmSource.IndexOf('  bool sendWebSocketText(AsyncWebSocketClient* client, const String& payload) {')
if ($callbackStart -lt 0) { throw 'Public callback sender not found.' }
$callbackBrace = $cmSource.IndexOf('{', $callbackStart)
$depth = 1
$end = $callbackBrace + 1
while ($end -lt $cmSource.Length -and $depth -gt 0) {
    if ($cmSource[$end] -eq '{') { $depth++ }
    elseif ($cmSource[$end] -eq '}') { $depth-- }
    $end++
}
if ($depth -ne 0) { throw 'Public callback sender is incomplete.' }
[IO.File]::WriteAllText((Join-Path $outDir 'callback-delivery.inc'), $cmSource.Substring($callbackStart, $end - $callbackStart), [Text.UTF8Encoding]::new($false))
$callbackExe = Join-Path $outDir 'runtime-callback-delivery.exe'
& $compiler -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'test/host/ws_stubs') -I $outDir (Join-Path $repo 'test/host/runtime_callback_delivery_test.cpp') -o $callbackExe
if ($LASTEXITCODE -ne 0) { throw 'Runtime callback delivery compile failed.' }
& $callbackExe
if ($LASTEXITCODE -ne 0) { throw 'Runtime callback delivery assertions failed.' }
