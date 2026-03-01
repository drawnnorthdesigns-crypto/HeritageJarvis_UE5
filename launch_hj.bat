@echo off
setlocal enabledelayedexpansion
title Heritage Jarvis
color 0A

echo.
echo   ================================================
echo    HERITAGE JARVIS — Starting...
echo   ================================================
echo.

:: --- Locate Python ---
where python >nul 2>&1
if errorlevel 1 (
    echo   [ERROR] Python not found. Install Python 3.10+
    pause
    exit /b 1
)

:: --- Activate venv if present ---
set "FLASK_DIR=C:\HeritageJarvis_Project - Copy"

:: --- Suppress browser auto-open (Flask opens one by default) ---
set "HJ_NO_BROWSER=1"

:: --- Kill any stale Flask on port 5000 ---
for /f "tokens=5" %%p in ('netstat -aon ^| findstr ":5000.*LISTENING" 2^>nul') do (
    taskkill /PID %%p /F >nul 2>&1
    timeout /t 1 >nul
)

:: --- Start Flask backend (hidden window) ---
echo   [1/3] Starting Flask backend...
if exist "%FLASK_DIR%\.venv\Scripts\activate.bat" (
    start "HeritageJarvis-Flask" /min cmd /c "set HJ_NO_BROWSER=1 && cd /d "%FLASK_DIR%" && call .venv\Scripts\activate.bat && python -m heritage_jarvis web"
) else (
    start "HeritageJarvis-Flask" /min cmd /c "set HJ_NO_BROWSER=1 && cd /d "%FLASK_DIR%" && python -m heritage_jarvis web"
)

:: --- Wait for Flask ---
echo   [2/3] Waiting for Flask...
set /a attempts=0
:WAIT_LOOP
timeout /t 2 /nobreak >nul
curl -s http://127.0.0.1:5000/health >nul 2>&1
if %errorlevel% == 0 goto FLASK_READY
set /a attempts+=1
if %attempts% geq 15 (
    echo          Flask did not respond after 30s, launching anyway...
    goto LAUNCH_GAME
)
echo          Waiting... (%attempts%/15)
goto WAIT_LOOP

:FLASK_READY
echo          Flask is online.

:LAUNCH_GAME
:: --- Detect UE5 ---
echo   [3/3] Launching Heritage Jarvis...

set "PROJECT_PATH=C:\HeritageJarvis_UE5\HeritageJarvis.uproject"
set "UE5_PATH="

for %%V in (5.7 5.6 5.5 5.4) do (
    if not defined UE5_PATH (
        if exist "C:\Program Files\Epic Games\UE_%%V\Engine\Binaries\Win64\UnrealEditor.exe" (
            set "UE5_PATH=C:\Program Files\Epic Games\UE_%%V\Engine\Binaries\Win64\UnrealEditor.exe"
        )
    )
)

if not defined UE5_PATH (
    echo.
    echo   [ERROR] Unreal Engine not found.
    pause
    exit /b 1
)

:: --- Launch UE5 in GAME MODE (no editor) ---
:: Force map AND game mode via URL — overrides per-map WorldSettings and DefaultGame.ini
start "" "%UE5_PATH%" "%PROJECT_PATH%" /Game/Maps/MainMenu?game=/Script/HeritageJarvis.HJMainGameMode -game -windowed -ResX=1920 -ResY=1080

echo.
echo   ================================================
echo    Heritage Jarvis is running!
echo.
echo    Close this window when you're done playing.
echo    (Flask backend will stop automatically)
echo   ================================================
echo.
pause
:: When user closes this window, kill Flask
taskkill /FI "WINDOWTITLE eq HeritageJarvis-Flask" /F >nul 2>&1
