// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 1 — Señuelo: silla real, sólida e indestructible (D6/D9).
// REGLA DE ORO DEL CAMUFLAJE: su aspecto debe ser IDÉNTICO al del pawn
// silla-jugador (ASillasPawnSilla). Ambos comparten mesh y escala por defecto;
// si se cambia uno, cambiar el otro (Fase 6 unifica con el asset temático real).

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SillasSenuelo.generated.h"

UCLASS()
class MYPARTYGAME_API ASillasSenuelo : public AActor
{
    GENERATED_BODY()

public:
    ASillasSenuelo();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sillas")
    TObjectPtr<UStaticMeshComponent> Mesh;
};
