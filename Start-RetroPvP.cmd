@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\run-retroarch.ps1"
if errorlevel 1 (
    echo.
    echo RetroPvP could not be started. Review the error above.
    pause
)
