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
    DOREPLIFETIME(APTPlayerState, bIsDevSpectator);
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
        PT->bIsDevSpectator = bIsDevSpectator; // si esculp/espectás en el lobby, seguís igual en el juego
        // bHasGuessedThisTurn NO se copia: es estado por-turno, arranca en false en el juego.
    }
}

// ── Sincronización de la cabeza, troceada ────────────────────────────────────────────────────
// Va por RPC y no por propiedad replicada porque el blob puede pasarse del tamaño máximo de una
// propiedad, y en ese caso Unreal lo tira sin avisar. Los RPC confiables se parten en varios
// paquetes solos y llegan en orden, así que sirven para cualquier tamaño.

namespace { constexpr int32 HeadChunkBytes = 8 * 1024; }

// ── Cola de envío trottleado ───────────────────────────────────────────────────
void APTPlayerState::EnqueueHeadJob(APTPlayerState* Target, const TSharedPtr<TArray<uint8>>& Data,
                                    int32 Version, bool bToServer)
{
    if (!Data.IsValid() || Data->Num() == 0) return;
    // Reemplazar un trabajo previo al MISMO destino (re-subida / re-envío supersede al anterior).
    OutHeadJobs.RemoveAll([&](const FHeadSendJob& J)
        { return J.bToServer == bToServer && (bToServer || J.Target.Get() == Target); });

    FHeadSendJob Job;
    Job.Target    = Target;
    Job.Data      = Data;
    Job.Version   = Version;
    Job.Next      = 0;
    Job.Total     = FMath::DivideAndRoundUp(Data->Num(), HeadChunkBytes);
    Job.bToServer = bToServer;
    OutHeadJobs.Add(MoveTemp(Job));
    EnsureHeadPump();
}

void APTPlayerState::EnsureHeadPump()
{
    if (GetWorld() && !GetWorldTimerManager().IsTimerActive(HeadSendTimer))
        GetWorldTimerManager().SetTimer(HeadSendTimer, this, &APTPlayerState::PumpHeadSend, 0.05f, /*loop=*/true);
}

void APTPlayerState::PumpHeadSend()
{
    const int32 ChunksPerPump = 3; // ~60 chunks/s: nunca acumula cerca del límite del buffer confiable
    int32 Sent = 0;
    while (Sent < ChunksPerPump && OutHeadJobs.Num() > 0)
    {
        FHeadSendJob& J = OutHeadJobs[0];
        // Descartar trabajos inválidos (destino desconectado, datos nulos, ya completo).
        if (!J.Data.IsValid() || J.Next >= J.Total || (!J.bToServer && !J.Target.IsValid()))
        { OutHeadJobs.RemoveAt(0); continue; }

        const int32 Offset = J.Next * HeadChunkBytes;
        if (Offset >= J.Data->Num()) { OutHeadJobs.RemoveAt(0); continue; }
        const int32 Count = FMath::Min(HeadChunkBytes, J.Data->Num() - Offset);
        TArray<uint8> Chunk(J.Data->GetData() + Offset, Count);

        if (J.bToServer) Server_UploadHeadChunk(J.Version, J.Next, J.Total, Chunk);
        else             J.Target->Client_ReceiveHeadChunk(this, J.Version, J.Next, J.Total, Chunk);

        ++J.Next; ++Sent;
        if (J.Next >= J.Total) OutHeadJobs.RemoveAt(0);
    }
    if (OutHeadJobs.Num() == 0) GetWorldTimerManager().ClearTimer(HeadSendTimer);
}

void APTPlayerState::UploadHead(const TArray<uint8>& Blob)
{
    if (Blob.Num() == 0) return;

    HeadBlob = Blob; // copia local: el dueño ve su cabeza sin esperar la vuelta del server
    const int32 Version = HeadVersion + 1;
    // Encolar la subida troceada al server (se manda de a poco en PumpHeadSend).
    EnqueueHeadJob(nullptr, MakeShared<TArray<uint8>>(Blob), Version, /*bToServer=*/true);
    UE_LOG(LogTemp, Log, TEXT("[Head] Subiendo cabeza: %d bytes en %d partes (v%d), trottleado."),
           Blob.Num(), FMath::DivideAndRoundUp(Blob.Num(), HeadChunkBytes), Version);
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
    if (!Target || Target == this || HeadBlob.Num() == 0) return;
    // Encolar (se manda troceado y paceado en PumpHeadSend). El RPC final va en el PlayerState del
    // DESTINATARIO y lleva de quién es la cabeza.
    EnqueueHeadJob(Target, MakeShared<TArray<uint8>>(HeadBlob), HeadVersion, /*bToServer=*/false);
}

void APTPlayerState::BroadcastHeadToAll()
{
    if (!HasAuthority() || HeadBlob.Num() == 0) return;
    const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    if (!GS) return;

    // Una sola copia del blob compartida entre todos los destinos (no copiar por jugador).
    TSharedPtr<TArray<uint8>> Shared = MakeShared<TArray<uint8>>(HeadBlob);
    for (APlayerState* PS : GS->PlayerArray)
        if (APTPlayerState* PT = Cast<APTPlayerState>(PS))
            if (PT != this)
                EnqueueHeadJob(PT, Shared, HeadVersion, /*bToServer=*/false);
}

void APTPlayerState::RefreshHeadsIfMissing()
{
    // Solo tiene sentido en CLIENTES: el host tiene todas las cabezas por autoridad.
    if (HasAuthority()) return;
    UWorld* W = GetWorld();
    const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
    if (!GS) return;

    // 1) Si ya hay una cabeza ENSAMBLÁNDOSE, no pedir nada (no amontonar re-envíos → evita el overflow
    //    confiable que cortaba la conexión en loop).
    for (APlayerState* PS : GS->PlayerArray)
        if (const APTPlayerState* PT = Cast<APTPlayerState>(PS))
            if (PT->PendingVersion != -1) return;

    // 2) Cooldown: como mucho un re-pedido cada HeadRequestCooldown segundos.
    const double Now = W->GetTimeSeconds();
    if (Now - LastHeadRequestTime < HeadRequestCooldown) return;

    bool bMissing = false;
    for (APlayerState* PS : GS->PlayerArray)
    {
        const APTPlayerState* PT = Cast<APTPlayerState>(PS);
        if (!PT || PT == this) continue;
        // Falta si: el server dice que tiene cabeza (HeadVersion>0) pero a mí no me llegaron los bytes,
        // o me llegó una versión vieja (re-editó y me perdí el broadcast).
        if (PT->HeadVersion > 0 && (PT->HeadBlob.Num() == 0 || PT->LocalHeadVersion < PT->HeadVersion))
        { bMissing = true; break; }
    }
    if (bMissing) { Server_RequestAllHeads(); LastHeadRequestTime = Now; }
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

void APTPlayerState::OnRep_DevSpectator()
{
    // El personaje se oculta/aparece según el flag. El propio pawn del jugador lo aplica por polling
    // (ver APTLobbyCharacter), así que acá no hace falta más: es solo el gancho de replicación.
}

void APTPlayerState::Server_SetLanguage_Implementation(const FString& InLanguage)
{
    // Se acepta cualquier idioma que el SERVIDOR conozca (los del CSV): así sumar uno nuevo no
    // pide tocar esto. Si el cliente manda uno que el server no tiene, queda el de referencia.
    const FString Code = InLanguage.Left(2).ToLower(); // "es-AR" → "es"
    Language = PTText::IsLanguageAvailable(Code) ? Code : TEXT("es");
}
