@echo off
setlocal
echo ========================================================
echo Building GBA Hardware Check App for EZ-Flash Omega...
echo ========================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed! Check messages above.
    pause
    exit /b %ERRORLEVEL%
)
echo.
echo [DONE] Build successful!
echo You can copy gba_hw_check.gba to your EZ-Flash Omega micro-SD card.
echo ========================================================
pause
