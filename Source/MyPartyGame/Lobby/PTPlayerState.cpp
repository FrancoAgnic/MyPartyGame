// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTPlayerState.h"
#include "PTLobbyCharacter.h"
#include "../PTTextTable.h"
#include "../PTGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Engine/NetConnection.h" // Barrera A: pacing por saturación del canal confiable del receptor
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

namespace
{
    constexpr int32 HeadChunkBytes = 8 * 1024;
    // Barrera B: tope duro de tamaño de una skin (geometría + PNG cabeza + PNG cuerpo, comprimido).
    // Una skin normal está MUY por debajo; esto solo frena datos anómalos (bug de horneado, cliente
    // manipulado) para que una skin gigante NO se transmita a todos y tumbe la sala.
    constexpr int32 MaxHeadBlobBytes = 2 * 1024 * 1024; // 2 MB
    constexpr int32 MaxHeadChunks    = MaxHeadBlobBytes / HeadChunkBytes + 8; // techo de partes válidas
}

// ── Cola de envío trottleado (vive en el PlayerState RECEPTOR para downloads) ────────────────────
void APTPlayerState::EnqueueHeadJob(APTPlayerState* Source, const TSharedPtr<TArray<uint8>>& Data,
                                    int32 Version, bool bToServer)
{
    if (!Data.IsValid() || Data->Num() == 0) return;
    // Reemplazar un trabajo previo equivalente (re-subida, o re-envío de la MISMA fuente: una
    // re-edición supersede lo que estuviera a medio mandar de esa fuente).
    OutHeadJobs.RemoveAll([&](const FHeadSendJob& J)
        { return J.bToServer == bToServer && (bToServer || J.Source.Get() == Source); });

    FHeadSendJob Job;
    Job.Source    = Source;
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
    // 2 chunks/pump (0.05s) = ~40 chunks/s por CLIENTE. Como ahora los downloads salen SERIAL por cliente
    // (una cabeza por vez, este PlayerState = el receptor), el canal confiable de ese cliente nunca se
    // llena aunque haya 8 cabezas para mandarle: se van una atrás de otra.
    // ── Barrera A: pacing adaptativo por saturación de la conexión del receptor ──
    // Un download (Client RPC) viaja por la conexión del cliente dueño de ESTE PlayerState. Si esa
    // conexión está saturada este tick (enlace lento/lageado, común con muchos jugadores), NO encolamos
    // más bunches confiables: se saltea el envío y se reintenta al próximo tick del timer. Así el buffer
    // confiable del canal (OutRec) drena antes de seguir y nunca llega al límite de 256 que corta la
    // conexión → evita el colapso aunque la skin sea grande o el cliente ande mal.
    // (Los uploads MI→server y el host local — sin UNetConnection — no pasan por esta compuerta.)
    if (OutHeadJobs.Num() > 0 && !OutHeadJobs[0].bToServer)
        if (UNetConnection* Conn = GetNetConnection())
            if (!Conn->IsNetReady(/*bLowLatency=*/false))
                return; // canal saturado: esperar al siguiente tick

    const int32 ChunksPerPump = 2;
    int32 Sent = 0;
    while (Sent < ChunksPerPump && OutHeadJobs.Num() > 0)
    {
        FHeadSendJob& J = OutHeadJobs[0];
        // Descartar trabajos inválidos (fuente desconectada, datos nulos, ya completo).
        if (!J.Data.IsValid() || J.Next >= J.Total || (!J.bToServer && !J.Source.IsValid()))
        { OutHeadJobs.RemoveAt(0); continue; }

        const int32 Offset = J.Next * HeadChunkBytes;
        if (Offset >= J.Data->Num()) { OutHeadJobs.RemoveAt(0); continue; }
        const int32 Count = FMath::Min(HeadChunkBytes, J.Data->Num() - Offset);
        TArray<uint8> Chunk(J.Data->GetData() + Offset, Count);

        // Upload: MI cabeza al server (RPC en mí, va por mi conexión). Download: la cabeza de J.Source
        // hacia el cliente dueño de ESTE PlayerState (RPC Client en mí → va por la conexión de este cliente).
        if (J.bToServer)                       Server_UploadHeadChunk(J.Version, J.Next, J.Total, Chunk);
        else if (APTPlayerState* Src = J.Source.Get()) Client_ReceiveHeadChunk(Src, J.Version, J.Next, J.Total, Chunk);
        else { OutHeadJobs.RemoveAt(0); continue; }

        ++J.Next; ++Sent;
        if (J.Next >= J.Total) OutHeadJobs.RemoveAt(0);
    }
    if (OutHeadJobs.Num() == 0) GetWorldTimerManager().ClearTimer(HeadSendTimer);
}

void APTPlayerState::UploadHead(const TArray<uint8>& Blob)
{
    if (Blob.Num() == 0) return;

    // Barrera B: no subir una skin anómala. Si se pasa del tope, se descarta acá (no se transmite a
    // nadie); el dueño se queda con el look default en vez de tumbar la sala.
    if (Blob.Num() > MaxHeadBlobBytes)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Head] Skin DEMASIADO grande (%d bytes > %d): no se sube."),
               Blob.Num(), MaxHeadBlobBytes);
        return;
    }

    HeadBlob = Blob; // copia local: el dueño ve su cabeza sin esperar la vuelta del server
    const int32 Version = HeadVersion + 1;
    // Encolar la subida troceada al server en MI cola (Source=nullptr, bToServer=true).
    EnqueueHeadJob(/*Source=*/nullptr, MakeShared<TArray<uint8>>(Blob), Version, /*bToServer=*/true);
    UE_LOG(LogTemp, Log, TEXT("[Head] Subiendo cabeza: %d bytes en %d partes (v%d), trottleado."),
           Blob.Num(), FMath::DivideAndRoundUp(Blob.Num(), HeadChunkBytes), Version);
}

void APTPlayerState::Server_UploadHeadChunk_Implementation(int32 Version, int32 ChunkIndex,
                                                           int32 TotalChunks, const TArray<uint8>& Data)
{
    // Barrera B: validar el encabezado. Un TotalChunks fuera de rango = subida corrupta/manipulada →
    // descartar sin reservar memoria ni reensamblar.
    if (TotalChunks <= 0 || TotalChunks > MaxHeadChunks)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Head] Subida rechazada de %s: TotalChunks=%d fuera de rango."),
               *GetDisplayNameSafe(), TotalChunks);
        PendingVersion = -1; PendingBlob.Reset();
        return;
    }

    if (ChunkIndex == 0) { PendingBlob.Reset(); PendingVersion = Version; PendingChunks = TotalChunks; }
    if (PendingVersion != Version) return; // llegó una parte de una subida vieja: descartar

    // Barrera B: cortar si el acumulado se pasa del tope (no dejar crecer sin límite ni reenviar a todos).
    if (PendingBlob.Num() + Data.Num() > MaxHeadBlobBytes)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Head] Subida de %s excede el tope (%d bytes): descartada."),
               *GetDisplayNameSafe(), MaxHeadBlobBytes);
        PendingVersion = -1; PendingBlob.Reset();
        return;
    }

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
    // Encolar en la cola del RECEPTOR (Target), marcando que la cabeza es MÍA (Source=this). Así todas
    // las cabezas que van a ese cliente salen SERIAL por su canal y no desbordan el buffer confiable.
    Target->EnqueueHeadJob(/*Source=*/this, MakeShared<TArray<uint8>>(HeadBlob), HeadVersion, /*bToServer=*/false);
}

void APTPlayerState::BroadcastHeadToAll()
{
    if (!HasAuthority() || HeadBlob.Num() == 0) return;
    const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    if (!GS) return;

    // Una sola copia del blob compartida entre todos los receptores (no copiar por jugador). Se encola
    // en la cola de CADA receptor (PT), con Source=this → stream serial por cliente, sin overflow.
    TSharedPtr<TArray<uint8>> Shared = MakeShared<TArray<uint8>>(HeadBlob);
    for (APlayerState* PS : GS->PlayerArray)
        if (APTPlayerState* PT = Cast<APTPlayerState>(PS))
            if (PT != this)
                PT->EnqueueHeadJob(/*Source=*/this, Shared, HeadVersion, /*bToServer=*/false);
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

    // Barrera B (defensivo): encabezado fuera de rango → descartar sin reservar memoria.
    if (TotalChunks <= 0 || TotalChunks > MaxHeadChunks) { Source->PendingVersion = -1; Source->PendingBlob.Reset(); return; }

    if (ChunkIndex == 0)
    {
        Source->PendingBlob.Reset();
        Source->PendingVersion = Version;
        Source->PendingChunks  = TotalChunks;
    }
    if (Source->PendingVersion != Version) return;

    if (Source->PendingBlob.Num() + Data.Num() > MaxHeadBlobBytes) { Source->PendingVersion = -1; Source->PendingBlob.Reset(); return; }

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
