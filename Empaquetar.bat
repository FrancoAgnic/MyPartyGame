@echo off
setlocal

set PROJECT_DIR=%~dp0
set PROJECT_FILE=%PROJECT_DIR%MyPartyGame.uproject
set PACKAGED_DIR=%PROJECT_DIR%Packaged
set UAT=%PROGRAMFILES%\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat
set ONEDRIVE_DEST=%ONEDRIVE%\Sculpturillo

echo.
echo ========================================
echo   Empaquetando Sculpturillo...
echo ========================================
echo.

call "%UAT%" BuildCookRun ^
    -project="%PROJECT_FILE%" ^
    -noP4 ^
    -platform=Win64 ^
    -clientconfig=Development ^
    -cook -build -stage -pak -archive ^
    -archivedirectory="%PACKAGED_DIR%"

if errorlevel 1 (
    echo.
    echo [ERROR] El empaquetado fallo. No se copio nada a OneDrive.
    pause
    exit /b 1
)

echo.
echo ========================================
echo   Copiando build a OneDrive...
echo ========================================
echo.

if not exist "%ONEDRIVE_DEST%" mkdir "%ONEDRIVE_DEST%"

robocopy "%PACKAGED_DIR%\Windows" "%ONEDRIVE_DEST%" /MIR /NFL /NDL /NJH /NJS /NC /NS

echo.
echo ========================================
echo   Listo! Build disponible en OneDrive:
echo   %ONEDRIVE_DEST%
echo ========================================
echo.
pause
