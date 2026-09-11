@echo off
setlocal
REM ============================================================
REM  M3 (DEV, se corre UNA VEZ por version del juego):
REM  Genera la "release version" del juego BASE. El cook de DLC (mapas de mod) la usa como referencia
REM  para EXCLUIR el contenido que ya viene en el juego (asi el .pak del mod trae SOLO el mapa nuevo).
REM  Salida: <Project>\Releases\<VER>\Windows\...  -> se incluye en el modding kit para los creadores.
REM
REM  OJO: PRIMERA VERSION, sin validar. Puede requerir ajustar flags segun tu setup.
REM ============================================================
set PROJECT_DIR=%~dp0
set PROJECT_FILE=%PROJECT_DIR%MyPartyGame.uproject
set UAT=%PROGRAMFILES%\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat
set RELVER=Sculpturillo1

echo.
echo === Generando release version base "%RELVER%" (para el cook de mapas-mod) ===
echo.

call "%UAT%" BuildCookRun ^
    -project="%PROJECT_FILE%" ^
    -noP4 -platform=Win64 -clientconfig=Development ^
    -cook -stage -pak ^
    -CreateReleaseVersion=%RELVER%

if errorlevel 1 ( echo [ERROR] Fallo la generacion de la release version. & pause & exit /b 1 )
echo.
echo Listo. Release version en: %PROJECT_DIR%Releases\%RELVER%
echo Incluir esa carpeta en el modding kit (los creadores la necesitan para cocinar su mapa).
pause
