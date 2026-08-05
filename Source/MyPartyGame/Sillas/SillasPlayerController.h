// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 0/1 — PlayerController de la arena.
// IMPORTANTE: en el template 5.x los mapping contexts de Enhanced Input los
// agrega el PlayerController (BP_ThirdPersonPlayerController), no el pawn.
// Como la arena usa esta clase, agregar los contextos es tarea nuestra — sin
// esto NINGÚN pawn responde (ni mannequin ni silla): botones muertos y cámara
// congelada. El input de captura del cazador (clic mantenido) llega en Fase 2.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SillasPlayerController.generated.h"

class UInputMappingContext;

UCLASS()
class MYPARTYGAME_API ASillasPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ASillasPlayerController();

    virtual void BeginPlay() override;

protected:
    // Se agregan en orden (prioridad = índice). Defaults: IMC_Default +
    // IMC_MouseLook del template ThirdPerson (los mismos del mannequin).
    UPROPERTY(EditDefaultsOnly, Category="Input")
    TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;
};
