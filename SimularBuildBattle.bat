@echo off
REM Simulacion del modo BUILD BATTLE (prototipo): una partida entera sin tocar nada. Tu jugador esculpe y vota
REM solo (PTBBDemo) y hay 4 bots que esculpen figuras y votan. 3 rondas de 45 s. Requiere compilar la rama
REM build-battle. Si la camara apunta para otro lado, es el mouse: Alt+Tab para soltarlo.
set "UE=D:\UnrealEngineVeersions\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%UE%" set "UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
start "" "%UE%" "%~dp0MyPartyGame.uproject" /Game/Template/levels/Lvl-01?listen -game -windowed -resx=1600 -resy=900 -nosteam -NoLiveCoding -ExecCmds="PTMatchTime 45 3,PTBuildBattle,PTBBDemo,PTBBBots 4"
