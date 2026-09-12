// Copyright Epic Games, Inc. All Rights Reserved.
// GameMode de AUTORÍA de mapas (crear mapa desde el juego). Standalone/solo: entrás al nivel plantilla
// del MapKit y esculpís LIBRE el escenario (sin palabra, sin reloj, sin turnos, sin bloqueo del cubo).
//
// Clave: NO usa APTSculptGameState → APTSculptPlayerController::CanLocalPlayerSculpt() devuelve true
// (esculpido libre). Reusa el PlayerController y el Character de esculpido (todas las tools ya andan).
// Al entrar: pone el pawn en modo vuelo y se asegura de que haya un APTSculptVolume para modelar.
//
// En BP_MapAuthorGameMode (derivado de esta clase) asignar:
//   - PlayerControllerClass = BP del PlayerController de esculpido (el mismo del gameplay)
//   - DefaultPawnClass      = BP del Character de esculpido (el mismo del gameplay)
//   - VolumeClass           = BP_SculptVolume (para spawnear el lienzo si el nivel no trae uno)

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PTMapAuthorGameMode.generated.h"

class APTSculptVolume;

UCLASS()
class MYPARTYGAME_API APTMapAuthorGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    APTMapAuthorGameMode();

    /** BP del volumen de esculpido a spawnear si el nivel plantilla no trae uno (asignar BP_SculptVolume). */
    UPROPERTY(EditDefaultsOnly, Category="MapAuthor")
    TSubclassOf<APTSculptVolume> VolumeClass;

protected:
    virtual void BeginPlay() override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

private:
    /** Si el nivel no trae un APTSculptVolume, spawnea uno (VolumeClass) en el origen. */
    void EnsureVolume();
    APTSculptVolume* FindVolume() const;
};
