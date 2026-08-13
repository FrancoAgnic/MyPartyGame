#include "PTSculptGameState.h"
#include "../Lobby/PTPlayerState.h"
#include "../PTGameUserSettings.h"
#include "../PTTextTable.h"
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
    // Idioma del jugador LOCAL: se toma de NUESTRA preferencia guardada, NO de la cultura del
    // motor. Motivo: FInternationalization::SetCurrentCulture("en") falla en silencio si el juego
    // todavía no tiene datos de localización para ese idioma → la cultura seguiría en "es" y todos
    // verían la palabra en español. El idioma de la palabra es una opción nuestra, no de UE.
    const int32 Lang = PTText::GetCurrentLanguageIndex();

    MaskedWord = MaskedWords.IsValidIndex(Lang) ? MaskedWords[Lang] : FString();
    if (MaskedWord.IsEmpty()) // falta esa traducción → mostrar la primera que haya, nunca vacío
        for (const FString& S : MaskedWords) if (!S.IsEmpty()) { MaskedWord = S; break; }

    OnTurnPhaseChanged.Broadcast(); // el HUD relee MaskedWord
}

void APTSculptGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APTSculptGameState, TurnPhase);
    DOREPLIFETIME(APTSculptGameState, CurrentSculptor);
    DOREPLIFETIME(APTSculptGameState, TurnEndServerTime);
    DOREPLIFETIME(APTSculptGameState, MaskedWords);
    DOREPLIFETIME(APTSculptGameState, CurrentRound);
    DOREPLIFETIME(APTSculptGameState, TotalRounds);
    DOREPLIFETIME(APTSculptGameState, bTurnEndedAllGuessed);
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

void APTSculptGameState::Multicast_SystemLine_Implementation(FName Key, const FString& Arg0, int32 Arg1)
{
    EmitLocalSystemLine(Key, Arg0, Arg1);
}

void APTSculptGameState::Multicast_SomeoneGuessed_Implementation(APTPlayerState* Guesser, int32 Points)
{
    OnSomeoneGuessed.Broadcast(Guesser, Points); // el HUD muestra el mini-popup junto a su nombre
}

void APTSculptGameState::EmitLocalSystemLine(FName Key, const FString& Arg0, int32 Arg1)
{
    // Se arma acá, en el cliente, para que cada uno la lea en SU idioma.
    FFormatOrderedArguments Args;
    Args.Add(FText::FromString(Arg0));
    Args.Add(FText::AsNumber(Arg1));
    OnChatLine.Broadcast(FString(), PTText::Format(Key, Args).ToString(), EPTChatType::System);
}
