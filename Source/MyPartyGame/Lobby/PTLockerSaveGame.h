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
#include "PTLockerSettings.h"   // máximo de slots editable (Project Settings → Game → Locker)
#include "PTLockerSaveGame.generated.h"

USTRUCT()
struct FPTLockerHeadSlot
{
    GENERATED_BODY()
    UPROPERTY() bool          bUsed = false;
    UPROPERTY() TArray<uint8> BakedBlob;  // cocinado (equipar/replicar)
    UPROPERTY() TArray<uint8> RawState;   // crudo (re-editar) — Fase 2
    UPROPERTY() TArray<uint8> ThumbPNG;   // miniatura renderizada (para el tile del Locker)
    UPROPERTY() FString       WorkshopId; // id del item del Workshop si se descargó de ahí (vacío = creación propia)
};

USTRUCT()
struct FPTLockerBodySlot
{
    GENERATED_BODY()
    UPROPERTY() bool          bUsed = false;
    UPROPERTY() TArray<uint8> BodyPNG;    // textura de pintura del cuerpo
    UPROPERTY() TArray<uint8> ThumbPNG;   // miniatura renderizada
    UPROPERTY() FString       WorkshopId; // id del item del Workshop si se descargó de ahí (vacío = creación propia)
};

UCLASS()
class MYPARTYGAME_API UPTLockerSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    // Cantidad de slots (cabezas = cuerpos). El máximo se edita en Project Settings → Game → Locker
    // (UPTLockerSettings::MaxSkinSlots). Este constexpr es solo un fallback si el settings no cargó.
    static constexpr int32 DefaultSlots = 22;

    UPROPERTY() TArray<FPTLockerHeadSlot> HeadSlots;
    UPROPERTY() TArray<FPTLockerBodySlot> BodySlots;

    // Índice del slot equipado (-1 = ninguno). Solo lo equipado se replica a los demás.
    UPROPERTY() int32 EquippedHead = -1;
    UPROPERTY() int32 EquippedBody = -1;

    // Ajusta la cantidad de slots al valor configurado (Project Settings → Game → Locker), cabeza y
    // cuerpo con la MISMA cantidad. Crece o achica hasta ese máximo, PERO nunca por debajo del último
    // slot que tenga una skin guardada (así bajar el máximo no borra skins).
    void EnsureSized()
    {
        const int32 Desired = UPTLockerSettings::GetMaxSkinSlots();
        int32 MinKeep = 1; // el slot 0 (Default) siempre existe
        for (int32 i = 0; i < HeadSlots.Num(); ++i) if (HeadSlots[i].bUsed) MinKeep = FMath::Max(MinKeep, i + 1);
        for (int32 i = 0; i < BodySlots.Num(); ++i) if (BodySlots[i].bUsed) MinKeep = FMath::Max(MinKeep, i + 1);
        const int32 N = FMath::Max(Desired, MinKeep);
        if (HeadSlots.Num() != N) HeadSlots.SetNum(N);
        if (BodySlots.Num() != N) BodySlots.SetNum(N);
    }
};
