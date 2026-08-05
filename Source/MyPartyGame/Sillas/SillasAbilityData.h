// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 3 — Definición de una habilidad de silla como DataAsset enchufable (D10).
// El CONTENIDO concreto espera la decisión P10b (candidatas: sonido señuelo,
// intercambio con señuelo, modo rígido, taunt). Cada habilidad se implementa
// como subclase (C++ o Blueprint) de este asset sobreescribiendo Ejecutar();
// el estado (cooldown) NO vive acá — vive en USillasAbilityComponent, este
// asset es configuración pura y compartida.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SillasAbilityData.generated.h"

UCLASS(Blueprintable, Abstract, BlueprintType)
class MYPARTYGAME_API USillasAbilityData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Habilidad")
    FText Nombre;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Habilidad", meta=(ClampMin="0.0"))
    float CooldownSeg = 10.0f;

    // Ejecuta el efecto. SIEMPRE llamada en el servidor (convención 3 del plan);
    // lo cosmético que necesite verse en clientes debe salir por multicast/replicación
    // desde lo que la habilidad toque.
    UFUNCTION(BlueprintNativeEvent, Category="Habilidad")
    void Ejecutar(APawn* Duenio) const;
    virtual void Ejecutar_Implementation(APawn* Duenio) const {}
};
