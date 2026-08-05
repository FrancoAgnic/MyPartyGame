// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 4 — Taunt/burla (D10b): la habilidad inicial de las sillas.
// Sonido burlón audible por TODOS (localizable): cero valor táctico directo,
// máximo valor de fiesta — y un mind game real: burlarse delata tu posición.
// Primera habilidad concreta del framework de Fase 3; el asset
// DA_HabilidadTaunt se asigna al componente Habilidad de BP_SillasPawnSilla.

#pragma once
#include "CoreMinimal.h"
#include "SillasAbilityData.h"
#include "SillasAbilityTaunt.generated.h"

class USoundBase;

UCLASS()
class MYPARTYGAME_API USillasAbilityTaunt : public USillasAbilityData
{
    GENERATED_BODY()

public:
    USillasAbilityTaunt();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Taunt")
    TObjectPtr<USoundBase> Sonido;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Taunt", meta=(ClampMin="0.0"))
    float RadioAudible = 2500.f;

    virtual void Ejecutar_Implementation(APawn* Duenio) const override;
};
