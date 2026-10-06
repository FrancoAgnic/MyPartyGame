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

    // Cantidad de slots (según Project Settings → Game → Locker, mismo valor cabeza/cuerpo).
    int32 NumHeadSlots() const { return Save ? Save->HeadSlots.Num() : UPTLockerSaveGame::DefaultSlots; }
    int32 NumBodySlots() const { return Save ? Save->BodySlots.Num() : UPTLockerSaveGame::DefaultSlots; }

    // Relee la variable de máximo de slots y reajusta el save (crece/achica). Llamar al abrir el Locker
    // para que un cambio en Project Settings tenga efecto sin reiniciar el juego.
    void RefreshSlotCount();

    // ── Consulta de slots ──
    bool  IsHeadSlotUsed(int32 Idx) const;
    bool  IsBodySlotUsed(int32 Idx) const;
    int32 GetEquippedHead() const { return Save ? Save->EquippedHead : -1; }
    int32 GetEquippedBody() const { return Save ? Save->EquippedBody : -1; }

    // ── Vista UNIFICADA de SKIN (modo "un solo slot" del Locker): la skin del slot i = cabeza i + cuerpo i
    //    (ya emparejados por índice). No mueve ni borra datos: solo presenta el par como una unidad. ──
    bool  IsSkinSlotUsed(int32 Idx) const { return IsHeadSlotUsed(Idx) || IsBodySlotUsed(Idx); }
    // Índice de la skin equipada (la cabeza manda; al equipar una skin se equipan cabeza y cuerpo al mismo i).
    int32 GetEquippedSkin() const { return GetEquippedHead(); }
    // Miniatura representativa de la skin: la de la cabeza si hay, si no la del cuerpo.
    const TArray<uint8>& GetSkinThumb(int32 Idx) const;
    // Primer slot de SKIN libre (ni cabeza ni cuerpo usados), salteando el 0 (Default). -1 si no hay lugar.
    int32 FirstFreeSkinSlot() const;

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

    // ── Workshop de skins (una SKIN = cabeza + cuerpo en un paquete) ──────────
    // Serializa la skin (cabeza HeadIdx + cuerpo BodyIdx) a bytes: de la cabeza va BakedBlob (equipar) +
    // RawState (editar) + Thumb; del cuerpo va BodyPNG + Thumb. false si la cabeza no tiene geometría
    // (no hay nada que publicar). Para publicar al Workshop sin que el subsistema dependa del Locker.
    bool ExportSkinBundle(int32 HeadIdx, int32 BodyIdx, TArray<uint8>& OutBytes) const;
    // Importa un paquete de skin a slots LIBRES (cabeza + cuerpo). Devuelve el índice de cabeza importada
    // (-1 = falló / Locker lleno). OutBodyIdx = índice de cuerpo (-1 si la skin no traía cuerpo). NO equipa.
    int32 ImportSkinBundle(const TArray<uint8>& InBytes, int32& OutBodyIdx);
    // Primer slot LIBRE (no usado), salteando el slot 0 "Default". -1 si no hay lugar.
    int32 FirstFreeHeadSlot() const;
    int32 FirstFreeBodySlot() const;

private:
    UPROPERTY() UPTLockerSaveGame* Save = nullptr;
    void EnsureLoaded();

    static const TArray<uint8> Empty; // referencia vacía para getters sin dato
};
