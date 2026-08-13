// Copyright Epic Games, Inc. All Rights Reserved.
// Componente compartido de sonidos de esculpido: loop 3D por herramienta (Add/Erase/Paint) que sigue
// al pincel + one-shots (ojos, undo simple, borrar todo). Los assets viven en UPTGameInstance (se
// asignan una sola vez y valen para el gameplay del Lvl-01 y para editar skins en el lobby).

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PTSculptVolume.h" // EPTEditMode
#include "PTSculptSoundComponent.generated.h"

class UAudioComponent;
class USoundBase;
class UPTGameInstance;

UCLASS(ClassGroup=(PT), meta=(BlueprintSpawnableComponent))
class MYPARTYGAME_API UPTSculptSoundComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    /** Llamar cada frame con el estado del pincel. Mantiene el loop de la herramienta activa (Add/Erase/
     *  Paint) siguiendo WorldPos; lo corta si bActive=false, si hay ojos, o si la herramienta no tiene loop. */
    void SetActiveTool(EPTEditMode Mode, bool bEyes, bool bActive, const FVector& WorldPos);
    void StopLoop();

    void PlayEyes(const FVector& WorldPos);
    void PlayUndoSimple(const FVector& WorldPos);
    void PlayUndoClearAll(const FVector& WorldPos);

protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    UPTGameInstance* GI() const;
    USoundBase* LoopSoundFor(EPTEditMode Mode, bool bEyes) const;
    void PlayOneShot3D(USoundBase* S, const FVector& Pos);

    UPROPERTY(Transient) UAudioComponent* Loop = nullptr;
    EPTEditMode LoopMode = EPTEditMode::Smooth; // Smooth = "ninguno" (Smooth no tiene loop)
    bool bLoopIsEyes = false;
};
