@echo off
cd /d "C:\Users\franc\Documents\Unreal Projects\MyPartyGame"

:: Leer mensaje de commit del primer argumento, o usar uno generico
set MSG=%~1
if "%MSG%"=="" set MSG=Fase 1: UMultiplayerSessionsSubsystem - Login, Create, Find, Join, Destroy, Start

git add Source/ Config/ MyPartyGame.uproject .gitignore
git commit -m "%MSG%"
git push origin main

echo.
echo LISTO - cambios subidos a GitHub

:: Guardar resultado en log
git log --oneline -3 > git_last_push.log 2>&1
echo --- >> git_last_push.log
git status >> git_last_push.log 2>&1

timeout /t 3
