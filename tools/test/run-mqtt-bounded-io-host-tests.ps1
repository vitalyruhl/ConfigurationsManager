param([string]$PubSubSource = '', [switch]$RecoveryContract)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir = [IO.Path]::GetFullPath((Join-Path $repo ('.local/mqtt-bounded-io-host-' + [guid]::NewGuid().ToString('N'))))
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
try {
$compiler = 'C:/Program Files/LLVM/bin/clang++.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw 'Verified clang++ compiler is unavailable.' }
$exe = Join-Path $outDir 'mqtt_bounded_io_test.exe'
& $compiler -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'test/host/mqtt_stubs') -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/mqtt_bounded_io_test.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Bounded MQTT host compile failed.' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Bounded MQTT host assertions failed.' }
if (!$PubSubSource) {
    foreach ($candidate in @('.pio/libdeps/mqtt/PubSubClient/src', '.pio/ble-validation-default/libdeps/mqtt/PubSubClient/src', '.pio/libdeps/usb/PubSubClient/src')) {
        $candidatePath = Join-Path $repo $candidate
        if (Test-Path -LiteralPath (Join-Path $candidatePath 'PubSubClient.cpp')) { $PubSubSource = $candidatePath; break }
    }
}
if (!$PubSubSource -or !(Test-Path -LiteralPath (Join-Path $PubSubSource 'PubSubClient.cpp'))) {
    throw 'PubSubClient 2.8 source is unavailable; install the mqtt example dependencies first.'
}
$integrationExe = Join-Path $outDir 'mqtt_pubsub_integration_test.exe'
$upstreamObject = Join-Path $outDir 'PubSubClient.o'
# Suppress two upstream-only warnings without weakening checks for the CM adapter.
& $compiler -std=c++17 -Wall -Wextra -Werror -Wno-sign-compare -Wno-unused-parameter -DESP32 -I (Join-Path $repo 'test/host/mqtt_stubs') -I $PubSubSource -c (Join-Path $PubSubSource 'PubSubClient.cpp') -o $upstreamObject
if ($LASTEXITCODE -ne 0) { throw 'Upstream PubSubClient host compile failed.' }
& $compiler -std=c++17 -Wall -Wextra -Werror -DESP32 -I (Join-Path $repo 'test/host/mqtt_stubs') -I (Join-Path $repo 'src') -I $PubSubSource (Join-Path $repo 'test/host/mqtt_pubsub_integration_test.cpp') $upstreamObject -o $integrationExe
if ($LASTEXITCODE -ne 0) { throw 'PubSubClient bounded transport integration compile failed.' }
if ($RecoveryContract) { & $integrationExe --recovery-contract } else { & $integrationExe }
if ($LASTEXITCODE -ne 0) { throw 'PubSubClient bounded transport integration assertions failed.' }
} finally {
    $resolvedOutput = (Resolve-Path -LiteralPath $outDir).Path
    $expectedParent = [IO.Path]::GetFullPath((Join-Path $repo '.local'))
    if ($resolvedOutput -ne $outDir -or [IO.Path]::GetDirectoryName($resolvedOutput) -ne $expectedParent) {
        throw 'Refusing cleanup outside the exact MQTT host-test output directory.'
    }
    Remove-Item -LiteralPath $resolvedOutput -Recurse -Force
}
