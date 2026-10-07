// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLockerSubsystem.h"
#include "PTHeadSaveGame.h"          // migración del save viejo (una sola cabeza)
#include "Kismet/GameplayStatics.h"
#include "Serialization/MemoryWriter.h" // empaquetar/leer el bundle de skin (Workshop)
#include "Serialization/MemoryReader.h"

namespace { const TCHAR* PTLockerSaveSlot = TEXT("PTLocker"); }

const TArray<uint8> UPTLockerSubsystem::Empty;

void UPTLockerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    EnsureLoaded();
}

void UPTLockerSubsystem::EnsureLoaded()
{
    if (Save) return;

    if (UGameplayStatics::DoesSaveGameExist(PTLockerSaveSlot, 0))
        Save = Cast<UPTLockerSaveGame>(UGameplayStatics::LoadGameFromSlot(PTLockerSaveSlot, 0));

    if (!Save)
        Save = Cast<UPTLockerSaveGame>(UGameplayStatics::CreateSaveGameObject(UPTLockerSaveGame::StaticClass()));
    if (!Save) return;
    Save->EnsureSized();

    // Migración: si no había Locker pero existe la cabeza vieja (slot único), meterla en la cabeza 0
    // y equiparla, para no perder lo que el jugador ya tenía.
    if (!Save->HeadSlots[0].bUsed && UGameplayStatics::DoesSaveGameExist(TEXT("PTHeadCustom"), 0))
    {
        if (UPTHeadSaveGame* Old = Cast<UPTHeadSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("PTHeadCustom"), 0)))
            if (Old->Blob.Num() > 0)
            {
                Save->HeadSlots[0].bUsed     = true;
                Save->HeadSlots[0].BakedBlob = Old->Blob;
                Save->EquippedHead           = 0;
            }
    }

    // Slot 0 = "Default" siempre disponible (look base del personaje). Si nadie lo llenó con una
    // creación propia, queda marcado como usado con blob vacío = equiparlo aplica el look por defecto.
    if (!Save->HeadSlots[0].bUsed) { Save->HeadSlots[0].bUsed = true; if (Save->EquippedHead < 0) Save->EquippedHead = 0; }
    if (!Save->BodySlots[0].bUsed) { Save->BodySlots[0].bUsed = true; if (Save->EquippedBody < 0) Save->EquippedBody = 0; }
    SaveToDisk();
}

void UPTLockerSubsystem::RefreshSlotCount()
{
    EnsureLoaded();
    if (!Save) return;
    const int32 Before = Save->HeadSlots.Num();
    Save->EnsureSized();
    if (Save->HeadSlots.Num() != Before) SaveToDisk();
}

bool UPTLockerSubsystem::IsHeadSlotUsed(int32 Idx) const
{
    return Save && Save->HeadSlots.IsValidIndex(Idx) && Save->HeadSlots[Idx].bUsed;
}
bool UPTLockerSubsystem::IsBodySlotUsed(int32 Idx) const
{
    return Save && Save->BodySlots.IsValidIndex(Idx) && Save->BodySlots[Idx].bUsed;
}

void UPTLockerSubsystem::SaveHeadSlot(int32 Idx, const TArray<uint8>& BakedBlob, const TArray<uint8>& RawState, const TArray<uint8>& ThumbPNG)
{
    EnsureLoaded();
    if (!Save || !Save->HeadSlots.IsValidIndex(Idx)) return;
    FPTLockerHeadSlot& S = Save->HeadSlots[Idx];
    S.bUsed = BakedBlob.Num() > 0;
    S.BakedBlob = BakedBlob;
    if (RawState.Num() > 0) S.RawState = RawState; // el crudo puede no venir (Fase 1)
    if (ThumbPNG.Num() > 0) S.ThumbPNG = ThumbPNG;
    SaveToDisk();
}
void UPTLockerSubsystem::SaveBodySlot(int32 Idx, const TArray<uint8>& BodyPNG, const TArray<uint8>& ThumbPNG)
{
    EnsureLoaded();
    if (!Save || !Save->BodySlots.IsValidIndex(Idx)) return;
    FPTLockerBodySlot& S = Save->BodySlots[Idx];
    S.bUsed = BodyPNG.Num() > 0;
    S.BodyPNG = BodyPNG;
    if (ThumbPNG.Num() > 0) S.ThumbPNG = ThumbPNG;
    SaveToDisk();
}
const TArray<uint8>& UPTLockerSubsystem::GetHeadThumb(int32 Idx) const
{
    if (Save && Save->HeadSlots.IsValidIndex(Idx)) return Save->HeadSlots[Idx].ThumbPNG;
    return Empty;
}
const TArray<uint8>& UPTLockerSubsystem::GetBodyThumb(int32 Idx) const
{
    if (Save && Save->BodySlots.IsValidIndex(Idx)) return Save->BodySlots[Idx].ThumbPNG;
    return Empty;
}
void UPTLockerSubsystem::SetHeadThumb(int32 Idx, const TArray<uint8>& PNG)
{
    EnsureLoaded();
    if (Save && Save->HeadSlots.IsValidIndex(Idx) && PNG.Num() > 0) { Save->HeadSlots[Idx].ThumbPNG = PNG; SaveToDisk(); }
}
void UPTLockerSubsystem::SetBodyThumb(int32 Idx, const TArray<uint8>& PNG)
{
    EnsureLoaded();
    if (Save && Save->BodySlots.IsValidIndex(Idx) && PNG.Num() > 0) { Save->BodySlots[Idx].ThumbPNG = PNG; SaveToDisk(); }
}

void UPTLockerSubsystem::EquipHead(int32 Idx)
{
    EnsureLoaded();
    if (!Save) return;
    Save->EquippedHead = (Save->HeadSlots.IsValidIndex(Idx) && Save->HeadSlots[Idx].bUsed) ? Idx : -1;
    SaveToDisk();
}
void UPTLockerSubsystem::EquipBody(int32 Idx)
{
    EnsureLoaded();
    if (!Save) return;
    Save->EquippedBody = (Save->BodySlots.IsValidIndex(Idx) && Save->BodySlots[Idx].bUsed) ? Idx : -1;
    SaveToDisk();
}

const TArray<uint8>& UPTLockerSubsystem::GetEquippedHeadBaked() const
{
    if (Save && Save->HeadSlots.IsValidIndex(Save->EquippedHead)) return Save->HeadSlots[Save->EquippedHead].BakedBlob;
    return Empty;
}
const TArray<uint8>& UPTLockerSubsystem::GetEquippedBodyPNG() const
{
    if (Save && Save->BodySlots.IsValidIndex(Save->EquippedBody)) return Save->BodySlots[Save->EquippedBody].BodyPNG;
    return Empty;
}
const TArray<uint8>& UPTLockerSubsystem::GetHeadRawState(int32 Idx) const
{
    const_cast<UPTLockerSubsystem*>(this)->EnsureLoaded(); // garantizar el save cargado (bug: re-editar
    // una cabeza mostraba el default porque el getter leía antes de cargar; recién Equip lo forzaba).
    if (Save && Save->HeadSlots.IsValidIndex(Idx)) return Save->HeadSlots[Idx].RawState;
    return Empty;
}
const TArray<uint8>& UPTLockerSubsystem::GetHeadBaked(int32 Idx) const
{
    const_cast<UPTLockerSubsystem*>(this)->EnsureLoaded();
    if (Save && Save->HeadSlots.IsValidIndex(Idx)) return Save->HeadSlots[Idx].BakedBlob;
    return Empty;
}
const TArray<uint8>& UPTLockerSubsystem::GetBodyPNG(int32 Idx) const
{
    if (Save && Save->BodySlots.IsValidIndex(Idx)) return Save->BodySlots[Idx].BodyPNG;
    return Empty;
}

void UPTLockerSubsystem::ClearHeadSlot(int32 Idx)
{
    EnsureLoaded();
    if (!Save || !Save->HeadSlots.IsValidIndex(Idx)) return;
    Save->HeadSlots[Idx] = FPTLockerHeadSlot();
    if (Save->EquippedHead == Idx) Save->EquippedHead = -1;
    SaveToDisk();
}
void UPTLockerSubsystem::ClearBodySlot(int32 Idx)
{
    EnsureLoaded();
    if (!Save || !Save->BodySlots.IsValidIndex(Idx)) return;
    Save->BodySlots[Idx] = FPTLockerBodySlot();
    if (Save->EquippedBody == Idx) Save->EquippedBody = -1;
    SaveToDisk();
}

void UPTLockerSubsystem::SaveToDisk()
{
    if (Save) UGameplayStatics::SaveGameToSlot(Save, PTLockerSaveSlot, 0);
}

// ── Vista unificada de SKIN (modo "un solo slot") ───────────────────────────────
const TArray<uint8>& UPTLockerSubsystem::GetSkinThumb(int32 Idx) const
{
    // La miniatura de la cabeza representa la skin; si la cabeza no tiene foto, usar la del cuerpo.
    const TArray<uint8>& H = GetHeadThumb(Idx);
    if (H.Num() > 0) return H;
    return GetBodyThumb(Idx);
}
int32 UPTLockerSubsystem::FirstFreeSkinSlot() const
{
    if (!Save) return -1;
    const int32 N = FMath::Min(Save->HeadSlots.Num(), Save->BodySlots.Num());
    for (int32 i = 1; i < N; ++i) // saltear el slot 0 (Default)
        if (!Save->HeadSlots[i].bUsed && !Save->BodySlots[i].bUsed) return i;
    return -1;
}

// ── Workshop de skins ───────────────────────────────────────────────────────────
int32 UPTLockerSubsystem::FirstFreeHeadSlot() const
{
    if (!Save) return -1;
    for (int32 i = 1; i < Save->HeadSlots.Num(); ++i) // salteamos el slot 0 (Default)
        if (!Save->HeadSlots[i].bUsed) return i;
    return -1;
}
int32 UPTLockerSubsystem::FirstFreeBodySlot() const
{
    if (!Save) return -1;
    for (int32 i = 1; i < Save->BodySlots.Num(); ++i)
        if (!Save->BodySlots[i].bUsed) return i;
    return -1;
}

bool UPTLockerSubsystem::ExportSkinBundle(int32 HeadIdx, int32 BodyIdx, TArray<uint8>& OutBytes) const
{
    const_cast<UPTLockerSubsystem*>(this)->EnsureLoaded();
    if (!Save) return false;

    // Copias locales (FArchive operator<< necesita referencias NO const).
    TArray<uint8> HBaked = GetHeadBaked(HeadIdx);
    if (HBaked.Num() == 0) return false; // sin cabeza custom → nada que publicar
    TArray<uint8> HRaw   = GetHeadRawState(HeadIdx);
    TArray<uint8> HThumb = GetHeadThumb(HeadIdx);
    TArray<uint8> BPNG   = GetBodyPNG(BodyIdx);
    TArray<uint8> BThumb = GetBodyThumb(BodyIdx);

    OutBytes.Reset();
    FMemoryWriter Ar(OutBytes, /*bIsPersistent=*/true);
    int32 Version = 1;
    Ar << Version;
    Ar << HBaked << HRaw << HThumb << BPNG << BThumb;
    return true;
}

int32 UPTLockerSubsystem::ImportSkinBundle(const TArray<uint8>& InBytes, int32& OutBodyIdx)
{
    OutBodyIdx = -1;
    EnsureLoaded();
    if (!Save || InBytes.Num() == 0) return -1;

    FMemoryReader Ar(InBytes, /*bIsPersistent=*/true);
    int32 Version = 0;
    Ar << Version;
    if (Version != 1) return -1;
    TArray<uint8> HBaked, HRaw, HThumb, BPNG, BThumb;
    Ar << HBaked << HRaw << HThumb << BPNG << BThumb;
    if (HBaked.Num() == 0) return -1;

    // Una SKIN ocupa el MISMO índice en cabeza y cuerpo (modelo fusionado del Locker). Buscar UN solo
    // slot libre para las dos partes, así la skin importada queda emparejada (editar/equipar/vaciar la
    // tratan como una unidad). Antes se pedían dos slots libres distintos y podían caer en índices
    // distintos → la cabeza y el cuerpo quedaban "despareados".
    const int32 SkinIdx = FirstFreeSkinSlot();
    if (SkinIdx < 0) return -1; // Locker lleno
    SaveHeadSlot(SkinIdx, HBaked, HRaw, HThumb);
    if (BPNG.Num() > 0) { SaveBodySlot(SkinIdx, BPNG, BThumb); OutBodyIdx = SkinIdx; }
    return SkinIdx;
}

bool UPTLockerSubsystem::ParseSkinBundle(const TArray<uint8>& InBytes, TArray<uint8>& OutHeadBaked, TArray<uint8>& OutBodyPNG)
{
    if (InBytes.Num() == 0) return false;
    FMemoryReader Ar(InBytes, /*bIsPersistent=*/true);
    int32 Version = 0;
    Ar << Version;
    if (Version != 1) return false;
    TArray<uint8> HRaw, HThumb, BThumb;
    Ar << OutHeadBaked << HRaw << HThumb << OutBodyPNG << BThumb;
    return !Ar.IsError() && (OutHeadBaked.Num() > 0 || OutBodyPNG.Num() > 0);
}
