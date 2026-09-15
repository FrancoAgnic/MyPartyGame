// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTMapAuthorGameMode.h"
#include "../Lobby/PTLobbyCharacter.h"
#include "../Lobby/PTPlayerState.h"
#include "../Sculpt/PTSculptPlayerController.h"
#include "../Sculpt/PTSculptVolume.h"
#include "PTMapEnvironment.h"
#include "../PTGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
    EnsureEnvironment();
    // Cargar el escenario guardado (si existe) para continuar donde lo dejaste. Diferido para que el
    // volumen (BeginPlay/Init) ya esté listo antes de aplicarle el snapshot.
    if (UWorld* W = GetWorld())
        W->GetTimerManager().SetTimer(LoadSavedTimer, this, &APTMapAuthorGameMode::LoadSavedMapIntoVolume, 0.4f, false);
}

void APTMapAuthorGameMode::LoadSavedMapIntoVolume()
{
    UPTGameInstance* GI = GetGameInstance<UPTGameInstance>();
    APTMapEnvironment* Env = Cast<APTMapEnvironment>(
        UGameplayStatics::GetActorOfClass(GetWorld(), APTMapEnvironment::StaticClass()));
    if (!GI || !Env) return;
    TArray<uint8> Blob;
    if (GI->LoadAuthoredMap(Blob))
    {
        Env->DeserializeEnvironment(Blob); // reconstruye los props colocados
        UE_LOG(LogTemp, Log, TEXT("[MapAuthor] Mapa guardado cargado (%d bytes)."), Blob.Num());
    }
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

void APTMapAuthorGameMode::EnsureEnvironment()
{
    if (UGameplayStatics::GetActorOfClass(GetWorld(), APTMapEnvironment::StaticClass())) return;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    UClass* Cls = EnvironmentClass ? *EnvironmentClass : APTMapEnvironment::StaticClass();
    GetWorld()->SpawnActor<APTMapEnvironment>(Cls, FTransform::Identity, Params);
}
