@echo off
setlocal enabledelayedexpansion
REM ============================================================
REM  M3 (CREADOR): cocina el mapa del plugin MapKit a un .pak de DLC y lo deja listo para el Workshop.
REM  Requisitos: tener el PROYECTO/kit + Unreal Engine 5.8 + la carpeta Releases\Sculpturillo1 (viene en el kit).
REM  Antes de correr: guarda tu .umap en  Plugins\MapKit\Content\  (ruta de paquete /MapKit/TuMapa).
REM
REM  OJO: PRIMERA VERSION, sin validar de punta a punta. Puede requerir ajustar flags/ruta de salida.
REM ============================================================
set PROJECT_DIR=%~dp0
set PROJECT_FILE=%PROJECT_DIR%MyPartyGame.uproject
set UAT=%PROGRAMFILES%\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat
set RELVER=Sculpturillo1
set OUT=%PROJECT_DIR%MapMods_Output

echo.
echo === Cocinando el mapa del plugin MapKit a .pak (DLC) ===
echo.

call "%UAT%" BuildCookRun ^
    -project="%PROJECT_FILE%" ^
    -noP4 -platform=Win64 -clientconfig=Development ^
    -cook -pak -stage ^
    -DLCName=MapKit ^
    -BasedOnReleaseVersion=%RELVER% ^
    -DLCIncludeEngineContent

if errorlevel 1 ( echo [ERROR] Fallo el cook del DLC. & pause & exit /b 1 )

REM Buscar el .pak generado (la ruta exacta puede variar segun version) y copiarlo a MapMods_Output.
if not exist "%OUT%" mkdir "%OUT%"
set FOUND=
for /f "delims=" %%F in ('dir /b /s "%PROJECT_DIR%Plugins\MapKit\Saved\*MapKit*.pak" 2^>nul') do set FOUND=%%F
if "!FOUND!"=="" for /f "delims=" %%F in ('dir /b /s "%PROJECT_DIR%Saved\*MapKit*.pak" 2^>nul') do set FOUND=%%F

if "!FOUND!"=="" (
    echo [AVISO] No encontre el .pak automaticamente. Buscalo bajo Plugins\MapKit\Saved o Saved\ y copialo como map.pak.
) else (
    copy /y "!FOUND!" "%OUT%\map.pak" >nul
    echo Copiado: "!FOUND!"  ->  "%OUT%\map.pak"
)

REM Plantilla de mod.json (editar MapName con el nombre real de tu .umap, y Title/Author).
if not exist "%OUT%\mod.json" (
> "%OUT%\mod.json" (
echo {
echo   "MapName": "/MapKit/TuMapa",
echo   "Title": "Mi Mapa",
echo   "Author": "TuNombre"
echo }
)
)

echo.
echo === Listo (revisar) ===
echo   %OUT%\map.pak   + editar   %OUT%\mod.json  (MapName = /MapKit/ + nombre de tu .umap)
echo   Para probar local: copiar ambos a  ^<Proyecto^>\MapMods\TuMod\  (map.pak + mod.json) y correr el juego empaquetado.
echo   Para publicar: subirlo por el Workshop (M4).
pause
