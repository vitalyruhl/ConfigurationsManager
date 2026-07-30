@echo off
setlocal EnableDelayedExpansion
set "script=%~dp0run-full-repository-gate.ps1"

where pwsh.exe >nul 2>&1
if errorlevel 1 (
    echo [E] PowerShell 7 or newer was not found. Install PowerShell 7 and run this starter again.
    set "exitCode=1"
) else (
    pwsh.exe -NoProfile -ExecutionPolicy Bypass -File "%script%" %*
    set "exitCode=!ERRORLEVEL!"
    if "!exitCode!"=="0" (
        echo [I] Full repository gate completed successfully.
    ) else (
        echo [E] Full repository gate failed with exit code !exitCode!.
    )
)

pause
exit /b !exitCode!
