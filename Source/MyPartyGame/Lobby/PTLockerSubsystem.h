// Copyright Epic Games, Inc. All Rights Reserved.
// Administra el Locker del jugador (6 cabezas + 6 cuerpos) en disco LOCAL. Es la fuente de verdad de
// qué está guardado y qué está equipado. Solo el slot EQUIPADO se replica (lo hace el personaje al
// nacer, subiendo su blob); el resto queda local.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PTLockerSaveGame.h"
#include "PTLockerSubsystem.generated.h"

UCLASS()
class MYPARTYGAME_API UPTLockerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    int32 NumHeadSlots() const { return UPTLockerSaveGame::NumHeadSlots; }
    int32 NumBodySlots() const { return UPTLockerSaveGame::NumBodySlots; }

    // ── Consulta de slots ──
    bool  IsHeadSlotUsed(int32 Idx) const;
    bool  IsBodySlotUsed(int32 Idx) const;
    int32 GetEquippedHead() const { return Save ? Save->EquippedHead : -1; }
    int32 GetEquippedBody() const { return Save ? Save->EquippedBody : -1; }

    // ── Guardar una creación en un slot (persistente a disco) ──
    void SaveHeadSlot(int32 Idx, const TArray<uint8>& BakedBlob, const TArray<uint8>& RawState, const TArray<uint8>& ThumbPNG);
    void SaveBodySlot(int32 Idx, const TArray<uint8>& BodyPNG, const TArray<uint8>& ThumbPNG);
    const TArray<uint8>& GetHeadThumb(int32 Idx) const;
    const TArray<uint8>& GetBodyThumb(int32 Idx) const;
    void SetHeadThumb(int32 Idx, const TArray<uint8>& PNG);
    void SetBodyThumb(int32 Idx, const TArray<uint8>& PNG);
    // true si el slot es el "Default" reservado (slot 0 sin creación propia = look base).
    bool IsHeadSlotDefault(int32 Idx) const { return Idx == 0 && Save && Save->HeadSlots.IsValidIndex(0) && Save->HeadSlots[0].BakedBlob.Num() == 0; }
    bool IsBodySlotDefault(int32 Idx) const { return Idx == 0 && Save && Save->BodySlots.IsValidIndex(0) && Save->BodySlots[0].BodyPNG.Num() == 0; }

    // ── Equipar (marca el activo; solo esto se replica) ──
    void EquipHead(int32 Idx);
    void EquipBody(int32 Idx);

    // ── Datos del slot equipado / de un slot dado ──
    const TArray<uint8>& GetEquippedHeadBaked() const;
    const TArray<uint8>& GetEquippedBodyPNG() const;
    const TArray<uint8>& GetHeadRawState(int32 Idx) const; // para re-editar (Fase 2)
    const TArray<uint8>& GetHeadBaked(int32 Idx) const;
    const TArray<uint8>& GetBodyPNG(int32 Idx) const;

    void ClearHeadSlot(int32 Idx);
    void ClearBodySlot(int32 Idx);

    void SaveToDisk();

private:
    UPROPERTY() UPTLockerSaveGame* Save = nullptr;
    void EnsureLoaded();

    static const TArray<uint8> Empty; // referencia vacía para getters sin dato
};
