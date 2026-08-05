@echo off
REM Test LOCAL multi-instancia en una sola PC (sin Steam): fuerza el OnlineSubsystem
REM NULL, asi las sesiones son LAN (bIsLANMatch) y dos instancias en la misma
REM maquina se encuentran via "buscar salas". Correr este .bat DOS veces:
REM la primera ventana crea la sala, la segunda la busca y se une.
REM (El override -ini es necesario: si solo falta Steam, el guard del subsistema
REM  corta el login a proposito — ver MultiplayerSessionsSubsystem.cpp linea ~150.)
"D:\UnrealEngineVeersions\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0MyPartyGame.uproject" -game -log -windowed -resx=1280 -resy=720 -ini:Engine:[OnlineSubsystem]:DefaultPlatformService=Null
