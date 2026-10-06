// Modo LOCAL (party estilo Jackbox): una sola PC conectada a la TV, un joystick que se pasan los
// jugadores para esculpir, y cada jugador entra desde el NAVEGADOR de su celular para ver la palabra
// (el escultor) o adivinar (el resto).
//
// Este subsistema vive en el GameInstance (sobrevive al OpenLevel menú → Lvl-01):
//   · levanta el servidor HTTP/WebSocket (FPTLocalPartyServer) en la red local,
//   · lleva la lista de jugadores del celular (nombre, idioma, color, token para reconectar),
//   · le pide al GameMode de la partida que cree/borre sus PlayerStates (sin controller),
//   · manda a cada celular su estado personalizado (fase, máscara, palabra secreta si esculpe...).
//
// Toda la lógica de turnos/puntaje sigue en APTSculptGameMode: acá solo se traduce celular ↔ partida.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "PTSculptGameState.h"
#include "PTPartyTransport.h" // TUniquePtr<IPTPartyTransport> necesita el tipo completo
#include "PTLocalPartySubsystem.generated.h"

class APTSculptGameMode;
class APTPlayerState;
class FJsonObject;

// Un jugador del celular. Sobrevive a desconexiones cortas (el celular se bloquea, cambia de app...):
// al volver, el celular manda su token y recupera el mismo jugador (mismo puntaje).
USTRUCT(BlueprintType)
struct FPTPhonePlayer
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32        Id = 0;
    UPROPERTY(BlueprintReadOnly) FString      Name;
    UPROPERTY(BlueprintReadOnly) FString      Language = TEXT("es");
    UPROPERTY(BlueprintReadOnly) FLinearColor Color = FLinearColor::White;
    UPROPERTY(BlueprintReadOnly) bool         bOnline = false;
    UPROPERTY(BlueprintReadOnly) bool         bVip = false; // el "anfitrión": empieza la partida / jugar de nuevo

    FString Token;
    int32   ConnId = INDEX_NONE;
    double  OfflineSince = 0.0;
    TWeakObjectPtr<APTPlayerState> PlayerState;
    FString LastSentState; // para no reenviar el mismo estado
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPTOnLocalPartyChanged);

UCLASS()
class MYPARTYGAME_API UPTLocalPartySubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Servidor ────────────────────────────────────────────────────────────
    // Levanta el transporte (idempotente): servidor local en la WiFi o, en modo ONLINE, la conexión al
    // relay (ver Tools/Relay). false si no se pudo (puerto ocupado / falta configurar el relay).
    bool StartServer();

    // ── Modo online (relay) ──
    UFUNCTION(BlueprintPure, Category="LocalParty") bool IsOnline() const { return bOnlineTransport; }
    /** Código de la sala online ("KQZT"); vacío mientras conecta o en modo local. */
    UFUNCTION(BlueprintPure, Category="LocalParty") FString GetRoomCode() const;
    /** "play.tudominio.com" (lo que se escribe en el celular, sin https). */
    UFUNCTION(BlueprintPure, Category="LocalParty") FString GetPublicHost() const;
    /** Texto del último error del relay (para mostrar en la TV). */
    FString GetOnlineError() const;
    void StopServer();
    UFUNCTION(BlueprintPure, Category="LocalParty") bool IsServerRunning() const;

    // URL que los celulares tienen que abrir (http://192.168.x.x:8787). Vacío si no hay red.
    UFUNCTION(BlueprintPure, Category="LocalParty") FString GetJoinUrl() const;

    // Puerto TCP del servidor (los celulares lo ven en la URL). 8787 para no chocar con el 7777 del juego.
    int32 Port = 8787;
    // Máximo de jugadores de celular.
    int32 MaxPlayers = 12;
    int32 MaxPlayersOnline = 24; // online (relay): amigos a distancia / audiencia chica
    // Segundos que se espera a un celular desconectado antes de sacarlo de la partida.
    float OfflineGraceInGame  = 90.f;
    float OfflineGraceInLobby = 20.f;

    // ── Partida ─────────────────────────────────────────────────────────────
    // El GameMode de Lvl-01 se registra al arrancar en modo local (y se desregistra al terminar).
    void BindGameMode(APTSculptGameMode* GM);
    void UnbindGameMode(APTSculptGameMode* GM);

    UFUNCTION(BlueprintPure, Category="LocalParty") const TArray<FPTPhonePlayer>& GetPlayers() const { return Players; }
    UFUNCTION(BlueprintPure, Category="LocalParty") FString GetVipName() const;
    const FPTPhonePlayer* FindByPlayerState(const APlayerState* PS) const;

    // Se dispara cuando entra/sale/reconecta alguien (la TV refresca su lista).
    UPROPERTY(BlueprintAssignable, Category="LocalParty") FPTOnLocalPartyChanged OnPlayersChanged;

    // Avisos PRIVADOS a un celular (los llama el GameMode).
    void NotifyGuessed(const APTPlayerState* PS, const FString& Word, int32 Points);
    void NotifyCloseGuess(const APTPlayerState* PS);
    void NotifyTurnStarted(const APTPlayerState* Sculptor); // vibra: "te toca, agarrá el joystick"

    // Olvidar a todos (al salir del modo local).
    void ClearPlayers();

    // ── FTickableGameObject ─────────────────────────────────────────────────
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UPTLocalPartySubsystem, STATGROUP_Tickables); }
    virtual bool IsTickable() const override { return !IsTemplate(); }
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }

private:
    TUniquePtr<IPTPartyTransport> Server;
    bool bOnlineTransport = false;
    int32 LocalPort = 0;
    TArray<FPTPhonePlayer> Players;
    TWeakObjectPtr<APTSculptGameMode> GameMode;
    TWeakObjectPtr<APTSculptGameState> BoundGameState; // para el OnChatLine
    int32  NextPlayerId = 1;
    float  StateAccum = 0.f;
    FString CachedLanIp;

    FPTPhonePlayer* FindByConn(int32 ConnId);
    FPTPhonePlayer* FindById(int32 Id);

    void HandleConnected(int32 ConnId);
    void HandleDisconnected(int32 ConnId);
    void HandleMessage(int32 ConnId, const FString& Text);
    void HandleJoin(int32 ConnId, const TSharedPtr<FJsonObject>& Msg);

    void EnsurePlayerState(FPTPhonePlayer& P);
    void RemovePlayer(int32 Id);
    void EnsureVip();
    void PushStates(bool bForce);
    FString BuildStateFor(const FPTPhonePlayer& P) const;
    void SendTo(const FPTPhonePlayer& P, const TSharedRef<FJsonObject>& Obj);
    void SendToConn(int32 ConnId, const TSharedRef<FJsonObject>& Obj);
    void BroadcastJson(const TSharedRef<FJsonObject>& Obj);

    UFUNCTION() void OnChatLine(const FString& Name, const FString& Message, EPTChatType Type);
    void OnPostLoadMap(UWorld* World);
    FDelegateHandle PostLoadMapHandle;
};
