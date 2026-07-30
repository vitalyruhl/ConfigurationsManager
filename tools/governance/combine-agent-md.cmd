@echo off
setlocal EnableDelayedExpansion
set "script=%~dp0combine-agent-md.ps1"

where pwsh.exe >nul 2>&1
if errorlevel 1 (
    echo [E] PowerShell 7 or newer was not found. Install PowerShell 7 and run this starter again.
    set "exitCode=1"
) else (
    pwsh.exe -NoProfile -ExecutionPolicy Bypass -File "%script%" %*
    set "exitCode=%ERRORLEVEL%"
    if "!exitCode!"=="0" (
        echo [I] Agent governance generation completed successfully.
    ) else (
        echo [E] Agent governance generation failed with exit code !exitCode!.
    )
)

pause
exit /b !exitCode!
