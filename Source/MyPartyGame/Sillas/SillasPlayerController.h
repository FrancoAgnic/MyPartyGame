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

class UInputAction;
class UInputMappingContext;

UCLASS()
class MYPARTYGAME_API ASillasPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ASillasPlayerController();

    virtual void BeginPlay() override;

    // Kit de acciones runtime del modo, COMPARTIDO por todos los pawns que este
    // controller posea. Vive acá y no en el pawn a propósito: el controller
    // sobrevive a las re-posesiones (rotura → cazador, respawn por ronda), así
    // el contexto se registra UNA vez y jamás queda huérfano consumiendo teclas.
    // (Ese era el bug: contextos de pawns muertos bloqueaban el clic de los nuevos.)
    void AsegurarKitRuntime(); // idempotente; la llaman BeginPlay y los pawns

    UInputAction* GetIACaptura()   const { return IA_Captura; }
    UInputAction* GetIASprint()    const { return IA_Sprint; }
    UInputAction* GetIAEmpujon()   const { return IA_Empujon; }
    UInputAction* GetIAHabilidad() const { return IA_Habilidad; }
    UInputAction* GetIAAguantar()  const { return IA_Aguantar; }

protected:
    // Se agregan en orden (prioridad = índice). Defaults: IMC_Default +
    // IMC_MouseLook del template ThirdPerson (los mismos del mannequin).
    UPROPERTY(EditDefaultsOnly, Category="Input")
    TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

private:
    UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Captura;
    UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Sprint;
    UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Empujon;
    UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Habilidad;
    UPROPERTY(Transient) TObjectPtr<UInputAction> IA_Aguantar;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> KitIMC;
    bool bKitAgregado = false;
};
