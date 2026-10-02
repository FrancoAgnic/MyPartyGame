// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLockerSubsystem.h"
#include "PTHeadSaveGame.h"          // migración del save viejo (una sola cabeza)
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

namespace
{
    const TCHAR* PTLockerSaveSlot = TEXT("PTLocker");

    // Archivos de una skin publicada en el Workshop (ver ExportSkinToFolder / ImportSkinFromFolder).
    const TCHAR* PTSkinJson    = TEXT("skin.json");
    const TCHAR* PTSkinHead    = TEXT("head.bin");     // BakedBlob (equipar/replicar)
    const TCHAR* PTSkinHeadRaw = TEXT("head_raw.bin"); // RawState (re-editar)
    const TCHAR* PTSkinBody    = TEXT("body.png");     // textura de pintura del cuerpo
    const TCHAR* PTSkinThumb   = TEXT("thumb.png");    // miniatura del tile del Locker

    // Topes de tamaño: el contenido del Workshop es de terceros → no cargar archivos absurdos a memoria.
    constexpr int64 PTSkinMaxBlob  = 16ll * 1024 * 1024;
    constexpr int64 PTSkinMaxRaw   = 32ll * 1024 * 1024;
    constexpr int64 PTSkinMaxImage =  8ll * 1024 * 1024;

    bool PT_LoadCapped(const FString& Path, int64 MaxBytes, TArray<uint8>& Out)
    {
        Out.Reset();
        const int64 Size = IFileManager::Get().FileSize(*Path);
        if (Size <= 0 || Size > MaxBytes) return false;
        return FFileHelper::LoadFileToArray(Out, *Path);
    }

    bool PT_IsPNG(const TArray<uint8>& B)
    {
        static const uint8 Sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
        return B.Num() >= 8 && FMemory::Memcmp(B.GetData(), Sig, 8) == 0;
    }

    // El estado crudo (re-edición) es [magic][TArray<uint8> campo][TArray<FVector4> ojos]. Chequeo
    // liviano de que los conteos declarados entren en el buffer, para no reservar memoria gigante al
    // re-editar una skin descargada malformada. Si no pasa, se descarta (la skin igual se equipa).
    bool PT_RawStateLooksSane(const TArray<uint8>& Raw)
    {
        int64 Pos = sizeof(uint32);
        auto ReadCount = [&](int32& Out) -> bool
        {
            if (Pos + (int64)sizeof(int32) > Raw.Num()) return false;
            FMemory::Memcpy(&Out, Raw.GetData() + Pos, sizeof(int32));
            Pos += sizeof(int32);
            return Out >= 0;
        };
        int32 NField = 0, NEyes = 0;
        if (!ReadCount(NField) || Pos + NField > Raw.Num()) return false;
        Pos += NField;
        if (!ReadCount(NEyes)) return false;
        return Pos + (int64)NEyes * 4 * (int64)sizeof(float) <= Raw.Num(); // cota inferior (float o double)
    }
}

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

// ── Skins del Workshop ──────────────────────────────────────────────────────────────────────────

bool UPTLockerSubsystem::IsSlotPublishable(bool bHead, int32 Idx) const
{
    if (!Save) return false;
    if (bHead) return Save->HeadSlots.IsValidIndex(Idx) && Save->HeadSlots[Idx].bUsed && Save->HeadSlots[Idx].BakedBlob.Num() > 0;
    return Save->BodySlots.IsValidIndex(Idx) && Save->BodySlots[Idx].bUsed && Save->BodySlots[Idx].BodyPNG.Num() > 0;
}

FString UPTLockerSubsystem::GetSlotWorkshopId(bool bHead, int32 Idx) const
{
    if (!Save) return FString();
    if (bHead) return Save->HeadSlots.IsValidIndex(Idx) ? Save->HeadSlots[Idx].WorkshopId : FString();
    return Save->BodySlots.IsValidIndex(Idx) ? Save->BodySlots[Idx].WorkshopId : FString();
}

int32 UPTLockerSubsystem::FindSlotByWorkshopId(bool bHead, const FString& WorkshopId) const
{
    if (!Save || WorkshopId.IsEmpty()) return -1;
    if (bHead)
    {
        for (int32 i = 0; i < Save->HeadSlots.Num(); ++i)
            if (Save->HeadSlots[i].bUsed && Save->HeadSlots[i].WorkshopId == WorkshopId) return i;
    }
    else
    {
        for (int32 i = 0; i < Save->BodySlots.Num(); ++i)
            if (Save->BodySlots[i].bUsed && Save->BodySlots[i].WorkshopId == WorkshopId) return i;
    }
    return -1;
}

int32 UPTLockerSubsystem::FindFreeSlot(bool bHead) const
{
    if (!Save) return -1;
    const int32 N = bHead ? Save->HeadSlots.Num() : Save->BodySlots.Num();
    for (int32 i = 1; i < N; ++i) // el 0 es el Default reservado
        if (!(bHead ? Save->HeadSlots[i].bUsed : Save->BodySlots[i].bUsed)) return i;
    return -1;
}

bool UPTLockerSubsystem::ExportSkinToFolder(bool bHead, int32 Idx, const FString& Folder, FString& OutThumbPath) const
{
    OutThumbPath.Reset();
    if (!IsSlotPublishable(bHead, Idx)) return false;
    IFileManager::Get().MakeDirectory(*Folder, /*Tree=*/true);

    bool bOk = true;
    const TArray<uint8>* Thumb = nullptr;
    if (bHead)
    {
        const FPTLockerHeadSlot& S = Save->HeadSlots[Idx];
        bOk &= FFileHelper::SaveArrayToFile(S.BakedBlob, *FPaths::Combine(Folder, PTSkinHead));
        // El crudo es lo que permite RE-EDITAR la skin descargada (sin él, se equipa pero se edita de cero).
        if (S.RawState.Num() > 0) bOk &= FFileHelper::SaveArrayToFile(S.RawState, *FPaths::Combine(Folder, PTSkinHeadRaw));
        Thumb = &S.ThumbPNG;
    }
    else
    {
        const FPTLockerBodySlot& S = Save->BodySlots[Idx];
        bOk &= FFileHelper::SaveArrayToFile(S.BodyPNG, *FPaths::Combine(Folder, PTSkinBody));
        Thumb = &S.ThumbPNG;
    }
    if (Thumb && PT_IsPNG(*Thumb))
    {
        const FString ThumbPath = FPaths::Combine(Folder, PTSkinThumb);
        if (FFileHelper::SaveArrayToFile(*Thumb, *ThumbPath)) OutThumbPath = ThumbPath;
    }

    const FString Json = FString::Printf(TEXT("{ \"Type\": \"%s\", \"Version\": 1 }"), bHead ? TEXT("Head") : TEXT("Body"));
    bOk &= FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Folder, PTSkinJson));
    return bOk;
}

bool UPTLockerSubsystem::ReadSkinType(const FString& Folder, bool& bOutHead)
{
    FString JsonStr;
    const FString JsonPath = FPaths::Combine(Folder, PTSkinJson);
    if (IFileManager::Get().FileSize(*JsonPath) > 64 * 1024) return false;
    if (!FFileHelper::LoadFileToString(JsonStr, *JsonPath)) return false;
    TSharedPtr<FJsonObject> Obj;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) return false;
    FString Type;
    if (!Obj->TryGetStringField(TEXT("Type"), Type)) return false;
    if (Type == TEXT("Head")) { bOutHead = true;  return true; }
    if (Type == TEXT("Body")) { bOutHead = false; return true; }
    return false;
}

int32 UPTLockerSubsystem::ImportSkinFromFolder(const FString& Folder, const FString& WorkshopId, bool& bOutHead, FString& OutError)
{
    EnsureLoaded();
    OutError.Reset();
    if (!Save || !ReadSkinType(Folder, bOutHead)) { OutError = TEXT("invalid"); return -1; }

    // Ya importada antes → reusar ese slot (si la editaste, no se pisa tu versión).
    const int32 Existing = FindSlotByWorkshopId(bOutHead, WorkshopId);
    if (Existing >= 0) return Existing;

    TArray<uint8> Thumb;
    if (!PT_LoadCapped(FPaths::Combine(Folder, PTSkinThumb), PTSkinMaxImage, Thumb) || !PT_IsPNG(Thumb)) Thumb.Reset();

    if (bOutHead)
    {
        TArray<uint8> Baked, Raw;
        if (!PT_LoadCapped(FPaths::Combine(Folder, PTSkinHead), PTSkinMaxBlob, Baked)) { OutError = TEXT("invalid"); return -1; }
        if (PT_LoadCapped(FPaths::Combine(Folder, PTSkinHeadRaw), PTSkinMaxRaw, Raw) && !PT_RawStateLooksSane(Raw)) Raw.Reset();

        const int32 Idx = FindFreeSlot(true);
        if (Idx < 0) { OutError = TEXT("full"); return -1; }
        FPTLockerHeadSlot& S = Save->HeadSlots[Idx];
        S = FPTLockerHeadSlot();
        S.bUsed      = true;
        S.BakedBlob  = MoveTemp(Baked);
        S.RawState   = MoveTemp(Raw);
        S.ThumbPNG   = MoveTemp(Thumb);
        S.WorkshopId = WorkshopId;
        SaveToDisk();
        return Idx;
    }

    TArray<uint8> Body;
    if (!PT_LoadCapped(FPaths::Combine(Folder, PTSkinBody), PTSkinMaxImage, Body) || !PT_IsPNG(Body)) { OutError = TEXT("invalid"); return -1; }

    const int32 Idx = FindFreeSlot(false);
    if (Idx < 0) { OutError = TEXT("full"); return -1; }
    FPTLockerBodySlot& S = Save->BodySlots[Idx];
    S = FPTLockerBodySlot();
    S.bUsed      = true;
    S.BodyPNG    = MoveTemp(Body);
    S.ThumbPNG   = MoveTemp(Thumb);
    S.WorkshopId = WorkshopId;
    SaveToDisk();
    return Idx;
}

void UPTLockerSubsystem::SaveToDisk()
{
    if (Save) UGameplayStatics::SaveGameToSlot(Save, PTLockerSaveSlot, 0);
}
