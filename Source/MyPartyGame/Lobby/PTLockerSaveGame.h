// Copyright Epic Games, Inc. All Rights Reserved.
// Locker (casillero) del jugador: hasta 6 cabezas + 6 texturas de cuerpo, guardadas LOCAL (disco).
// Cada slot de cabeza guarda dos cosas:
//   - BakedBlob : el resultado COCINADO (geometría + texturas PNG). Es lo que se equipa y se replica.
//   - RawState  : el estado CRUDO del volumen (SDF + colores + ojos + pintura) para poder RE-EDITAR
//                 la cabeza más adelante sin empezar de cero (Fase 2; en Fase 1 puede ir vacío).
// El slot de cuerpo guarda solo la textura de pintura (PNG); el cuerpo no se esculpe.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PTLockerSaveGame.generated.h"

USTRUCT()
struct FPTLockerHeadSlot
{
    GENERATED_BODY()
    UPROPERTY() bool          bUsed = false;
    UPROPERTY() TArray<uint8> BakedBlob;  // cocinado (equipar/replicar)
    UPROPERTY() TArray<uint8> RawState;   // crudo (re-editar) — Fase 2
    UPROPERTY() TArray<uint8> ThumbPNG;   // miniatura renderizada (para el tile del Locker)
};

USTRUCT()
struct FPTLockerBodySlot
{
    GENERATED_BODY()
    UPROPERTY() bool          bUsed = false;
    UPROPERTY() TArray<uint8> BodyPNG;    // textura de pintura del cuerpo
    UPROPERTY() TArray<uint8> ThumbPNG;   // miniatura renderizada
};

UCLASS()
class MYPARTYGAME_API UPTLockerSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    static constexpr int32 NumHeadSlots = 6;
    static constexpr int32 NumBodySlots = 6;

    UPROPERTY() TArray<FPTLockerHeadSlot> HeadSlots;
    UPROPERTY() TArray<FPTLockerBodySlot> BodySlots;

    // Índice del slot equipado (-1 = ninguno). Solo lo equipado se replica a los demás.
    UPROPERTY() int32 EquippedHead = -1;
    UPROPERTY() int32 EquippedBody = -1;

    // Asegura que los arrays tengan el tamaño fijo (por si el save es viejo o recién creado).
    void EnsureSized()
    {
        if (HeadSlots.Num() != NumHeadSlots) HeadSlots.SetNum(NumHeadSlots);
        if (BodySlots.Num() != NumBodySlots) BodySlots.SetNum(NumBodySlots);
    }
};
