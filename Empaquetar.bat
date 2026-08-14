@echo off
setlocal

REM ============================================================
REM  Empaqueta Sculpturillo y lo SUBE A STEAM por SteamPipe.
REM  - Cook/build/stage/pak/archive del proyecto.
REM  - Renombra el exe a Sculpturillo.exe.
REM  - Espeja el build al content del ContentBuilder (carpeta "Sculpturillo").
REM  - Pone la version (de DefaultGame.ini) en la desc del build de SteamPipe.
REM  - Sube con steamcmd (usa el login CACHEADO; no se guarda password aca).
REM ============================================================

set PROJECT_DIR=%~dp0
set PROJECT_FILE=%PROJECT_DIR%MyPartyGame.uproject
set PACKAGED_DIR=%PROJECT_DIR%Packaged
set UAT=%PROGRAMFILES%\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat

set SDK=C:\Users\franc\Documents\Steam-sdk\tools\ContentBuilder
set CONTENT=%SDK%\content\Sculpturillo
set STEAMCMD=%SDK%\builder\steamcmd.exe
set APPVDF=%SDK%\scripts\app_5114580.vdf
set STEAMUSER=francoagnic07

REM La version sale de Config\DefaultGame.ini (ProjectVersion): el juego y la desc de
REM SteamPipe quedan sincronizados sin tocarlos a mano.
set VERSION=
for /f "tokens=2 delims==" %%v in ('findstr /b "ProjectVersion=" "%PROJECT_DIR%Config\DefaultGame.ini"') do set VERSION=%%v
if "%VERSION%"=="" set VERSION=0.0.0

echo.
echo ========================================
echo   Empaquetando Sculpturillo v%VERSION% ...
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
    echo [ERROR] El empaquetado fallo. No se subio nada.
    pause
    exit /b 1
)

REM El exe se genera como MyPartyGame.exe (nombre del proyecto/target). Lo renombramos a
REM Sculpturillo.exe (el bootstrap sigue lanzando la carpeta MyPartyGame internamente).
if exist "%PACKAGED_DIR%\Windows\MyPartyGame.exe" ren "%PACKAGED_DIR%\Windows\MyPartyGame.exe" "Sculpturillo.exe"

echo.
echo ========================================
echo   Copiando build a la carpeta Sculpturillo del ContentBuilder ...
echo ========================================
echo.

if not exist "%CONTENT%" mkdir "%CONTENT%"
REM /MIR = espeja (borra lo viejo que ya no esta), sin loguear cada archivo.
robocopy "%PACKAGED_DIR%\Windows" "%CONTENT%" /MIR /NFL /NDL /NJH /NJS /NC /NS

REM Poner la version actual en la "desc" del build de SteamPipe (solo esa linea del VDF).
powershell -NoProfile -Command "(Get-Content -LiteralPath '%APPVDF%') -replace '\"desc\"\s+\"[^\"]*\"', ('\"desc\" \"Sculpturillo_v%VERSION%\"') | Set-Content -LiteralPath '%APPVDF%'"

echo.
echo ========================================
echo   Subiendo Sculpturillo v%VERSION% a Steam (SteamPipe) ...
echo ========================================
echo.

"%STEAMCMD%" +login %STEAMUSER% +run_app_build "%APPVDF%" +quit
if errorlevel 1 (
    echo.
    echo [ERROR] La subida a Steam fallo. Si pide password/Steam Guard, inicia sesion
    echo         una vez con: "%STEAMCMD%" +login %STEAMUSER%  (y despues corre esto de nuevo).
    pause
    exit /b 1
)

echo.
echo ========================================
echo   Listo! Sculpturillo v%VERSION% subido a Steam.
echo ========================================
echo.
pause
