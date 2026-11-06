$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$tempRoot = Join-Path $repo '.Temp'
$taskName = 'pulse-output-host-tests-' + [guid]::NewGuid().ToString('N')
$outDir = Join-Path $tempRoot $taskName
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$compiler = 'C:/Program Files/LLVM/bin/clang++.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw 'Verified clang++ compiler is unavailable.' }
$profiles = @(
  @{ Name = 'explicit'; Defines = @() },
  @{ Name = 'variant-rgb'; Defines = @('-DLED_BUILTIN=30', '-DEXPECTED_BUILTIN_PIN=30', '-DEXPECTED_BUILTIN_OFF=0') },
  @{ Name = 'board-override'; Defines = @('-DLED_BUILTIN=30', '-DCM_BUILTIN_LED_PIN=8', '-DCM_BUILTIN_LED_ACTIVE_LOW=1', '-DEXPECTED_BUILTIN_PIN=8', '-DEXPECTED_BUILTIN_OFF=1') }
)
try {
  foreach ($profile in $profiles) {
    $exe = Join-Path $outDir ($profile.Name + '.exe')
    & $compiler -std=c++17 -Wall -Wextra -Werror @($profile.Defines) -I (Join-Path $repo 'test/host/pulse_output_stubs') -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/pulse_output_test.cpp') (Join-Path $repo 'src/helpers/HelperModule.cpp') -o $exe
    if ($LASTEXITCODE -ne 0) { throw "PulseOutput host compile failed: $($profile.Name)" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "PulseOutput host assertions failed: $($profile.Name)" }
  }
} finally {
  $target = (Resolve-Path -LiteralPath $outDir).Path
  if ((Split-Path -Parent $target) -ne (Resolve-Path -LiteralPath $tempRoot).Path -or
      (Split-Path -Leaf $target) -ne $taskName) {
    throw 'Refusing cleanup outside the exact PulseOutput test directory.'
  }
  Remove-Item -LiteralPath $target -Recurse -Force
}
