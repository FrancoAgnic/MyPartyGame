// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTPlayerState.h"
#include "PTLobbyCharacter.h"
#include "../PTTextTable.h"
#include "../PTGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

int32 APTPlayerState::GetLanguageIndex() const
{
    // Se resuelve el código contra la tabla del SERVIDOR (es el que elige y enmascara la palabra).
    // Si ese idioma no existe acá, cae al de referencia en vez de devolver un índice inválido.
    const int32 Idx = PTText::GetLanguageIndex(Language);
    return Idx == INDEX_NONE ? 0 : Idx;
}

void APTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APTPlayerState, DisplayName);
    DOREPLIFETIME(APTPlayerState, bIsHost);
    DOREPLIFETIME(APTPlayerState, bIsReady);
    DOREPLIFETIME(APTPlayerState, bHasGuessedThisTurn);
    DOREPLIFETIME(APTPlayerState, GameScore);
    // HeadBlob NO va acá a propósito: viaja troceado por RPC (ver UploadHead / SendHeadTo).
    DOREPLIFETIME(APTPlayerState, HeadVersion);
    DOREPLIFETIME(APTPlayerState, Language);
}

void APTPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
    Super::CopyProperties(NewPlayerState);
    if (APTPlayerState* PT = Cast<APTPlayerState>(NewPlayerState))
    {
        PT->DisplayName = DisplayName;
        PT->bIsHost     = bIsHost;
        PT->HeadBlob    = HeadBlob;    // la cabeza custom viaja al Lvl-01 (seamless travel)
        PT->HeadVersion = HeadVersion; // ...y su versión, para que el pawn nuevo la aplique
        PT->Language    = Language;    // el idioma también viaja (palabra por idioma en el juego)
        // bHasGuessedThisTurn NO se copia: es estado por-turno, arranca en false en el juego.
    }
}

// ── Sincronización de la cabeza, troceada ────────────────────────────────────────────────────
// Va por RPC y no por propiedad replicada porque el blob puede pasarse del tamaño máximo de una
// propiedad, y en ese caso Unreal lo tira sin avisar. Los RPC confiables se parten en varios
// paquetes solos y llegan en orden, así que sirven para cualquier tamaño.

namespace { constexpr int32 HeadChunkBytes = 8 * 1024; }

void APTPlayerState::UploadHead(const TArray<uint8>& Blob)
{
    if (Blob.Num() == 0) return;

    HeadBlob = Blob; // copia local: el dueño ve su cabeza sin esperar la vuelta del server
    const int32 Version     = HeadVersion + 1;
    const int32 TotalChunks = FMath::DivideAndRoundUp(Blob.Num(), HeadChunkBytes);

    for (int32 i = 0; i < TotalChunks; ++i)
    {
        const int32 Offset = i * HeadChunkBytes;
        const int32 Count  = FMath::Min(HeadChunkBytes, Blob.Num() - Offset);
        TArray<uint8> Chunk(Blob.GetData() + Offset, Count);
        Server_UploadHeadChunk(Version, i, TotalChunks, Chunk);
    }
    UE_LOG(LogTemp, Log, TEXT("[Head] Subiendo cabeza: %d bytes en %d partes (v%d)."),
           Blob.Num(), TotalChunks, Version);
}

void APTPlayerState::Server_UploadHeadChunk_Implementation(int32 Version, int32 ChunkIndex,
                                                           int32 TotalChunks, const TArray<uint8>& Data)
{
    if (ChunkIndex == 0) { PendingBlob.Reset(); PendingVersion = Version; PendingChunks = TotalChunks; }
    if (PendingVersion != Version) return; // llegó una parte de una subida vieja: descartar

    PendingBlob.Append(Data);

    if (ChunkIndex < TotalChunks - 1) return; // faltan partes

    HeadBlob    = MoveTemp(PendingBlob);
    HeadVersion = Version;
    PendingBlob.Reset();
    PendingVersion = -1;

    UE_LOG(LogTemp, Log, TEXT("[Head] Cabeza recibida de %s: %d bytes (v%d)."),
           *GetDisplayNameSafe(), HeadBlob.Num(), HeadVersion);

    // El server la aplica a su propia copia del pawn y se la reparte a todos.
    if (APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(GetPawn()))
        Char->ApplyReplicatedHead();
    BroadcastHeadToAll();
}

void APTPlayerState::SendHeadTo(APTPlayerState* Target)
{
    if (!Target || HeadBlob.Num() == 0) return;

    // El dueño de esta cabeza ya la tiene aplicada localmente: no hace falta mandársela.
    if (Target == this) return;

    const int32 TotalChunks = FMath::DivideAndRoundUp(HeadBlob.Num(), HeadChunkBytes);
    for (int32 i = 0; i < TotalChunks; ++i)
    {
        const int32 Offset = i * HeadChunkBytes;
        const int32 Count  = FMath::Min(HeadChunkBytes, HeadBlob.Num() - Offset);
        TArray<uint8> Chunk(HeadBlob.GetData() + Offset, Count);
        // El RPC va en el PlayerState del DESTINATARIO (es el que ese cliente posee), y lleva
        // como parámetro de quién es la cabeza.
        Target->Client_ReceiveHeadChunk(this, HeadVersion, i, TotalChunks, Chunk);
    }
}

void APTPlayerState::BroadcastHeadToAll()
{
    if (!HasAuthority() || HeadBlob.Num() == 0) return;
    const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    if (!GS) return;

    for (APlayerState* PS : GS->PlayerArray)
        if (APTPlayerState* PT = Cast<APTPlayerState>(PS))
            SendHeadTo(PT);
}

void APTPlayerState::Server_RequestAllHeads_Implementation()
{
    // Un cliente entró (a la sala o al Lvl-01) y su mundo está lleno de pawns sin cabeza:
    // se le mandan TODAS las cabezas que el server tenga guardadas.
    const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    if (!GS) return;

    for (APlayerState* PS : GS->PlayerArray)
        if (APTPlayerState* PT = Cast<APTPlayerState>(PS))
            PT->SendHeadTo(this);
}

void APTPlayerState::Client_ReceiveHeadChunk_Implementation(APTPlayerState* Source, int32 Version,
                                                            int32 ChunkIndex, int32 TotalChunks,
                                                            const TArray<uint8>& Data)
{
    if (!Source) return; // el PlayerState de origen todavía no replicó a este cliente

    if (ChunkIndex == 0)
    {
        Source->PendingBlob.Reset();
        Source->PendingVersion = Version;
        Source->PendingChunks  = TotalChunks;
    }
    if (Source->PendingVersion != Version) return;

    Source->PendingBlob.Append(Data);
    if (ChunkIndex < TotalChunks - 1) return;

    Source->HeadBlob         = MoveTemp(Source->PendingBlob);
    Source->LocalHeadVersion = Version;
    Source->PendingBlob.Reset();
    Source->PendingVersion = -1;

    // Aplicarla ya si el pawn existe; si todavía no llegó, el Tick del personaje la aplica
    // igual en cuanto aparezca (compara LocalHeadVersion con la que tiene puesta).
    if (APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(Source->GetPawn()))
        Char->ApplyReplicatedHead();
}

void APTPlayerState::Server_SetDisplayName(const FString& InName)
{
    if (HasAuthority())
    {
        DisplayName = InName;
        // El nombre se setea en PostLogin, justo cuando el PlayerState recién empieza a replicar.
        // Sin forzar la actualización puede tardar hasta el siguiente tick de red en llegarle a
        // los demás clientes, y hasta entonces lo ven en blanco.
        ForceNetUpdate();
        OnRep_DisplayName(); // El host no recibe su propio OnRep; llamarlo manual.
    }
}

void APTPlayerState::Server_ReportDisplayName_Implementation(const FString& InName)
{
    // Solo se acepta si trae algo y todavía no había un nombre real (no pisar el de Steam).
    const FString Clean = InName.TrimStartAndEnd().Left(24);
    if (Clean.IsEmpty()) return;
    if (!DisplayName.IsEmpty() && !DisplayName.StartsWith(TEXT("Player_"))) return;
    Server_SetDisplayName(Clean);
}

void APTPlayerState::Client_HostClosedGame_Implementation()
{
    if (UPTGameInstance* GI = GetWorld() ? Cast<UPTGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
        GI->SetPendingConnectError(PTText::GetStr(TEXT("ERR_HOST_LEFT")));

    // Salida ordenada por decisión propia del cliente: cierra la conexión desde este lado en vez
    // de esperar a que el servidor desaparezca abajo suyo.
    UGameplayStatics::OpenLevel(this, FName("MainMenu"));
}

FString APTPlayerState::GetDisplayNameSafe() const
{
    if (!DisplayName.IsEmpty()) return DisplayName;
    const FString EngineName = GetPlayerName(); // PlayerNamePrivate: lo replica el motor
    if (!EngineName.IsEmpty()) return EngineName;
    return TEXT("Jugador");
}

void APTPlayerState::Server_SetHost(bool bInHost)
{
    if (HasAuthority()) { bIsHost = bInHost; }
}

void APTPlayerState::OnRep_DisplayName() { /* El HUD del lobby lee DisplayName por polling, no necesita reaccionar acá. */ }

void APTPlayerState::Server_SetLanguage_Implementation(const FString& InLanguage)
{
    // Se acepta cualquier idioma que el SERVIDOR conozca (los del CSV): así sumar uno nuevo no
    // pide tocar esto. Si el cliente manda uno que el server no tiene, queda el de referencia.
    const FString Code = InLanguage.Left(2).ToLower(); // "es-AR" → "es"
    Language = PTText::IsLanguageAvailable(Code) ? Code : TEXT("es");
}
