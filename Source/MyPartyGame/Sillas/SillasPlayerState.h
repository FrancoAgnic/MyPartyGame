// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 1 — PlayerState del modo Sillas.
// Hereda de APTPlayerState para conservar DisplayName/bIsHost del template
// (ASillasGameMode los restaura tras el seamless travel desde el lobby).
// Rol replicado (D1: los eliminados se convierten en cazadores) y puntos del
// match (D7b: todos los roles puntúan en todo momento).

#pragma once
#include "CoreMinimal.h"
#include "PTPlayerState.h"
#include "SillasPlayerState.generated.h"

UENUM(BlueprintType)
enum class ESillasRole : uint8
{
    Silla   UMETA(DisplayName="Silla"),
    Cazador UMETA(DisplayName="Cazador")
};

UCLASS()
class MYPARTYGAME_API ASillasPlayerState : public APTPlayerState
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(ReplicatedUsing=OnRep_Rol, BlueprintReadOnly, Category="Sillas")
    ESillasRole Rol = ESillasRole::Silla;

    // true si empezó la ronda como silla y ya fue capturada (aunque su rol ahora
    // sea Cazador por la infección de D1) — lo necesita el puntaje y el HUD.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Sillas")
    bool bEliminadoEstaRonda = false;

    // Puntos acumulados del MATCH (D7b). El Score heredado del engine no se usa.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Sillas")
    int32 PuntosMatch = 0;

    // Llamar solo desde el servidor (HasAuthority).
    void Server_SetRol(ESillasRole InRol);
    void Server_MarcarEliminado();
    void Server_SumarPuntos(int32 Puntos);
    void Server_ResetRonda(); // vuelve a Silla y limpia bEliminadoEstaRonda

    UFUNCTION() void OnRep_Rol();
};
