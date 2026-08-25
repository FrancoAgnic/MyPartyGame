@echo off
setlocal

REM ============================================================
REM  Empaqueta Sculpturillo y lo sube al PLAYTEST (App 5115870, Depot 5115871).
REM  Igual que Empaquetar.bat pero apuntando a la app del Playtest.
REM  - NO escribe steam_appid.txt: al abrir por el Playtest, Steam pasa 5115870 solo.
REM  - setlive vacio: subir el build y luego dejarlo LIVE en la rama default del Playtest
REM    desde la web (SteamPipe -> Builds -> Set Build Live), porque steamcmd no puede el default.
REM ============================================================

set PROJECT_DIR=%~dp0
set PROJECT_FILE=%PROJECT_DIR%MyPartyGame.uproject
set PACKAGED_DIR=%PROJECT_DIR%Packaged
set UAT=%PROGRAMFILES%\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat

set SDK=C:\Users\franc\Documents\Steam-sdk\tools\ContentBuilder
set CONTENT=%SDK%\content\Sculpturillo_Playtest
set STEAMCMD=%SDK%\builder\steamcmd.exe
set APPVDF=%SDK%\scripts\app_5115870.vdf
set STEAMUSER=francoagnic07

set VERSION=
for /f "tokens=2 delims==" %%v in ('findstr /b "ProjectVersion=" "%PROJECT_DIR%Config\DefaultGame.ini"') do set VERSION=%%v
if "%VERSION%"=="" set VERSION=0.0.0

echo.
echo ========================================
echo   Empaquetando Sculpturillo (PLAYTEST) v%VERSION% ...
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

if exist "%PACKAGED_DIR%\Windows\Sculpturillo.exe" del /q "%PACKAGED_DIR%\Windows\Sculpturillo.exe"
if exist "%PACKAGED_DIR%\Windows\MyPartyGame.exe" ren "%PACKAGED_DIR%\Windows\MyPartyGame.exe" "Sculpturillo.exe"

REM NO steam_appid.txt en el build (que Steam pase el App ID del Playtest al lanzar).
if exist "%PACKAGED_DIR%\Windows\steam_appid.txt" del /q "%PACKAGED_DIR%\Windows\steam_appid.txt"
if exist "%PACKAGED_DIR%\Windows\MyPartyGame\Binaries\Win64\steam_appid.txt" del /q "%PACKAGED_DIR%\Windows\MyPartyGame\Binaries\Win64\steam_appid.txt"

REM Borrar Saved del build (ver nota en Empaquetar.bat).
if exist "%PACKAGED_DIR%\Windows\MyPartyGame\Saved" rmdir /s /q "%PACKAGED_DIR%\Windows\MyPartyGame\Saved"
if exist "%PACKAGED_DIR%\Windows\Engine\Saved"      rmdir /s /q "%PACKAGED_DIR%\Windows\Engine\Saved"

echo.
echo ========================================
echo   Copiando build a la carpeta Sculpturillo_Playtest del ContentBuilder ...
echo ========================================
echo.

if not exist "%CONTENT%" mkdir "%CONTENT%"
robocopy "%PACKAGED_DIR%\Windows" "%CONTENT%" /MIR /NFL /NDL /NJH /NJS /NC /NS

REM VDF del build del Playtest (setlive vacio: se deja live a mano desde la web).
> "%APPVDF%" (
echo "appbuild"
echo {
echo   "appid" "5115870"
echo   "desc" "Sculpturillo_Playtest_v%VERSION%"
echo   "buildoutput" "%SDK%\output"
echo   "contentroot" ""
echo   "setlive" ""
echo   "preview" "0"
echo   "local" ""
echo   "depots"
echo   {
echo     "5115871" "%SDK%\scripts\depot_5115871.vdf"
echo   }
echo }
)

echo.
echo ========================================
echo   Subiendo al PLAYTEST (5115870) v%VERSION% ...
echo ========================================
echo.

"%STEAMCMD%" +login %STEAMUSER% +run_app_build "%APPVDF%" +quit
if errorlevel 1 (
    echo.
    echo [ERROR] La subida a Steam fallo. Si pide password/Steam Guard, inicia sesion una
    echo         vez a mano con:  "%STEAMCMD%" +login %STEAMUSER%   y volve a correr esto.
    pause
    exit /b 1
)

echo.
echo ========================================
echo   Listo! Playtest v%VERSION% subido.
echo   Abriendo la pagina de Builds... solo falta: Set Build Live -^> rama default.
echo ========================================
echo.

REM Abrir la pagina de Builds del Playtest en el navegador (unico paso manual: Set Build Live -> default).
start "" "https://partner.steamgames.com/apps/builds/5115870"

pause
