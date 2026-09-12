// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTMapAuthorGameMode.h"
#include "../Lobby/PTLobbyCharacter.h"
#include "../Lobby/PTPlayerState.h"
#include "../Sculpt/PTSculptPlayerController.h"
#include "../Sculpt/PTSculptVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

APTMapAuthorGameMode::APTMapAuthorGameMode()
{
    // Reusa el pawn/PC de esculpido (todas las tools ya andan). El GameState queda en el base
    // (AGameStateBase, NO APTSculptGameState) → CanLocalPlayerSculpt() = true → esculpido LIBRE.
    // El BP derivado reasigna estas clases a los BP configurados (con materiales/HUD asignados).
    DefaultPawnClass      = APTLobbyCharacter::StaticClass();
    PlayerControllerClass = APTSculptPlayerController::StaticClass();
    PlayerStateClass      = APTPlayerState::StaticClass();
    bStartPlayersAsSpectators = false;
}

void APTMapAuthorGameMode::BeginPlay()
{
    Super::BeginPlay();
    EnsureVolume();
}

void APTMapAuthorGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);

    // El nivel de autoría es un vacío: el pawn tiene que VOLAR (si no, cae). Igual que el gameplay.
    if (APTLobbyCharacter* Char = NewPlayer ? Cast<APTLobbyCharacter>(NewPlayer->GetPawn()) : nullptr)
        Char->ApplyGameplayMovementMode();
}

APTSculptVolume* APTMapAuthorGameMode::FindVolume() const
{
    return Cast<APTSculptVolume>(
        UGameplayStatics::GetActorOfClass(GetWorld(), APTSculptVolume::StaticClass()));
}

void APTMapAuthorGameMode::EnsureVolume()
{
    if (FindVolume()) return; // el nivel ya trae uno (lo reusa)
    if (!VolumeClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MapAuthor] Sin VolumeClass asignado y el nivel no trae APTSculptVolume: no se puede esculpir."));
        return;
    }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APTSculptVolume* V = GetWorld()->SpawnActor<APTSculptVolume>(VolumeClass, FTransform::Identity, Params);
    UE_LOG(LogTemp, Log, TEXT("[MapAuthor] Volumen de esculpido %s."), V ? TEXT("spawneado") : TEXT("NO se pudo spawnear"));
}
