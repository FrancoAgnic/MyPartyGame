#include "PTSculptGameState.h"
#include "../Lobby/PTPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Net/UnrealNetwork.h"

void APTSculptGameState::OnRep_Masked()
{
    RefreshLocalMasked();
}

void APTSculptGameState::RefreshLocalMasked()
{
    // Idioma del jugador LOCAL: por la cultura actual (la que eligió en Settings).
    const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
    const bool bEnglish = Culture.StartsWith(TEXT("en"));
    MaskedWord = bEnglish ? MaskedWordEn : MaskedWordEs;
    if (MaskedWord.IsEmpty() && !bEnglish) MaskedWord = MaskedWordEn; // fallback si falta el idioma
    if (MaskedWord.IsEmpty() &&  bEnglish) MaskedWord = MaskedWordEs;
    OnTurnPhaseChanged.Broadcast(); // el HUD relee MaskedWord
}

void APTSculptGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APTSculptGameState, TurnPhase);
    DOREPLIFETIME(APTSculptGameState, CurrentSculptor);
    DOREPLIFETIME(APTSculptGameState, TurnEndServerTime);
    DOREPLIFETIME(APTSculptGameState, MaskedWordEs);
    DOREPLIFETIME(APTSculptGameState, MaskedWordEn);
    DOREPLIFETIME(APTSculptGameState, CurrentRound);
    DOREPLIFETIME(APTSculptGameState, TotalRounds);
}

float APTSculptGameState::GetTurnSecondsRemaining() const
{
    if (TurnPhase != EPTTurnPhase::Drawing) return 0.f;
    const double Remaining = TurnEndServerTime - GetServerWorldTimeSeconds();
    return FMath::Max(0.f, (float)Remaining);
}

bool APTSculptGameState::IsLocalPlayerSculptor() const
{
    if (!CurrentSculptor) return false;
    // Cliente: hay un solo PlayerController local; su PlayerState es el del jugador local.
    if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
        return PC->PlayerState == CurrentSculptor;
    return false;
}

void APTSculptGameState::OnRep_TurnPhase()
{
    OnTurnPhaseChanged.Broadcast();
}

void APTSculptGameState::Multicast_ChatLine_Implementation(const FString& Name, const FString& Message, EPTChatType Type)
{
    OnChatLine.Broadcast(Name, Message, Type);
}
