// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 3 — Componente genérico de habilidad con cooldown (D10/P10b).
// El pawn (o su BP) le enchufa un USillasAbilityData; el componente maneja
// activación server-authoritative y cooldown replicado (para el HUD de Fase 5).
// Sin asset asignado, el componente existe pero no hace nada — el framework
// queda listo para cuando se decida el contenido (P10b).

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SillasAbilityComponent.generated.h"

class USillasAbilityData;

UCLASS(ClassGroup=(Sillas), meta=(BlueprintSpawnableComponent))
class MYPARTYGAME_API USillasAbilityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USillasAbilityComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Llamar desde el input del pawn local. Valida en el servidor.
    UFUNCTION(BlueprintCallable, Category="Habilidad")
    void IntentarActivar();

    UFUNCTION(BlueprintPure, Category="Habilidad")
    float GetCooldownRestante() const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Habilidad")
    TObjectPtr<USillasAbilityData> Habilidad;

private:
    UFUNCTION(Server, Reliable)
    void Server_Activar();

    // Momento (tiempo de servidor sincronizado) en que vuelve a estar disponible.
    UPROPERTY(Replicated)
    float CooldownHastaServerTime = 0.f;
};
