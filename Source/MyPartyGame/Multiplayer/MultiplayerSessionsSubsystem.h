// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 1 — Núcleo de red del template Party Game.
// INVARIANTE: ningún archivo fuera de esta clase toca IOnlineSubsystem ni IOnlineSession.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "MultiplayerSessionsSubsystem.generated.h"

// -------------------------------------------------------------------
// Delegates propios hacia la UI (NO exponer los de OSS hacia afuera)
// -------------------------------------------------------------------
DECLARE_MULTICAST_DELEGATE_OneParam(FPTOnLoginComplete,          bool /*bWasSuccessful*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPTOnCreateSessionComplete,  bool /*bWasSuccessful*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FPTOnFindSessionsComplete,
    const TArray<FOnlineSessionSearchResult>& /*Results*/, bool /*bWasSuccessful*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPTOnJoinSessionComplete,    EOnJoinSessionCompleteResult::Type /*Result*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPTOnDestroySessionComplete, bool /*bWasSuccessful*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPTOnStartSessionComplete,   bool /*bWasSuccessful*/);
// Se aceptó una invitación de Steam (overlay o "Unirse a partida"). La UI del menú lo escucha
// para procesar el join en cola. Ver ProcessPendingInvite().
DECLARE_MULTICAST_DELEGATE(FPTOnInviteAccepted);
// La lista de amigos terminó de leerse (ReadFriends). La UI relee GetFriends() y repinta.
DECLARE_MULTICAST_DELEGATE(FPTOnFriendsListUpdated);
// LLEGÓ una invitación de un amigo (juego abierto). El GameInstance muestra el popup Aceptar/Rechazar.
// Param: nombre del que invita.
DECLARE_MULTICAST_DELEGATE_OneParam(FPTOnInviteReceived, const FString& /*FromName*/);
// Steam pide UNIRSE a la partida de un amigo (aceptó "Unirse"/invitación con rich presence "connect",
// o el juego se lanzó desde ahí). Param: SteamID del host al que hay que viajar. El GameInstance viaja.
DECLARE_MULTICAST_DELEGATE_OneParam(FPTOnJoinRequestedById, const FString& /*HostSteamId*/);
// No se pudo invitar (p.ej. Steam offline). Param: mensaje para mostrar. La UI lo escucha.
DECLARE_MULTICAST_DELEGATE_OneParam(FPTOnInviteWarning, const FString& /*Msg*/);

// -------------------------------------------------------------------
// Info de un amigo de Steam para la UI (sin exponer tipos de OSS hacia afuera).
// -------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FPTFriendInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Friends") FString DisplayName;
    // String del FUniqueNetId — se usa para invitar (InviteFriend). Opaco para la UI.
    UPROPERTY(BlueprintReadOnly, Category="Friends") FString UserId;
    UPROPERTY(BlueprintReadOnly, Category="Friends") bool bOnline          = false;
    UPROPERTY(BlueprintReadOnly, Category="Friends") bool bPlayingThisGame = false;
    UPROPERTY(BlueprintReadOnly, Category="Friends") bool bJoinable        = false;
};

// -------------------------------------------------------------------

UCLASS()
class MYPARTYGAME_API UMultiplayerSessionsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UMultiplayerSessionsSubsystem();

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ------------------------------------------------------------------
    // API pública — la UI llama a esto, NUNCA a OSS directamente
    // ------------------------------------------------------------------
    void Login();

    // Rango de jugadores que el template permite por sala (UI: Host Game "Range: 2-10").
    static constexpr int32 MinPlayersAllowed = 2;
    static constexpr int32 MaxPlayersAllowed = 10;

    // Visibilidad de la sesión (reemplaza al viejo sistema de código):
    //   bFriendsOnly=false → PÚBLICA: aparece en la pestaña "Públicas" del Find Game; cualquiera entra.
    //   bFriendsOnly=true  → PRIVADA SOLO AMIGOS: aparece solo en la pestaña "Amigos" (para tus amigos)
    //                        y ellos entran directo (sin request). No lleva contraseña.
    // El nombre de la sala no se pasa: se toma del nombre de Steam del host (GetLocalPlayerDisplayName).
    void CreateSession(int32 NumPublicConnections, bool bFriendsOnly);
    void FindSessions(int32 MaxSearchResults);
    void JoinSession(const FOnlineSessionSearchResult& SessionResult, const FString& Password);

    void DestroySession();
    void StartSession();

    // El HOST cambia la visibilidad de la sesión YA CREADA (lobby → opciones): re-publica la sesión.
    // true = privada solo amigos, false = pública. Solo tiene efecto en el host (tiene los settings).
    void SetSessionFriendsOnly(bool bFriendsOnly);
    // Visibilidad actual de la sesión propia (para inicializar el toggle del host). Lee el estado REAL
    // de la sesión publicada (KEY_HAS_PASSWORD = cómo se creó la room); cae al pendiente si no hay sesión.
    bool IsSessionFriendsOnly() const;

    // Destruye una sesión que haya quedado REGISTRADA localmente de una partida anterior.
    // Llamar al mostrar el menú principal: ahí nunca debería haber sesión activa.
    // Sin esto: (a) el host que volvió al menú deja su sala anunciada = "sesión fantasma" que
    // aparece en Find y no se puede joinear, y (b) con NAME_GameSession todavía ocupado, unirse a
    // OTRA sala falla → quedaba todo trabado hasta reiniciar el juego.
    void CleanupStaleSession();

    // Helper para que el PlayerController viaje tras crear/unirse (Fases 2-3).
    // Devuelve la dirección de conexión resuelta (ej: "192.168.1.5:7777").
    bool GetResolvedConnectString(FString& OutConnectString) const;

    // Getters de estado (para UI / PlayerController)
    FString GetPendingJoinPassword()    const { return PendingJoinPassword;  }
    FString GetPendingHostPassword()    const { return PendingPassword;      }
    FString GetPendingSessionName()     const { return PendingSessionName;   }
    int32   GetPendingMaxPlayers()      const { return PendingNumPublicConnections; }

    // Nombre de Steam del jugador local — la UI lo manda como "?Name=" al viajar (host y clientes)
    // para que PTLobbyGameMode lo lea de PlayerState->GetPlayerName() en vez de un placeholder.
    FString GetLocalPlayerDisplayName() const;
    bool    IsLoggedIn()                const { return bIsLoggedIn;          }

    // Helpers estáticos para leer settings de un resultado de búsqueda
    static FString GetServerNameFromResult(const FOnlineSessionSearchResult& Result);
    static bool    GetHasPasswordFromResult(const FOnlineSessionSearchResult& Result);
    // true si la sesión es PRIVADA SOLO AMIGOS (la usa el Find Game para separar las 2 pestañas).
    static bool    GetIsFriendsOnlyFromResult(const FOnlineSessionSearchResult& Result);
    // SteamID (string) del host/dueño de la sesión — para saber si es amigo nuestro (pestaña Amigos).
    static FString GetOwnerSteamIdFromResult(const FOnlineSessionSearchResult& Result);
    // true si ese SteamID está en nuestra lista de amigos (requiere ReadFriends previo).
    bool           IsFriendSteamId(const FString& SteamId) const;
    // Jugadores actuales anunciados por el host (clave propia CUR_PLAYERS). Devuelve -1 si el resultado
    // no la trae (ahí el que llama cae al fallback NumPublicConnections - NumOpenPublicConnections).
    static int32   GetCurrentPlayersFromResult(const FOnlineSessionSearchResult& Result);

    // El HOST actualiza el nº de jugadores anunciado (lo llama el lobby GameMode al entrar/salir alguien).
    // Re-publica la sesión (UpdateSession) para que la lista de "buscar partidas" muestre el número real.
    void UpdateAdvertisedPlayerCount(int32 CurrentPlayers);

    // AGameModeBase::CanServerTravel rechaza cualquier URL de travel que contenga '%', ':' o '\'
    // (ver GameModeBase.cpp) — por eso NO se puede URL-encodear el nombre de Steam (UrlEncode
    // mete '%XX' para tildes/símbolos/emojis, y eso tira el travel entero). En vez de codificar,
    // se descartan los caracteres no seguros directamente; el motor tampoco decodifica '%XX' del
    // lado de PlayerState->GetPlayerName(), así que encodear no serviría de nada de todos modos.
    static FString SanitizeNameForTravelURL(const FString& Name);

    // Fase 4 — Compara un intento de contraseña contra la del host.
    // Devuelve true si coincide, o si el host no puso contraseña.
    bool DoesHostPasswordMatch(const FString& Attempt) const;

    // ------------------------------------------------------------------
    // Invitaciones nativas de Steam (conviven con el código y las públicas)
    // ------------------------------------------------------------------
    // Abre el panel "invitar amigos" del overlay de Steam para la sesión actual (camino simple,
    // no necesita lista propia). Útil como botón "Invitar por Steam".
    void ShowSteamInviteOverlay();

    // Invita a un amigo puntual a la sesión actual, por su UserId (el de FPTFriendInfo).
    void InviteFriend(const FString& FriendUserId);

    // Procesa una invitación aceptada que quedó EN COLA (join + travel lo hace quien escuche
    // OnJoinSessionComplete). La llama la UI del menú: en su NativeConstruct (caso "lanzado desde
    // la invitación") y al recibir OnInviteAccepted (caso "juego ya abierto en el menú").
    void ProcessPendingInvite();
    bool HasPendingInvite() const { return bHasPendingInvite; }

    // Popup de invitación (F3): al aceptar/rechazar el popup que muestra el GameInstance.
    // Aceptar → se une a la partida del que invitó (join + travel). Rechazar → descarta.
    void AcceptReceivedInvite();
    void DeclineReceivedInvite();

    // Pide a Steam la lista de amigos; al terminar dispara OnFriendsListUpdated y GetFriends()
    // queda actualizado. Repetir para refrescar estados (online / jugando / joinable).
    void ReadFriends();
    const TArray<FPTFriendInfo>& GetFriends() const { return CachedFriends; }

    // ------------------------------------------------------------------
    // Delegates hacia la UI — suscribirse a estos, no a los de OSS
    // ------------------------------------------------------------------
    FPTOnLoginComplete           OnLoginComplete;
    FPTOnCreateSessionComplete   OnCreateSessionComplete;
    FPTOnFindSessionsComplete    OnFindSessionsComplete;
    FPTOnJoinSessionComplete     OnJoinSessionComplete;
    FPTOnDestroySessionComplete  OnDestroySessionComplete;
    FPTOnStartSessionComplete    OnStartSessionComplete;
    FPTOnInviteAccepted          OnInviteAccepted;
    FPTOnFriendsListUpdated      OnFriendsListUpdated;
    FPTOnInviteReceived          OnInviteReceived;
    FPTOnJoinRequestedById       OnJoinRequestedById;
    FPTOnInviteWarning           OnInviteWarning;

    // Rich presence "connect" de Steam: para que las invitaciones/"Unirse a partida" lleven A QUÉ
    // partida unirse (el host), no un genérico "vení a jugar". Se setea al hostear/unirse; se limpia al salir.
    void SetJoinPresence(const FString& HostSteamId);
    void ClearJoinPresence();
    // SteamID del jugador local (host) como string; vacío si no hay Steam.
    FString GetLocalSteamId() const;
    // true si Steam está conectado y el jugador local NO está en estado "Desconectado/Offline".
    // Si es false, invitar no sirve → conviene avisar antes de enviar.
    bool IsSteamOnline() const;
    // AppID de Steam con el que se lanzó el juego (base 5114580 vs Playtest 5115870). 0 si no hay Steam.
    // Sirve para distinguir en runtime si estamos corriendo la build del Playtest.
    uint32 GetSteamAppId() const;
    // Al arrancar: si el juego se lanzó desde una invitación (command line con nuestro token), devuelve
    // el SteamID del host a unirse; vacío si no. Lo consulta el GameInstance tras el login.
    FString ConsumeLaunchJoinHostId();

protected:
    // Callbacks que OSS invoca internamente
    void HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful,
                             const FUniqueNetId& UserId, const FString& Error);
    void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void HandleFindSessionsComplete(bool bWasSuccessful);
    void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
    void HandleStartSessionComplete(FName SessionName, bool bWasSuccessful);
    // Steam avisa que se aceptó una invitación (overlay) o "Unirse a partida" de la lista de amigos.
    void HandleSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId,
        FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
    // Steam avisa que LLEGÓ una invitación (juego abierto) → mostramos el popup.
    void HandleSessionInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FromId,
        const FString& AppId, const FOnlineSessionSearchResult& InviteResult);
    // La lectura de la lista de amigos terminó.
    void HandleReadFriendsComplete(int32 LocalUserNum, bool bWasSuccessful,
        const FString& ListName, const FString& ErrorStr);

private:
    // Interfaz de sesiones (única referencia a OSS en todo el proyecto)
    IOnlineSessionPtr SessionInterface;

    // Búsqueda y settings en curso
    TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
    TSharedPtr<FOnlineSessionSearch>   LastSessionSearch;

    // Handles de delegates OSS — se guardan y limpian en cada callback
    FDelegateHandle LoginCompleteHandle;
    FDelegateHandle CreateSessionCompleteHandle;
    FDelegateHandle FindSessionsCompleteHandle;
    FDelegateHandle JoinSessionCompleteHandle;
    FDelegateHandle DestroySessionCompleteHandle;
    FDelegateHandle StartSessionCompleteHandle;
    FDelegateHandle SessionInviteAcceptedHandle;
    FDelegateHandle SessionInviteReceivedHandle;

    // Invitación aceptada que espera a que la UI del menú la procese (join+travel).
    bool bHasPendingInvite = false;
    TSharedPtr<FOnlineSessionSearchResult> PendingInviteResult;
    // Invitación RECIBIDA (popup) a la espera de Aceptar/Rechazar.
    TSharedPtr<FOnlineSessionSearchResult> ReceivedInviteResult;
    // Si la invitación llegó como LobbyInvite_t (raw Steam), guardamos la lobby a unirse al Aceptar.
    uint64 ReceivedInviteLobbyId = 0;

    // Amigos leídos por ReadFriends (para la UI) + mapa UserId→net id para poder invitar.
    TArray<FPTFriendInfo> CachedFriends;
    TMap<FString, FUniqueNetIdPtr> FriendIdMap;

    // Estado interno
    bool    bIsLoggedIn               = false;
    bool    bCreateSessionOnDestroy   = false;  // si había sesión vieja, destruir y recrear
    int32   PendingNumPublicConnections = 0;
    bool    bPendingFriendsOnly       = false; // visibilidad elegida al crear (true = privada solo amigos)
    FString PendingSessionName;
    FString PendingPassword;        // (obsoleto/inerte: siempre vacío desde que se sacó el código)
    FString PendingJoinPassword;    // (obsoleto/inerte)

    // Claves de settings de sesión (definidas como FName para evitar typos)
    static const FName KEY_SERVER_NAME;
    static const FName KEY_HAS_PASSWORD; // ahora = "privada solo amigos" (true) vs pública (false)
    static const FName KEY_MATCH_TYPE;
    static const FName KEY_LOBBY_ID;     // ID interno de lobby Steam para join directo worldwide
    static const FName KEY_CUR_PLAYERS;  // jugadores actuales (el host la actualiza al entrar/salir)
    static const FName KEY_OWNER_ID;     // SteamID del host/dueño (para filtrar la pestaña Amigos)

    // URL de SteamSockets calculada por FPTSteamDirectJoin tras join directo worldwide.
    FString WorldwideConnectURL;
#if PT_WITH_STEAM
    // Punteros opacos (raw) — TUniquePtr requeriría tipo completo en este header,
    // pero UHT genera código que no puede incluir steam_api.h.
    struct FPTSteamWorldwideSearch* WorldwideSearch = nullptr;
    struct FPTSteamDirectJoin*      WorldwideJoin   = nullptr;
    struct FPTSteamJoinListener*    JoinListener    = nullptr; // escucha "Unirse a partida" de Steam
#endif

    // Helpers privados
    IOnlineSessionPtr GetSessions() const;
    bool IsUsingNullSubsystem() const;                   // true → LAN (NULL subsystem)
    static FString HashPassword(const FString& Plain);   // (obsoleto/inerte; ya no hay contraseñas)
    void InternalCreateSession();                        // crea de verdad tras login/destroy
    void InternalFindSessions(int32 MaxSearchResults);    // Find de sesiones (la UI filtra por pestaña)
    void InternalJoinByLobbyId(uint64 LobbyId);          // Join directo worldwide (sin SessionInterface)

#if !UE_BUILD_SHIPPING
    // ------------------------------------------------------------------
    // Comandos de consola de debug (solo en builds de desarrollo / editor)
    // ------------------------------------------------------------------
    void RegisterDebugCommands();
    void UnregisterDebugCommands();

    IConsoleCommand* DebugCmd_Login          = nullptr;
    IConsoleCommand* DebugCmd_CreateSession  = nullptr;
    IConsoleCommand* DebugCmd_FindSessions   = nullptr;
    IConsoleCommand* DebugCmd_JoinSession    = nullptr;
    IConsoleCommand* DebugCmd_DestroySession = nullptr;
    IConsoleCommand* DebugCmd_InvitePopup    = nullptr;

    // Caché de resultados para PT.Debug.Join [índice]
    TArray<FOnlineSessionSearchResult> CachedSearchResults;
#endif
};
