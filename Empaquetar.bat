@echo off
setlocal

set PROJECT_DIR=%~dp0
set PROJECT_FILE=%PROJECT_DIR%MyPartyGame.uproject
set PACKAGED_DIR=%PROJECT_DIR%Packaged
set UAT=%PROGRAMFILES%\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat
set ONEDRIVE_DEST=%ONEDRIVE%\Sculpturillo
set RAR=%PROGRAMFILES%\WinRAR\Rar.exe

REM La version sale de Config\DefaultGame.ini (ProjectVersion) para que el nombre del .rar
REM y el numero que muestra el menu no puedan quedar desincronizados.
set VERSION=
for /f "tokens=2 delims==" %%v in ('findstr /b "ProjectVersion=" "%PROJECT_DIR%Config\DefaultGame.ini"') do set VERSION=%%v
if "%VERSION%"=="" set VERSION=0.0.0
set RAR_NAME=Sculpturillo_v%VERSION%.rar
set RAR_LOCAL=%PACKAGED_DIR%\%RAR_NAME%

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
    echo [ERROR] El empaquetado fallo. No se comprimio ni se subio nada.
    pause
    exit /b 1
)

echo.
echo ========================================
echo   Comprimiendo build en %RAR_NAME% ...
echo ========================================
echo.

if not exist "%RAR%" (
    echo [ADVERTENCIA] No encontre WinRAR en "%RAR%".
    echo Como respaldo copio la carpeta suelta a OneDrive.
    if not exist "%ONEDRIVE_DEST%" mkdir "%ONEDRIVE_DEST%"
    robocopy "%PACKAGED_DIR%\Windows" "%ONEDRIVE_DEST%" /MIR /NFL /NDL /NJH /NJS /NC /NS
    goto :done
)

REM .rar limpio: borrar el anterior y comprimir el CONTENIDO de Packaged\Windows
REM (raiz del .rar = Engine\, MyPartyGame\, MyPartyGame.exe). -m1 = rapido (los .pak
REM ya vienen comprimidos con Oodle). Para achicarlo mas, agregar  -x*.pdb  y excluir
REM los Manifest_DebugFiles (el amigo no los necesita para jugar).
if exist "%RAR_LOCAL%" del /q "%RAR_LOCAL%"
pushd "%PACKAGED_DIR%\Windows"
"%RAR%" a -r -m1 -idq "%RAR_LOCAL%" "*"
set RAR_ERR=%errorlevel%
popd

if not "%RAR_ERR%"=="0" (
    echo.
    echo [ERROR] Fallo la compresion ^(codigo %RAR_ERR%^). No se subio nada.
    pause
    exit /b 1
)

echo.
echo ========================================
echo   Subiendo %RAR_NAME% a OneDrive...
echo ========================================
echo.

if not exist "%ONEDRIVE_DEST%" mkdir "%ONEDRIVE_DEST%"

REM Limpiar los archivos SUELTOS viejos que dejaba la copia anterior, para que OneDrive
REM sincronice solo el .rar y no las miles de archivos del cook.
if exist "%ONEDRIVE_DEST%\Engine"     rmdir /s /q "%ONEDRIVE_DEST%\Engine"
if exist "%ONEDRIVE_DEST%\MyPartyGame" rmdir /s /q "%ONEDRIVE_DEST%\MyPartyGame"
del /q "%ONEDRIVE_DEST%\MyPartyGame.exe" 2>nul
del /q "%ONEDRIVE_DEST%\Manifest_*"      2>nul
del /q "%ONEDRIVE_DEST%\NOTICES.txt"     2>nul

REM Las builds viejas SI se borran: el nombre ahora lleva la version, asi que si no se limpian
REM se van acumulando en OneDrive y el que la baja no sabe cual es la ultima.
del /q "%ONEDRIVE_DEST%\Sculpturillo*.rar" 2>nul

copy /y "%RAR_LOCAL%" "%ONEDRIVE_DEST%\%RAR_NAME%"

:done
echo.
echo ========================================
echo   Listo! Build comprimida en OneDrive:
echo   %ONEDRIVE_DEST%\%RAR_NAME%
echo ========================================
echo.
pause
