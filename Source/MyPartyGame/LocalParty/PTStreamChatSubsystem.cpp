#include "PTStreamChatSubsystem.h"
#include "../Sculpt/PTSculptGameMode.h"
#include "../Lobby/PTPlayerState.h"
#include "../PTGameInstance.h"
#include "../PTGameUserSettings.h"
#include "Engine/World.h"
#include "GameFramework/GameMode.h" // MatchState::LeavingMap
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogPTStreamChatSub, Log, All);

void UPTStreamChatSubsystem::Deinitialize()
{
    if (TwitchChat.IsValid()) TwitchChat->Stop();
    if (KickChat.IsValid())   KickChat->Stop();
    Super::Deinitialize();
}

bool UPTStreamChatSubsystem::IsEnabled() const
{
    const UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance());
    return GI && GI->PendingMatchSettings.bStreamChat;
}

void UPTStreamChatSubsystem::SetEnabled(bool bEnabled)
{
    if (UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance()))
        GI->PendingMatchSettings.bStreamChat = bEnabled;
    // Tick aplica el cambio (lectores + partida).
}

bool UPTStreamChatSubsystem::IsActive() const
{
    const UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance());
    if (!GI || !GI->PendingMatchSettings.bStreamChat || GI->bLocalPartyMode) return false;
    const UWorld* W = GI->GetWorld();
    return W && W->GetNetMode() != NM_Client; // solo el host lee el chat (los amigos lo ven replicado)
}

void UPTStreamChatSubsystem::SetChannel(EPTChatPlatform Platform, const FString& Channel)
{
    const FString Norm = FPTStreamChat::NormalizeChannel(Platform, Channel);
    if (UPTGameUserSettings* S = UPTGameUserSettings::Get())
    {
        if (Platform == EPTChatPlatform::Twitch) S->SetTwitchChannel(Norm); else S->SetKickChannel(Norm);
        S->SaveSettings();
    }
    SyncReaders();
}

FString UPTStreamChatSubsystem::GetChannel(EPTChatPlatform Platform) const
{
    const UPTGameUserSettings* S = UPTGameUserSettings::Get();
    if (!S) return FString();
    return Platform == EPTChatPlatform::Twitch ? S->GetTwitchChannel() : S->GetKickChannel();
}

FPTStreamChat::EStatus UPTStreamChatSubsystem::GetStatus(EPTChatPlatform Platform) const
{
    const TSharedPtr<FPTStreamChat>& C = Platform == EPTChatPlatform::Twitch ? TwitchChat : KickChat;
    return C.IsValid() ? C->GetStatus() : FPTStreamChat::EStatus::Off;
}

FString UPTStreamChatSubsystem::GetError(EPTChatPlatform Platform) const
{
    const TSharedPtr<FPTStreamChat>& C = Platform == EPTChatPlatform::Twitch ? TwitchChat : KickChat;
    return C.IsValid() ? C->GetLastError() : FString();
}

APTSculptGameMode* UPTStreamChatSubsystem::FindGameMode() const
{
    const UWorld* W = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    // Durante un viaje (partida → lobby y vuelta) el GameMode viejo sigue vivo en el mapa de transición: ahí no
    // se crea nada (crasheaba). Los espectadores se recrean al llegar a la próxima partida.
    if (!W || W->IsInSeamlessTravel()) return nullptr;
    APTSculptGameMode* GM = W->GetAuthGameMode<APTSculptGameMode>();
    if (!GM || !GM->HasActorBegunPlay() || GM->GetMatchState() == MatchState::LeavingMap) return nullptr;
    return GM->IsStreamChatGame() ? GM : nullptr;
}

void UPTStreamChatSubsystem::Tick(float DeltaTime)
{
    const bool bActive = IsActive();
    if (bActive != bWasActive)
    {
        bWasActive = bActive;
        SyncReaders();
        if (!bActive) ClearViewers(); // se apagó el switch: los espectadores salen de la partida
        UE_LOG(LogPTStreamChatSub, Log, TEXT("Chat del stream (modo clásico): %s."), bActive ? TEXT("ON") : TEXT("OFF"));
    }
    if (TwitchChat.IsValid()) TwitchChat->Tick();
    if (KickChat.IsValid())   KickChat->Tick();
    SyncGameMode();
}

void UPTStreamChatSubsystem::SyncReaders()
{
    const bool bActive = IsActive();
    auto Ensure = [this, bActive](TSharedPtr<FPTStreamChat>& Chat, EPTChatPlatform Platform)
    {
        if (!bActive) { if (Chat.IsValid()) Chat->Stop(); return; }
        if (!Chat.IsValid())
        {
            Chat = MakeShared<FPTStreamChat>(Platform);
            Chat->OnMessage.BindUObject(this, &UPTStreamChatSubsystem::HandleMessage);
        }
        Chat->Start(GetChannel(Platform)); // mismo canal = no hace nada; vacío = apaga esa plataforma
    };
    Ensure(TwitchChat, EPTChatPlatform::Twitch);
    Ensure(KickChat,   EPTChatPlatform::Kick);
}

void UPTStreamChatSubsystem::SyncGameMode()
{
    APTSculptGameMode* GM = IsActive() ? FindGameMode() : nullptr;
    if (GM != BoundGM.Get())
    {
        if (APTSculptGameMode* Old = BoundGM.Get()) Old->StreamChat_SetActive(false);
        BoundGM = GM;
        for (FViewer& V : Viewers) V.PS.Reset(); // las de la partida anterior murieron con su mundo
        if (GM) GM->StreamChat_SetActive(true);
    }
    if (!GM) return;
    // Qué chats se están leyendo (la pestaña se llama "Twitch", "Kick" o "Twitch + Kick"). Se setea cada tick:
    // si no cambió, no se replica nada.
    const uint8 Platforms = (GetChannel(EPTChatPlatform::Twitch).IsEmpty() ? 0 : 1)
                          | (GetChannel(EPTChatPlatform::Kick).IsEmpty()   ? 0 : 2);
    GM->StreamChat_SetActive(true, Platforms);

    // Los que se unieron antes (en el lobby o en la partida anterior) entran sin anuncio.
    for (FViewer& V : Viewers)
        if (!V.PS.IsValid()) V.PS = GM->StreamChat_AddViewer(V.Name, V.Lang, V.Color, V.Platform, /*bAnnounce=*/false);
}

void UPTStreamChatSubsystem::HandleMessage(const FPTStreamChatMessage& M)
{
    if (!IsActive()) return;
    const FString Text = M.Text.TrimStartAndEnd();
    if (Text.IsEmpty()) return;
    const FString Key = FString::Printf(TEXT("%d:%s"), (int32)M.Platform + 1, *M.UserId);
    const int32 Idx = Viewers.IndexOfByPredicate([&Key](const FViewer& V) { return V.Key == Key; });

    if (Text.StartsWith(TEXT("!")))
    {
        FString Cmd = Text, Rest;
        Text.Split(TEXT(" "), &Cmd, &Rest);
        Cmd = Cmd.ToLower();
        FString Lang;
        if (FPTStreamChat::ParseJoinCommand(Cmd, Lang)) { if (Idx == INDEX_NONE) Join(M, Key, Lang); return; }
        if (FPTStreamChat::IsLeaveCommand(Cmd))         { if (Idx != INDEX_NONE) Leave(Idx, true); return; }
        return; // comandos de otros bots del canal: no son intentos
    }
    if (Idx == INDEX_NONE) return; // no se unió: es chat normal del stream (no entra al juego)

    FViewer& V = Viewers[Idx];
    const double Now = FPlatformTime::Seconds();
    if (Now - V.LastChatAt < GuessCooldown) return;
    V.LastChatAt = Now;
    APTSculptGameMode* GM = FindGameMode();
    if (GM && GM == BoundGM.Get() && V.PS.IsValid()) GM->StreamChat_Message(V.PS.Get(), Text.Left(MaxMsgLen));
}

void UPTStreamChatSubsystem::Join(const FPTStreamChatMessage& M, const FString& Key, const FString& Lang)
{
    if (Viewers.Num() >= MaxViewers) return;

    // Nombre del chat (sin caracteres de control, recortado). Si choca con otro espectador, se numera.
    FString Base;
    for (TCHAR C : M.UserName) if (C >= 32 && C != 127) Base.AppendChar(C);
    Base = Base.TrimStartAndEnd().Left(MaxNameLen);
    if (Base.IsEmpty()) Base = TEXT("Viewer");
    FString Name = Base;
    for (int32 Num = 2; Viewers.ContainsByPredicate([&Name](const FViewer& X) { return X.Name.Equals(Name, ESearchCase::IgnoreCase); }); ++Num)
    {
        const FString Suffix = FString::FromInt(Num);
        Name = Base.Left(MaxNameLen - Suffix.Len()) + Suffix;
    }

    FViewer V;
    V.Key = Key;
    V.Name = Name;
    V.Lang = Lang;
    V.Platform = (uint8)M.Platform + 1;
    // Su color del chat si es legible sobre fondo oscuro; si no, uno al azar (estable por persona).
    V.Color = FLinearColor::MakeFromHSV8((uint8)(GetTypeHash(Key) & 0xFF), 150, 255);
    if (M.Color.Len() == 7 && M.Color.StartsWith(TEXT("#")))
    {
        const FLinearColor C = FLinearColor::FromSRGBColor(FColor::FromHex(M.Color));
        if (C.GetLuminance() > 0.12f) V.Color = C;
    }
    if (APTSculptGameMode* GM = FindGameMode(); GM && GM == BoundGM.Get())
        V.PS = GM->StreamChat_AddViewer(V.Name, V.Lang, V.Color, V.Platform, /*bAnnounce=*/true);
    Viewers.Add(V);
    UE_LOG(LogPTStreamChatSub, Log, TEXT("Se unió '%s' desde %s (%d en total)."), *Name,
        M.Platform == EPTChatPlatform::Kick ? TEXT("Kick") : TEXT("Twitch"), Viewers.Num());
}

void UPTStreamChatSubsystem::Leave(int32 Index, bool bAnnounce)
{
    if (!Viewers.IsValidIndex(Index)) return;
    if (APTSculptGameMode* GM = FindGameMode(); GM && GM == BoundGM.Get())
        if (APTPlayerState* PS = Viewers[Index].PS.Get()) GM->StreamChat_RemoveViewer(PS, bAnnounce);
    Viewers.RemoveAt(Index);
}

void UPTStreamChatSubsystem::ClearViewers()
{
    for (int32 i = Viewers.Num() - 1; i >= 0; --i) Leave(i, false);
}
