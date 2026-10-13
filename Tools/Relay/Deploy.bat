@echo off
setlocal
cd /d "%~dp0"

echo ============================================================
echo   Deploy del relay de Sculpturillo (actualiza la pagina web
echo   que ven los celulares/chat en sculpturillo.svoltanic.com)
echo ============================================================
echo.
set /p IP=Pone la IP de tu servidor (VPS) y Enter:
if "%IP%"=="" ( echo. & echo Sin IP, cancelado. & pause & exit /b 1 )

echo.
echo Empaquetando y subiendo a %IP% ...
tar --exclude=node_modules --exclude=.env -czf "%TEMP%\relay.tgz" -C . .
if errorlevel 1 ( echo [ERROR] No se pudo empaquetar. & pause & exit /b 1 )

scp "%TEMP%\relay.tgz" root@%IP%:/tmp/relay.tgz
if errorlevel 1 ( echo [ERROR] No se pudo subir ^(revisa la IP / tu clave SSH^). & pause & exit /b 1 )

ssh root@%IP% "mkdir -p /opt/sculpturillo-relay && tar -xzf /tmp/relay.tgz -C /opt/sculpturillo-relay && rm /tmp/relay.tgz && cd /opt/sculpturillo-relay && docker compose up -d --build"
if errorlevel 1 ( echo [ERROR] El deploy en el servidor fallo. & pause & exit /b 1 )

echo.
echo ============================================================
echo   LISTO. Recarga la pagina en el celular (Ctrl+F5 o cerrar
echo   y volver a abrir). Ya deberias ver los guiones separados.
echo ============================================================
pause
