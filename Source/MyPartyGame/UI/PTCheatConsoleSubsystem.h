// Copyright Epic Games, Inc. All Rights Reserved.
// Consola de comandos OCULTA (anti-accidente / anti-trampa). NO es la consola de Unreal (esa está
// desactivada): es una caja de texto propia que:
//   1) Solo se abre con un GESTO SECRETO: Ctrl+Alt+Shift+K pulsado DOS veces seguidas (<0.6s). Así no
//      se abre por accidente ni la encuentra un jugador casual.
//   2) Arranca BLOQUEADA: hasta que no escribís el COMANDO MAESTRO (PT_CHEAT_UNLOCK) ningún otro comando
//      hace nada. Una vez desbloqueada (dura la sesión), reenvía lo que tecleás a ConsoleCommand del
//      PlayerController (ahí viven los exec dev: PTSolo, PTSpectate, etc.).
// Se arma 100% en C++ con Slate (no necesita WBP). El gesto se detecta con un input preprocessor global,
// así funciona en cualquier mapa (menú, lobby, partida) sin importar qué controlador tenga el foco.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PTCheatConsoleSubsystem.generated.h"

class SEditableTextBox;
class STextBlock;
class SWidget;

UCLASS()
class MYPARTYGAME_API UPTCheatConsoleSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Abre/cierra la consola (lo llama el gesto secreto). */
    void Toggle();
    void Close();
    bool IsOpen() const { return bOpen; }

private:
    void Open();
    void OnTextCommitted(const FText& Text, ETextCommit::Type CommitType);
    void RunLine(const FString& Line);
    void SetOutput(const FString& Msg);

    bool bOpen     = false;
    bool bUnlocked = false; // false hasta tipear el comando maestro

    // Estado del gesto (doble combo).
    double LastComboTime = 0.0;

    // Restauración de input al cerrar (heurística: cursor visible = contexto UI → GameAndUI).
    bool bPrevCursor = false;

    // Slate (no UObjects): vivos solo mientras la consola está abierta.
    TSharedPtr<SWidget>          Root;
    TSharedPtr<SEditableTextBox> InputBox;
    TSharedPtr<STextBlock>       OutputText;

    // Input preprocessor que detecta el gesto a nivel aplicación.
    TSharedPtr<class FPTCheatInputProcessor> InputProcessor;
};
