// CHAT DEL STREAM en el modo CLÁSICO (online con amigos): el host juega con sus amigos y su chat de Twitch /
// Kick puede sumarse escribiendo !unirse. Los que se unen adivinan desde el chat (nunca esculpen).
//
// Vive en el GameInstance del HOST (sobrevive lobby → partida → lobby):
//   · lee el chat con FPTStreamChat (el mismo lector del modo audiencia, sin cuentas; mismos canales guardados),
//   · lleva la lista de espectadores unidos (si se unen en el lobby, entran al arrancar la partida),
//   · cuando hay partida (APTSculptGameMode) le pide que cree sus PlayerStates y le pasa sus mensajes.
// Se prende con FPTMatchSettings::bStreamChat (botón "Chat del stream" del panel de ajustes del host).
// En modo local / audiencia no hace nada: ahí el chat lo maneja UPTLocalPartySubsystem.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "PTStreamChat.h"
#include "PTStreamChatSubsystem.generated.h"

class APTSculptGameMode;
class APTPlayerState;

UCLASS()
class MYPARTYGAME_API UPTStreamChatSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;

    /** Switch del host para la próxima partida (y la actual). */
    void SetEnabled(bool bEnabled);
    bool IsEnabled() const;
    /** Prendido Y aplicable: host / server, no modo local / audiencia. */
    bool IsActive() const;

    /** Canal (nombre o link; vacío = no leer esa plataforma). Es el mismo que usa el modo audiencia (se guarda
     *  en GameUserSettings). */
    void SetChannel(EPTChatPlatform Platform, const FString& Channel);
    FString GetChannel(EPTChatPlatform Platform) const;
    FPTStreamChat::EStatus GetStatus(EPTChatPlatform Platform) const;
    FString GetError(EPTChatPlatform Platform) const;
    int32 GetViewerCount() const { return Viewers.Num(); }

    int32 MaxViewers    = 150;
    float GuessCooldown = 1.0f; // segundos entre mensajes de una misma persona (anti-spam)
    static constexpr int32 MaxNameLen = 12;
    static constexpr int32 MaxMsgLen  = 60;

    // ── FTickableGameObject ──
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UPTStreamChatSubsystem, STATGROUP_Tickables); }
    virtual bool IsTickable() const override { return !IsTemplate(); }
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }

private:
    struct FViewer
    {
        FString      Key;  // "1:<id de Twitch>" / "2:<id de Kick>"
        FString      Name;
        FString      Lang = TEXT("es");
        FLinearColor Color = FLinearColor::White;
        uint8        Platform = 1;
        double       LastChatAt = 0.0;
        TWeakObjectPtr<APTPlayerState> PS;
    };
    TArray<FViewer> Viewers;
    TSharedPtr<FPTStreamChat> TwitchChat;
    TSharedPtr<FPTStreamChat> KickChat;
    TWeakObjectPtr<APTSculptGameMode> BoundGM;
    bool bWasActive = false;

    APTSculptGameMode* FindGameMode() const;
    void SyncReaders();   // conecta / corta los lectores según IsActive y los canales guardados
    void SyncGameMode();  // engancha la partida actual y le crea las PlayerStates que falten
    void HandleMessage(const FPTStreamChatMessage& M);
    void Join(const FPTStreamChatMessage& M, const FString& Key, const FString& Lang);
    void Leave(int32 Index, bool bAnnounce);
    void ClearViewers();
};
