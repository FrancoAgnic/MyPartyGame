@echo off
REM Lanza el juego en modo standalone real (no Editor, no PIE) sin pasar por
REM el .exe empaquetado -- evita el bloqueo de Smart App Control en esta PC.
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0MyPartyGame.uproject" -game -log
