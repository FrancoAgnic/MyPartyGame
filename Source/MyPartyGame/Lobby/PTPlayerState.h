// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 3 — Datos replicados por jugador en el lobby.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PTHeadSaveGame.h" // FPTHeadSection (geometría de la cabeza custom, replicada)
#include "PTPlayerState.generated.h"

UCLASS()
class MYPARTYGAME_API APTPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(ReplicatedUsing=OnRep_DisplayName, BlueprintReadOnly, Category="Lobby")
    FString DisplayName;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby")
    bool bIsHost = false;

    // Listo para arrancar (toggle en el HUD del lobby). El GameMode revisa esto en
    // APTLobbyGameMode::CheckReadyState para el countdown automatico.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby")
    bool bIsReady = false;

    // ── Partida (Sculpturillo) ──────────────────────────────────────────────
    // Si este jugador ya adivinó la palabra del turno actual. El servidor lo resetea
    // al empezar cada turno. Se usa para el HUD ("quién adivinó") y para saber cuándo
    // terminó el turno (todos adivinaron).
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Game")
    bool bHasGuessedThisTurn = false;

    // Puntaje acumulado de la partida (estilo Skribbl). El servidor lo resetea a 0 al
    // empezar cada partida y suma al adivinar / cuando alguien adivina tu escultura.
    // (Se llama GameScore y no Score porque APlayerState ya tiene un Score propio.)
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Game")
    int32 GameScore = 0;

    // Idioma elegido por el jugador ("es", "en", "pt", ...). El cliente lo manda al servidor para
    // que el GameMode le muestre la palabra a adivinar EN SU IDIOMA (ver Server_SetLanguage).
    // Viaja el CÓDIGO y no el índice: si dos jugadores tuvieran CSV distintos, el código sigue
    // significando lo mismo y el índice no.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Game")
    FString Language = TEXT("es");

    /** Índice de idioma del servidor para este jugador (para FPTWordEntry::ForLang). */
    int32 GetLanguageIndex() const;

    // El cliente le dice al servidor su idioma (para la palabra a adivinar por idioma).
    UFUNCTION(Server, Reliable)
    void Server_SetLanguage(const FString& InLanguage);

    // ── Cabeza custom (sincronizada POR PARTES) ─────────────────────────────
    // Geometría final horneada de la cabeza (arcilla + pintura + ojos), serializada y comprimida.
    //
    // IMPORTANTE: esto NO se replica como propiedad. Una cabeza detallada supera, incluso
    // comprimida, el tamaño máximo que Unreal manda en una propiedad replicada, y cuando eso pasa
    // el dato se descarta EN SILENCIO: el server veía las cabezas de los clientes (esas llegaban
    // por RPC) pero los clientes nunca veían la del server. Por eso ahora viaja troceada en RPCs
    // confiables (ver UploadHead / SendHeadTo), que sí soportan cualquier tamaño.
    // El array vive igual en los dos lados: en el server es el original, en el cliente es lo
    // reensamblado.
    TArray<uint8> HeadBlob;

    // Sube +1 cada vez que el jugador confirma una cabeza nueva. Esto SÍ se replica (son 4 bytes):
    // el pawn compara esta versión con la que tiene aplicada y se re-aplica si difiere, y el
    // cliente la usa para saber que le falta pedir una cabeza nueva.
    UPROPERTY(Replicated)
    int32 HeadVersion = 0;

    /** Versión ya reensamblada localmente. Si es < HeadVersion, a este cliente le falta la cabeza. */
    int32 LocalHeadVersion = 0;

    // ── API de sincronización ───────────────────────────────────────────────

    /** [Cliente dueño] Sube la cabeza al server, troceada. El server la guarda y la reparte a todos. */
    void UploadHead(const TArray<uint8>& Blob);

    /** [Cliente] Le pide al server TODAS las cabezas de la partida. Se llama al entrar a la sala y
     *  al entrar al Lvl-01 después del travel — los dos momentos donde un pawn nace sin cabeza. */
    UFUNCTION(Server, Reliable)
    void Server_RequestAllHeads();

    /** [Server] Manda esta cabeza a TODOS los jugadores conectados (incluido el propio host). */
    void BroadcastHeadToAll();

private:
    UFUNCTION(Server, Reliable)
    void Server_UploadHeadChunk(int32 Version, int32 ChunkIndex, int32 TotalChunks, const TArray<uint8>& Data);

    UFUNCTION(Client, Reliable)
    void Client_ReceiveHeadChunk(APTPlayerState* Source, int32 Version, int32 ChunkIndex,
                                 int32 TotalChunks, const TArray<uint8>& Data);

    /** [Server] Manda MI cabeza a un jugador concreto, troceada. */
    void SendHeadTo(APTPlayerState* Target);

    /** Buffer de reensamblado (server: lo que sube el dueño; cliente: lo que baja del server). */
    TArray<uint8> PendingBlob;
    int32         PendingVersion = -1;
    int32         PendingChunks  = 0;

public:

    // Llamar solo desde el servidor (HasAuthority).
    void Server_SetDisplayName(const FString& InName);
    void Server_SetHost(bool bInHost);

    /** Nombre a MOSTRAR, con red de seguridad. Todo lo que pinte un nombre en la UI debe usar
     *  esto y NO leer DisplayName directo: si por lo que sea DisplayName todavía no llegó a este
     *  cliente, se cae a GetPlayerName() (que replica el motor por su cuenta) y en última
     *  instancia a un texto genérico — nunca un nombre vacío. */
    UFUNCTION(BlueprintPure, Category="Lobby")
    FString GetDisplayNameSafe() const;

    /** El cliente le manda su nombre al servidor. Backstop por si el "?Name=" de la URL de travel
     *  se perdió (contraseña mal formada, reconexión, migración de host): sin esto el jugador
     *  queda como "Player_2" para todos. */
    UFUNCTION(Server, Reliable)
    void Server_ReportDisplayName(const FString& InName);

    /** [Server→cliente] El anfitrión cerró la partida: volvé al menú por las buenas.
     *  Se manda ANTES de destruir la sesión; si no, el cliente se queda con una conexión muerta
     *  y el juego se le cae. */
    UFUNCTION(Client, Reliable)
    void Client_HostClosedGame();

    UFUNCTION() void OnRep_DisplayName();

    // El seamless travel Lobby→Lvl-01 puede recrear el PlayerState; sin copiar estos
    // campos a mano se perderían (nombre/host quedarían vacíos en el juego).
    virtual void CopyProperties(APlayerState* NewPlayerState) override;
};
