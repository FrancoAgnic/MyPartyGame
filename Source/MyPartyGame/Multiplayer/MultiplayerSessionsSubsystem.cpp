// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 1 — Implementación de UMultiplayerSessionsSubsystem.
// INVARIANTE: único archivo del proyecto que toca IOnlineSubsystem / IOnlineSession.

#include "MultiplayerSessionsSubsystem.h"
#include "PTSteamWorldwideSearch.h"             // FPTSteamWorldwideSearch / FPTSteamDirectJoin
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"              // FOnlineSessionSettings
#include "Online/OnlineSessionNames.h"          // SEARCH_LOBBIES
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineFriendsInterface.h"  // IOnlineFriends / FOnlineFriend / EFriendsLists
#include "Interfaces/OnlineExternalUIInterface.h" // ShowInviteUI (overlay de Steam)
#include "Interfaces/OnlinePresenceInterface.h" // FOnlineUserPresence
#include "Misc/SecureHash.h"                    // FMD5

DEFINE_LOG_CATEGORY_STATIC(LogPTSessions, Log, All);

// --- Claves de settings de sesión ---
const FName UMultiplayerSessionsSubsystem::KEY_SERVER_NAME  = FName("SERVER_NAME");
const FName UMultiplayerSessionsSubsystem::KEY_HAS_PASSWORD = FName("HAS_PASSWORD"); // = privada solo amigos
const FName UMultiplayerSessionsSubsystem::KEY_CUR_PLAYERS  = FName("CUR_PLAYERS");
const FName UMultiplayerSessionsSubsystem::KEY_MATCH_TYPE   = FName("MATCH_TYPE");
const FName UMultiplayerSessionsSubsystem::KEY_LOBBY_ID     = FName("PT_LOBBY_ID");
const FName UMultiplayerSessionsSubsystem::KEY_OWNER_ID     = FName("PT_OWNER_ID");

// ==========================================================================
// Lifecycle
// ==========================================================================

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem() {}

void UMultiplayerSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld()))
    {
        SessionInterface = Subsystem->GetSessionInterface();
        UE_LOG(LogPTSessions, Log, TEXT("OSS activo: %s"), *Subsystem->GetSubsystemName().ToString());

        // Invitaciones de Steam: registrar YA (en el arranque) el delegate de "invitación aceptada".
        // Si el juego se LANZA desde una invitación, Steam dispara esto apenas termina de iniciar,
        // así que tiene que estar registrado antes de que aparezca el menú. Ver ProcessPendingInvite.
        if (SessionInterface.IsValid())
        {
            SessionInviteAcceptedHandle = SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(
                FOnSessionUserInviteAcceptedDelegate::CreateUObject(
                    this, &UMultiplayerSessionsSubsystem::HandleSessionUserInviteAccepted));

            // Invitación RECIBIDA con el juego abierto → popup Aceptar/Rechazar (F3).
            SessionInviteReceivedHandle = SessionInterface->AddOnSessionInviteReceivedDelegate_Handle(
                FOnSessionInviteReceivedDelegate::CreateUObject(
                    this, &UMultiplayerSessionsSubsystem::HandleSessionInviteReceived));
        }
    }
    else
    {
        UE_LOG(LogPTSessions, Warning, TEXT("No se encontró OnlineSubsystem. Verificar plugins y DefaultEngine.ini."));
    }

#if !UE_BUILD_SHIPPING
    RegisterDebugCommands();
#endif
}

void UMultiplayerSessionsSubsystem::Deinitialize()
{
#if !UE_BUILD_SHIPPING
    UnregisterDebugCommands();
#endif

    // Cancelar búsquedas/joins worldwide en curso
#if PT_WITH_STEAM
    delete WorldwideSearch; WorldwideSearch = nullptr;
    delete WorldwideJoin;   WorldwideJoin   = nullptr;
#endif

    // Limpiar cualquier handle de OSS pendiente por seguridad
    if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld()))
    {
        if (IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface())
        {
            Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteHandle);
        }
    }

    // Destruir la sesión activa al cerrar el juego → evita sesiones "fantasma" que quedan
    // anunciadas en Steam y no se pueden joinear. Best-effort: en shutdown el callback async
    // puede no completar, pero al menos se dispara DestroySession en OSS.
    if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession))
    {
        SessionInterface->DestroySession(NAME_GameSession);
    }

    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
        SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteHandle);
        SessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(SessionInviteAcceptedHandle);
        SessionInterface->ClearOnSessionInviteReceivedDelegate_Handle(SessionInviteReceivedHandle);
    }

    Super::Deinitialize();
}

// ==========================================================================
// Helpers privados
// ==========================================================================

IOnlineSessionPtr UMultiplayerSessionsSubsystem::GetSessions() const
{
    return SessionInterface;
}

bool UMultiplayerSessionsSubsystem::IsUsingNullSubsystem() const
{
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    return Subsystem && Subsystem->GetSubsystemName() == FName("NULL");
}

FString UMultiplayerSessionsSubsystem::HashPassword(const FString& Plain)
{
    if (Plain.IsEmpty()) return FString();
    // MD5 simple — suficiente para el template; usar PBKDF2/bcrypt en producción seria.
    return FMD5::HashAnsiString(*Plain);
}

FString UMultiplayerSessionsSubsystem::GetLocalPlayerDisplayName() const
{
    if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld()))
    {
        if (IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface())
        {
            const FString Nickname = Identity->GetPlayerNickname(0);
            if (!Nickname.IsEmpty()) return Nickname;
        }
    }
    return TEXT("Player");
}

// ==========================================================================
// LOGIN — agnóstico (NULL resuelve al instante, Steam autentica contra la cuenta local)
// ==========================================================================

void UMultiplayerSessionsSubsystem::Login()
{
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    if (!Subsystem)
    {
        UE_LOG(LogPTSessions, Error, TEXT("Login: OnlineSubsystem no disponible."));
        OnLoginComplete.Broadcast(false);
        return;
    }

    // Si el .ini pide Steam pero terminamos en NULL, es porque el cliente de Steam no
    // estaba listo en el instante exacto en que arrancó el motor (carrera de arranque muy
    // común: "Cannot create IPC pipe to Steam client process. Steam is probably not
    // running."). Sin este chequeo, el juego sigue andando con un backend NULL (sin red
    // real) sin avisar nada — el usuario ve el menú normal, puede "crear"/"buscar"
    // sesiones, pero nunca son reales y nunca se encuentran entre dos PCs.
    FString ConfiguredPlatform;
    GConfig->GetString(TEXT("OnlineSubsystem"), TEXT("DefaultPlatformService"), ConfiguredPlatform, GEngineIni);
    if (Subsystem->GetSubsystemName() == FName("NULL") && ConfiguredPlatform.Equals(TEXT("Steam"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogPTSessions, Error, TEXT("Login: se esperaba Steam pero el subsistema activo es NULL (Steam no estaba listo al arrancar). Reiniciar el juego con Steam ya abierto."));
        OnLoginComplete.Broadcast(false);
        return;
    }

    IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
    if (!Identity.IsValid())
    {
        UE_LOG(LogPTSessions, Error, TEXT("Login: IdentityInterface no disponible."));
        OnLoginComplete.Broadcast(false);
        return;
    }

    // En NULL, el usuario ya está "logueado" sin flujo real → resolver al instante.
    if (Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
    {
        bIsLoggedIn = true;
        UE_LOG(LogPTSessions, Log, TEXT("Login: ya autenticado (subsistema NULL o sesión previa)."));
        OnLoginComplete.Broadcast(true);
        return;
    }

    LoginCompleteHandle = Identity->AddOnLoginCompleteDelegate_Handle(
        0,
        FOnLoginCompleteDelegate::CreateUObject(this, &UMultiplayerSessionsSubsystem::HandleLoginComplete));

    // AutoLogin es el punto de entrada uniforme.
    // En Steam, resuelve contra la cuenta de Steam ya logueada en el cliente local
    // (sin flujo de UI adicional) — el backend se elige por .ini, este código no cambia.
    if (!Identity->AutoLogin(0))
    {
        Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteHandle);
        UE_LOG(LogPTSessions, Warning, TEXT("Login: AutoLogin() devolvió false."));
        OnLoginComplete.Broadcast(false);
    }
}

void UMultiplayerSessionsSubsystem::HandleLoginComplete(
    int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
    if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld()))
    {
        if (IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface())
        {
            Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteHandle);
        }
    }

    bIsLoggedIn = bWasSuccessful;
    UE_LOG(LogPTSessions, Log, TEXT("Login completo — éxito: %s | error: %s"),
        bWasSuccessful ? TEXT("SÍ") : TEXT("NO"), *Error);

    OnLoginComplete.Broadcast(bWasSuccessful);
}

// ==========================================================================
// CREATE SESSION
// ==========================================================================

void UMultiplayerSessionsSubsystem::CreateSession(int32 NumPublicConnections, bool bFriendsOnly)
{
    if (!GetSessions().IsValid())
    {
        UE_LOG(LogPTSessions, Error, TEXT("CreateSession: SessionInterface no válida."));
        OnCreateSessionComplete.Broadcast(false);
        return;
    }
    if (!bIsLoggedIn)
    {
        UE_LOG(LogPTSessions, Warning, TEXT("CreateSession: llamado sin login previo."));
        OnCreateSessionComplete.Broadcast(false);
        return;
    }

    // Rango de jugadores fijado por el template (cada juego define su propia lógica de mínimo de arranque).
    PendingNumPublicConnections = FMath::Clamp(NumPublicConnections, MinPlayersAllowed, MaxPlayersAllowed);
    // El nombre de sala no lo tipea el usuario: es el nombre de Steam del host.
    PendingSessionName          = GetLocalPlayerDisplayName();
    // Visibilidad: pública o privada-solo-amigos. Ya NO hay código/contraseña.
    bPendingFriendsOnly         = bFriendsOnly;
    PendingPassword             = FString(); // inerte (se sacó el sistema de código)

    WorldwideConnectURL.Reset(); // limpiar URL de join previo al crear una sesión nueva

    // Si ya existe una sesión activa, destruirla primero y recrear en el callback de destroy.
    if (GetSessions()->GetNamedSession(NAME_GameSession) != nullptr)
    {
        UE_LOG(LogPTSessions, Log, TEXT("CreateSession: existe sesión previa, destruyendo..."));
        bCreateSessionOnDestroy = true;
        DestroySession();
        return;
    }

    InternalCreateSession();
}

void UMultiplayerSessionsSubsystem::InternalCreateSession()
{
    LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
    const bool bIsNULL = IsUsingNullSubsystem();
    LastSessionSettings->bIsLANMatch            = bIsNULL;   // true en NULL, false en Steam
    LastSessionSettings->NumPublicConnections   = PendingNumPublicConnections;
    LastSessionSettings->bAllowJoinInProgress   = true;
    LastSessionSettings->bShouldAdvertise       = true;
    // Steam exige bUsesPresence == bUseLobbiesIfAvailable (si no coinciden, CreateSession falla
    // con "the values... have to match"). bAllowJoinViaPresence es para el botón "unirse" del
    // overlay de Steam, no restringe el FindSessions normal a amigos — se deja igual a los otros.
    LastSessionSettings->bUsesPresence          = !bIsNULL;
    LastSessionSettings->bUseLobbiesIfAvailable = !bIsNULL;
    LastSessionSettings->bAllowJoinViaPresence  = !bIsNULL;
    // Invitaciones nativas de Steam (overlay "Invitar amigo" + "Unirse a partida" desde la lista de
    // amigos). No quita nada de lo público ni del código; solo agrega el camino de invitación directa.
    LastSessionSettings->bAllowInvites          = !bIsNULL;
    LastSessionSettings->BuildUniqueId          = 1;

    // Nombre visible elegido por el usuario (el FName interno siempre es NAME_GameSession)
    LastSessionSettings->Set(KEY_SERVER_NAME, PendingSessionName,
        EOnlineDataAdvertisementType::ViaOnlineService);

    // Visibilidad: true = privada solo amigos (pestaña Amigos), false = pública (pestaña Públicas).
    // Se anuncia SIEMPRE (también las de amigos): así tus amigos la ven en su Find Game; el filtrado
    // por pestaña + por relación de amistad lo hace la UI. Ya no hay contraseña.
    LastSessionSettings->Set(KEY_HAS_PASSWORD, bPendingFriendsOnly,
        EOnlineDataAdvertisementType::ViaOnlineService);

    // Clave de tipo para filtrar en búsquedas (solo sesiones de este template)
    LastSessionSettings->Set(KEY_MATCH_TYPE, FString("PartyLobby"),
        EOnlineDataAdvertisementType::ViaOnlineService);

    // Jugadores actuales: arranca en 1 (el host). El lobby GameMode la va actualizando al entrar/salir
    // gente (UpdateAdvertisedPlayerCount), así la lista de "buscar partidas" muestra el número real.
    LastSessionSettings->Set(KEY_CUR_PLAYERS, 1,
        EOnlineDataAdvertisementType::ViaOnlineService);

    CreateSessionCompleteHandle = GetSessions()->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateUObject(
            this, &UMultiplayerSessionsSubsystem::HandleCreateSessionComplete));

    const ULocalPlayer* LP = GetWorld()->GetFirstLocalPlayerFromController();
    if (!LP)
    {
        GetSessions()->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
        UE_LOG(LogPTSessions, Error, TEXT("InternalCreateSession: no hay LocalPlayer."));
        OnCreateSessionComplete.Broadcast(false);
        return;
    }

    if (!GetSessions()->CreateSession(*LP->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
    {
        GetSessions()->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
        UE_LOG(LogPTSessions, Warning, TEXT("InternalCreateSession: CreateSession() devolvió false."));
        OnCreateSessionComplete.Broadcast(false);
    }
}

void UMultiplayerSessionsSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (GetSessions().IsValid())
    {
        GetSessions()->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
    }

    UE_LOG(LogPTSessions, Log, TEXT("CreateSession completo — éxito: %s | sesión: %s"),
        bWasSuccessful ? TEXT("SÍ") : TEXT("NO"), *SessionName.ToString());

    OnCreateSessionComplete.Broadcast(bWasSuccessful);
    // El ServerTravel a /Game/Maps/Lobby?listen lo hará quien escuche este delegate (Fase 2/3),
    // NO el subsistema, para mantener separación red ↔ flujo de mapas.
}

// ==========================================================================
// FIND SESSIONS
// ==========================================================================

void UMultiplayerSessionsSubsystem::FindSessions(int32 MaxSearchResults)
{
    InternalFindSessions(MaxSearchResults);
}

void UMultiplayerSessionsSubsystem::InternalFindSessions(int32 MaxSearchResults)
{
    auto BroadcastFailure = [this]()
    {
        OnFindSessionsComplete.Broadcast({}, false);
    };

    if (!bIsLoggedIn)
    {
        UE_LOG(LogPTSessions, Warning, TEXT("FindSessions: llamado sin login previo."));
        BroadcastFailure();
        return;
    }

#if PT_WITH_STEAM
    // Búsqueda directa con k_ELobbyDistanceFilterWorldwide para que amigos de cualquier
    // región del mundo aparezcan — el engine hardcodea k_ELobbyDistanceFilterDefault
    // (misma región / regiones cercanas) y no expone ningún override.
    if (!IsUsingNullSubsystem() && SteamMatchmaking())
    {
        UE_LOG(LogPTSessions, Log, TEXT("FindSessions: usando búsqueda worldwide directa (Steam)."));

        delete WorldwideSearch;
        WorldwideSearch = new FPTSteamWorldwideSearch();
        WorldwideSearch->Start(MaxSearchResults,
            [this](TArray<FPTLobbyEntry>&& Entries, bool bOk)
            {
                delete WorldwideSearch; WorldwideSearch = nullptr;

                if (!bOk)
                {
                    UE_LOG(LogPTSessions, Warning, TEXT("FindSessions (worldwide): búsqueda fallida."));
                    OnFindSessionsComplete.Broadcast({}, false);
                    return;
                }

                UE_LOG(LogPTSessions, Log, TEXT("FindSessions (worldwide): %d resultado(s)."), Entries.Num());

                // Convertir FPTLobbyEntry → FOnlineSessionSearchResult para mantener
                // la misma interfaz que usa la UI (PTMainMenuWidget).
                TArray<FOnlineSessionSearchResult> Results;
                for (const FPTLobbyEntry& E : Entries)
                {
                    FOnlineSessionSearchResult R;
                    R.Session.OwningUserName                    = E.Name;
                    R.Session.SessionSettings.NumPublicConnections = E.NumPublicConnections;
                    R.Session.NumOpenPublicConnections           = E.NumOpenPublic;
                    R.Session.SessionSettings.Set(KEY_SERVER_NAME,  E.Name,         EOnlineDataAdvertisementType::DontAdvertise);
                    R.Session.SessionSettings.Set(KEY_HAS_PASSWORD, E.bHasPassword, EOnlineDataAdvertisementType::DontAdvertise);
                    R.Session.SessionSettings.Set(KEY_CUR_PLAYERS,  E.CurPlayers,   EOnlineDataAdvertisementType::DontAdvertise);
                    // SteamID del host (dueño de la sesión) → lo usa la pestaña Amigos para filtrar.
                    R.Session.SessionSettings.Set(KEY_OWNER_ID,     E.P2PAddr,      EOnlineDataAdvertisementType::DontAdvertise);
                    // Guardar lobby ID como string para que InternalJoinByLobbyId lo lea.
                    R.Session.SessionSettings.Set(KEY_LOBBY_ID,
                        FString::Printf(TEXT("%llu"), E.LobbyId),
                        EOnlineDataAdvertisementType::DontAdvertise);
                    Results.Add(MoveTemp(R));
                }

#if !UE_BUILD_SHIPPING
                CachedSearchResults = Results;
#endif
                OnFindSessionsComplete.Broadcast(Results, true);
            });
        return;
    }
#endif // PT_WITH_STEAM

    // Ruta de respaldo: NULL subsystem (LAN) o plataformas sin Steamworks.
    if (!GetSessions().IsValid())
    {
        UE_LOG(LogPTSessions, Error, TEXT("FindSessions: SessionInterface no válida."));
        BroadcastFailure();
        return;
    }

    LastSessionSearch                    = MakeShareable(new FOnlineSessionSearch());
    LastSessionSearch->MaxSearchResults  = MaxSearchResults;
    LastSessionSearch->bIsLanQuery       = IsUsingNullSubsystem();
    if (!IsUsingNullSubsystem())
    {
        LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
        LastSessionSearch->QuerySettings.Set(KEY_MATCH_TYPE, FString("PartyLobby"), EOnlineComparisonOp::Equals);
    }

    FindSessionsCompleteHandle = GetSessions()->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(
            this, &UMultiplayerSessionsSubsystem::HandleFindSessionsComplete));

    const ULocalPlayer* LP = GetWorld()->GetFirstLocalPlayerFromController();
    if (!LP)
    {
        GetSessions()->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
        UE_LOG(LogPTSessions, Error, TEXT("FindSessions: no hay LocalPlayer."));
        BroadcastFailure();
        return;
    }

    if (!GetSessions()->FindSessions(*LP->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
    {
        GetSessions()->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
        UE_LOG(LogPTSessions, Warning, TEXT("FindSessions: FindSessions() devolvió false."));
        BroadcastFailure();
    }
}

void UMultiplayerSessionsSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
    if (GetSessions().IsValid())
    {
        GetSessions()->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
    }

    if (!LastSessionSearch.IsValid() || LastSessionSearch->SearchResults.Num() == 0)
    {
        UE_LOG(LogPTSessions, Log, TEXT("FindSessions: sin resultados."));
        OnFindSessionsComplete.Broadcast({}, false);
        return;
    }

    UE_LOG(LogPTSessions, Log, TEXT("FindSessions: %d resultado(s) encontrado(s)."),
        LastSessionSearch->SearchResults.Num());

    for (int32 i = 0; i < LastSessionSearch->SearchResults.Num(); ++i)
    {
        const auto& R = LastSessionSearch->SearchResults[i];
        UE_LOG(LogPTSessions, Log, TEXT("  [%d] Nombre: %s | Jugadores: %d/%d | Contraseña: %s"),
            i,
            *GetServerNameFromResult(R),
            R.Session.NumOpenPublicConnections,
            R.Session.SessionSettings.NumPublicConnections,
            GetHasPasswordFromResult(R) ? TEXT("SÍ") : TEXT("NO"));
    }

#if !UE_BUILD_SHIPPING
    CachedSearchResults = LastSessionSearch->SearchResults;
#endif

    OnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
}

// ==========================================================================
// JOIN SESSION
// ==========================================================================

void UMultiplayerSessionsSubsystem::JoinSession(
    const FOnlineSessionSearchResult& SessionResult, const FString& Password)
{
    PendingJoinPassword = Password;
    PendingSessionName  = GetServerNameFromResult(SessionResult);

    // Si el resultado viene de nuestra búsqueda worldwide directa, tiene KEY_LOBBY_ID —
    // en ese caso usamos el join directo por Steamworks (sin pasar por SessionInterface,
    // que también llamaría a JoinLobby pero primero filtraría por región).
    FString LobbyIdStr;
    if (SessionResult.Session.SessionSettings.Get(KEY_LOBBY_ID, LobbyIdStr) && !LobbyIdStr.IsEmpty())
    {
        const uint64 LobbyId = FCString::Strtoui64(*LobbyIdStr, nullptr, 10);
        InternalJoinByLobbyId(LobbyId);
        return;
    }

    // Ruta estándar del engine (NULL subsystem / LAN o fallback).
    if (!GetSessions().IsValid())
    {
        UE_LOG(LogPTSessions, Error, TEXT("JoinSession: SessionInterface no válida."));
        OnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
        return;
    }

    JoinSessionCompleteHandle = GetSessions()->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateUObject(
            this, &UMultiplayerSessionsSubsystem::HandleJoinSessionComplete));

    const ULocalPlayer* LP = GetWorld()->GetFirstLocalPlayerFromController();
    if (!LP)
    {
        GetSessions()->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
        UE_LOG(LogPTSessions, Error, TEXT("JoinSession: no hay LocalPlayer."));
        OnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
        return;
    }

    if (!GetSessions()->JoinSession(*LP->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult))
    {
        GetSessions()->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
        UE_LOG(LogPTSessions, Warning, TEXT("JoinSession: JoinSession() devolvió false."));
        OnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
    }
}

void UMultiplayerSessionsSubsystem::InternalJoinByLobbyId(uint64 LobbyId)
{
#if PT_WITH_STEAM
    UE_LOG(LogPTSessions, Log, TEXT("JoinSession (worldwide): uniendo lobby %llu directamente."), LobbyId);
    WorldwideConnectURL.Reset();
    delete WorldwideJoin;
    WorldwideJoin = new FPTSteamDirectJoin();
    WorldwideJoin->JoinLobby(LobbyId, [this](bool bOk, FString URL)
    {
        delete WorldwideJoin; WorldwideJoin = nullptr;
        if (bOk)
        {
            WorldwideConnectURL = URL;
            UE_LOG(LogPTSessions, Log, TEXT("JoinSession (worldwide): lobby unida, URL=%s"), *URL);
            OnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::Success);
        }
        else
        {
            UE_LOG(LogPTSessions, Warning, TEXT("JoinSession (worldwide): falló al unirse a la lobby."));
            OnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
        }
    });
#else
    UE_LOG(LogPTSessions, Error, TEXT("InternalJoinByLobbyId: Steamworks no disponible."));
    OnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
#endif
}

void UMultiplayerSessionsSubsystem::HandleJoinSessionComplete(
    FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    if (GetSessions().IsValid())
    {
        GetSessions()->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
    }

    UE_LOG(LogPTSessions, Log, TEXT("JoinSession completo — resultado: %d"), (int32)Result);

    OnJoinSessionComplete.Broadcast(Result);
    // El ClientTravel (con ?Password=...) lo hace quien escuche este delegate (Fase 3),
    // usando GetResolvedConnectString() + GetPendingJoinPassword().
}

// ==========================================================================
// INVITACIONES DE STEAM (aceptar + invitar + overlay)
// ==========================================================================

void UMultiplayerSessionsSubsystem::HandleSessionUserInviteAccepted(
    const bool bWasSuccessful, const int32 ControllerId,
    FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
    if (!bWasSuccessful || !InviteResult.IsValid())
    {
        UE_LOG(LogPTSessions, Warning, TEXT("Invitación aceptada pero inválida (ok=%s, result válido=%s)."),
            bWasSuccessful ? TEXT("sí") : TEXT("no"), InviteResult.IsValid() ? TEXT("sí") : TEXT("no"));
        return;
    }

    // Guardar en cola: NO unirse acá. El join+travel lo dispara la UI del menú llamando a
    // ProcessPendingInvite() — así funciona tanto si el juego ya está en el menú (escucha
    // OnInviteAccepted) como si recién se lanzó desde la invitación (el menú lo procesa al construirse).
    PendingInviteResult = MakeShared<FOnlineSessionSearchResult>(InviteResult);
    bHasPendingInvite   = true;
    UE_LOG(LogPTSessions, Log, TEXT("Invitación de Steam aceptada — encolada para unirse (%s)."),
        *GetServerNameFromResult(InviteResult));

    OnInviteAccepted.Broadcast();
}

void UMultiplayerSessionsSubsystem::ProcessPendingInvite()
{
    if (!bHasPendingInvite || !PendingInviteResult.IsValid()) return;

    // Consumir la cola (idempotente: si se llama dos veces, la segunda no hace nada).
    bHasPendingInvite = false;
    const TSharedPtr<FOnlineSessionSearchResult> R = PendingInviteResult;
    PendingInviteResult.Reset();

    UE_LOG(LogPTSessions, Log, TEXT("Procesando invitación en cola → uniéndose a %s."),
        *GetServerNameFromResult(*R));

    // Sin contraseña: la invitación ya autoriza. JoinSession dispara OnJoinSessionComplete →
    // el menú viaja con GetResolvedConnectString().
    JoinSession(*R, FString());
}

void UMultiplayerSessionsSubsystem::HandleSessionInviteReceived(
    const FUniqueNetId& UserId, const FUniqueNetId& FromId,
    const FString& AppId, const FOnlineSessionSearchResult& InviteResult)
{
    if (!InviteResult.IsValid())
    {
        UE_LOG(LogPTSessions, Warning, TEXT("Invitación recibida pero el resultado es inválido."));
        return;
    }

    ReceivedInviteResult = MakeShared<FOnlineSessionSearchResult>(InviteResult);

    // Nombre del que invita = dueño de la sesión (nombre de Steam del host). Fallback a la lista
    // de amigos por si acaso, y si no, un genérico.
    FString FromName = InviteResult.Session.OwningUserName;
    if (FromName.IsEmpty())
    {
        const FString FromIdStr = FromId.ToString();
        for (const FPTFriendInfo& F : CachedFriends)
            if (F.UserId == FromIdStr) { FromName = F.DisplayName; break; }
    }
    if (FromName.IsEmpty()) FromName = TEXT("Un amigo");

    UE_LOG(LogPTSessions, Log, TEXT("Invitación recibida de %s → mostrando popup."), *FromName);
    OnInviteReceived.Broadcast(FromName);
}

void UMultiplayerSessionsSubsystem::AcceptReceivedInvite()
{
    if (!ReceivedInviteResult.IsValid()) return;
    const TSharedPtr<FOnlineSessionSearchResult> R = ReceivedInviteResult;
    ReceivedInviteResult.Reset();
    UE_LOG(LogPTSessions, Log, TEXT("Popup: invitación ACEPTADA → uniéndose."));
    JoinSession(*R, FString());
}

void UMultiplayerSessionsSubsystem::DeclineReceivedInvite()
{
    UE_LOG(LogPTSessions, Log, TEXT("Popup: invitación RECHAZADA."));
    ReceivedInviteResult.Reset();
}

void UMultiplayerSessionsSubsystem::ShowSteamInviteOverlay()
{
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    if (!Subsystem)
    {
        UE_LOG(LogPTSessions, Warning, TEXT("ShowSteamInviteOverlay: sin OnlineSubsystem."));
        return;
    }
    if (IOnlineExternalUIPtr ExternalUI = Subsystem->GetExternalUIInterface())
    {
        // Abre el panel de amigos del overlay de Steam en modo "invitar a la sesión".
        ExternalUI->ShowInviteUI(0, NAME_GameSession);
    }
    else
    {
        UE_LOG(LogPTSessions, Warning, TEXT("ShowSteamInviteOverlay: sin ExternalUI (¿overlay de Steam deshabilitado?)."));
    }
}

void UMultiplayerSessionsSubsystem::InviteFriend(const FString& FriendUserId)
{
    if (!GetSessions().IsValid()) return;
    const FUniqueNetIdPtr* Found = FriendIdMap.Find(FriendUserId);
    if (!Found || !Found->IsValid())
    {
        UE_LOG(LogPTSessions, Warning, TEXT("InviteFriend: no se encontró el net id de %s (¿releer amigos?)."), *FriendUserId);
        return;
    }
    const ULocalPlayer* LP = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
    if (!LP) return;

    if (GetSessions()->SendSessionInviteToFriend(*LP->GetPreferredUniqueNetId(), NAME_GameSession, **Found))
    {
        UE_LOG(LogPTSessions, Log, TEXT("Invitación enviada a %s."), *FriendUserId);
    }
    else
    {
        UE_LOG(LogPTSessions, Warning, TEXT("InviteFriend: SendSessionInviteToFriend devolvió false."));
    }
}

// ==========================================================================
// LISTA DE AMIGOS
// ==========================================================================

void UMultiplayerSessionsSubsystem::ReadFriends()
{
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineFriendsPtr Friends = Subsystem ? Subsystem->GetFriendsInterface() : nullptr;
    if (!Friends.IsValid())
    {
        UE_LOG(LogPTSessions, Warning, TEXT("ReadFriends: sin FriendsInterface (¿NULL subsystem/LAN?)."));
        CachedFriends.Reset();
        FriendIdMap.Reset();
        OnFriendsListUpdated.Broadcast();
        return;
    }

    Friends->ReadFriendsList(0, EFriendsLists::ToString(EFriendsLists::Default),
        FOnReadFriendsListComplete::CreateUObject(this, &UMultiplayerSessionsSubsystem::HandleReadFriendsComplete));
}

void UMultiplayerSessionsSubsystem::HandleReadFriendsComplete(
    int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
{
    CachedFriends.Reset();
    FriendIdMap.Reset();

    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
    IOnlineFriendsPtr Friends = Subsystem ? Subsystem->GetFriendsInterface() : nullptr;

    if (bWasSuccessful && Friends.IsValid())
    {
        TArray<TSharedRef<FOnlineFriend>> List;
        Friends->GetFriendsList(0, ListName, List);
        for (const TSharedRef<FOnlineFriend>& F : List)
        {
            const FOnlineUserPresence& P = F->GetPresence();
            FPTFriendInfo Info;
            Info.DisplayName      = F->GetDisplayName();
            Info.UserId           = F->GetUserId()->ToString();
            Info.bOnline          = P.bIsOnline;
            Info.bPlayingThisGame = P.bIsPlayingThisGame;
            Info.bJoinable        = P.bIsJoinable;
            CachedFriends.Add(Info);
            FriendIdMap.Add(Info.UserId, F->GetUserId());
        }

        // Orden: los que están jugando Sculpturillo primero, luego online, luego offline.
        CachedFriends.Sort([](const FPTFriendInfo& A, const FPTFriendInfo& B)
        {
            auto Rank = [](const FPTFriendInfo& X) { return X.bPlayingThisGame ? 0 : (X.bOnline ? 1 : 2); };
            const int32 RA = Rank(A), RB = Rank(B);
            if (RA != RB) return RA < RB;
            return A.DisplayName < B.DisplayName;
        });
    }
    else
    {
        UE_LOG(LogPTSessions, Warning, TEXT("ReadFriends falló: %s"), *ErrorStr);
    }

    UE_LOG(LogPTSessions, Log, TEXT("Amigos leídos: %d"), CachedFriends.Num());
    OnFriendsListUpdated.Broadcast();
}

// ==========================================================================
// DESTROY SESSION
// ==========================================================================

void UMultiplayerSessionsSubsystem::DestroySession()
{
    if (!GetSessions().IsValid())
    {
        UE_LOG(LogPTSessions, Warning, TEXT("DestroySession: SessionInterface no válida."));
        OnDestroySessionComplete.Broadcast(false);
        return;
    }

    DestroySessionCompleteHandle = GetSessions()->AddOnDestroySessionCompleteDelegate_Handle(
        FOnDestroySessionCompleteDelegate::CreateUObject(
            this, &UMultiplayerSessionsSubsystem::HandleDestroySessionComplete));

    if (!GetSessions()->DestroySession(NAME_GameSession))
    {
        GetSessions()->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
        UE_LOG(LogPTSessions, Warning, TEXT("DestroySession: DestroySession() devolvió false."));
        OnDestroySessionComplete.Broadcast(false);
    }
}

void UMultiplayerSessionsSubsystem::CleanupStaleSession()
{
    if (!GetSessions().IsValid()) return;
    if (GetSessions()->GetNamedSession(NAME_GameSession) == nullptr) return; // nada que limpiar

    UE_LOG(LogPTSessions, Warning,
        TEXT("CleanupStaleSession: quedó una sesión registrada al volver al menú → destruyendo."));
    bCreateSessionOnDestroy = false; // limpieza pura: no recrear nada después
    DestroySession();
}

void UMultiplayerSessionsSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (GetSessions().IsValid())
    {
        GetSessions()->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
    }

    UE_LOG(LogPTSessions, Log, TEXT("DestroySession completo — éxito: %s"),
        bWasSuccessful ? TEXT("SÍ") : TEXT("NO"));

    // Si se destruyó para recrear (CreateSession cuando existía sesión previa)
    if (bWasSuccessful && bCreateSessionOnDestroy)
    {
        bCreateSessionOnDestroy = false;
        InternalCreateSession();
        return;
    }

    OnDestroySessionComplete.Broadcast(bWasSuccessful);
}

// ==========================================================================
// START SESSION
// ==========================================================================

void UMultiplayerSessionsSubsystem::StartSession()
{
    if (!GetSessions().IsValid())
    {
        UE_LOG(LogPTSessions, Warning, TEXT("StartSession: SessionInterface no válida."));
        OnStartSessionComplete.Broadcast(false);
        return;
    }

    StartSessionCompleteHandle = GetSessions()->AddOnStartSessionCompleteDelegate_Handle(
        FOnStartSessionCompleteDelegate::CreateUObject(
            this, &UMultiplayerSessionsSubsystem::HandleStartSessionComplete));

    if (!GetSessions()->StartSession(NAME_GameSession))
    {
        GetSessions()->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteHandle);
        UE_LOG(LogPTSessions, Warning, TEXT("StartSession: StartSession() devolvió false."));
        OnStartSessionComplete.Broadcast(false);
    }
}

void UMultiplayerSessionsSubsystem::HandleStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (GetSessions().IsValid())
    {
        GetSessions()->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteHandle);
    }

    UE_LOG(LogPTSessions, Log, TEXT("StartSession completo — éxito: %s"),
        bWasSuccessful ? TEXT("SÍ") : TEXT("NO"));

    OnStartSessionComplete.Broadcast(bWasSuccessful);
}

// ==========================================================================
// HELPERS PÚBLICOS
// ==========================================================================

bool UMultiplayerSessionsSubsystem::GetResolvedConnectString(FString& OutConnectString) const
{
    // Tras un join directo worldwide, la URL ya está calculada (steam.[id]:[port]).
    if (!WorldwideConnectURL.IsEmpty())
    {
        OutConnectString = WorldwideConnectURL;
        return true;
    }
    if (!SessionInterface.IsValid()) return false;
    return SessionInterface->GetResolvedConnectString(NAME_GameSession, OutConnectString);
}

bool UMultiplayerSessionsSubsystem::DoesHostPasswordMatch(const FString& Attempt) const
{
    if (PendingPassword.IsEmpty()) return true;   // sesión sin contraseña: aceptar a todos
    return HashPassword(Attempt) == HashPassword(PendingPassword);
}

FString UMultiplayerSessionsSubsystem::GetServerNameFromResult(const FOnlineSessionSearchResult& Result)
{
    FString Out;
    Result.Session.SessionSettings.Get(KEY_SERVER_NAME, Out);
    return Out;
}

bool UMultiplayerSessionsSubsystem::GetHasPasswordFromResult(const FOnlineSessionSearchResult& Result)
{
    // Compat: KEY_HAS_PASSWORD ahora significa "privada solo amigos".
    return GetIsFriendsOnlyFromResult(Result);
}

bool UMultiplayerSessionsSubsystem::GetIsFriendsOnlyFromResult(const FOnlineSessionSearchResult& Result)
{
    bool bFriendsOnly = false;
    Result.Session.SessionSettings.Get(KEY_HAS_PASSWORD, bFriendsOnly);
    return bFriendsOnly;
}

FString UMultiplayerSessionsSubsystem::GetOwnerSteamIdFromResult(const FOnlineSessionSearchResult& Result)
{
    // Los resultados de la búsqueda worldwide guardan el SteamID del host en KEY_OWNER_ID.
    FString OwnerId;
    if (Result.Session.SessionSettings.Get(KEY_OWNER_ID, OwnerId) && !OwnerId.IsEmpty())
        return OwnerId;
    // Ruta estándar (NULL/LAN): el engine llena OwningUserId.
    return Result.Session.OwningUserId.IsValid() ? Result.Session.OwningUserId->ToString() : FString();
}

bool UMultiplayerSessionsSubsystem::IsFriendSteamId(const FString& SteamId) const
{
    return !SteamId.IsEmpty() && FriendIdMap.Contains(SteamId);
}

int32 UMultiplayerSessionsSubsystem::GetCurrentPlayersFromResult(const FOnlineSessionSearchResult& Result)
{
    int32 Cur = 0;
    if (Result.Session.SessionSettings.Get(KEY_CUR_PLAYERS, Cur) && Cur > 0)
        return Cur;
    return -1; // no vino la clave → el que llama usa el fallback (Max - Open)
}

void UMultiplayerSessionsSubsystem::SetSessionFriendsOnly(bool bFriendsOnly)
{
    bPendingFriendsOnly = bFriendsOnly;
    if (!LastSessionSettings.IsValid()) return; // solo el host tiene los settings creados
    IOnlineSessionPtr S = GetSessions();
    if (!S.IsValid()) return;

    LastSessionSettings->Set(KEY_HAS_PASSWORD, bFriendsOnly, EOnlineDataAdvertisementType::ViaOnlineService);
    S->UpdateSession(NAME_GameSession, *LastSessionSettings, /*bShouldRefreshOnlineData=*/true);
    UE_LOG(LogPTSessions, Log, TEXT("SetSessionFriendsOnly: sesión ahora %s."),
        bFriendsOnly ? TEXT("privada (solo amigos)") : TEXT("pública"));
}

void UMultiplayerSessionsSubsystem::UpdateAdvertisedPlayerCount(int32 CurrentPlayers)
{
    if (!LastSessionSettings.IsValid()) return; // solo el host tiene los settings de la sesión creada
    IOnlineSessionPtr S = GetSessions();
    if (!S.IsValid()) return;

    LastSessionSettings->Set(KEY_CUR_PLAYERS, FMath::Max(0, CurrentPlayers),
        EOnlineDataAdvertisementType::ViaOnlineService);
    // Re-publica la sesión con el nuevo valor (Steam actualiza el lobby data → la lista lo ve al refrescar).
    S->UpdateSession(NAME_GameSession, *LastSessionSettings, /*bShouldRefreshOnlineData=*/true);
    UE_LOG(LogPTSessions, Log, TEXT("UpdateAdvertisedPlayerCount: %d jugadores anunciados."), CurrentPlayers);
}

FString UMultiplayerSessionsSubsystem::SanitizeNameForTravelURL(const FString& Name)
{
    FString Result;
    Result.Reserve(Name.Len());
    for (const TCHAR Ch : Name)
    {
        if (FChar::IsAlnum(Ch) || Ch == TEXT('-') || Ch == TEXT('_') || Ch == TEXT(' '))
        {
            Result.AppendChar(Ch);
        }
    }
    Result.TrimStartAndEndInline();
    return Result.IsEmpty() ? TEXT("Player") : Result;
}

// ==========================================================================
// DEBUG — Comandos de consola (solo en builds no-shipping)
// ==========================================================================

#if !UE_BUILD_SHIPPING

void UMultiplayerSessionsSubsystem::RegisterDebugCommands()
{
    // PT.Debug.Login
    DebugCmd_Login = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("PT.Debug.Login"),
        TEXT("[Fase1 Debug] Login al subsistema de sesiones."),
        FConsoleCommandDelegate::CreateUObject(this, &UMultiplayerSessionsSubsystem::Login),
        ECVF_Default);

    // PT.Debug.Create [Privada=0/1] [MaxJugadores]
    DebugCmd_CreateSession = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("PT.Debug.Create"),
        TEXT("[Fase1/5 Debug] Crear sesión (nombre = Steam name del host). Args: [Privada=0] [Max=4]"),
        FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args)
        {
            const bool    bPrivate  = Args.Num() > 0 && Args[0] != TEXT("0");
            const int32   Max       = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 4;
            UE_LOG(LogPTSessions, Log, TEXT("[Debug] CreateSession(privada=%s, %d)"),
                bPrivate ? TEXT("SÍ") : TEXT("NO"), Max);
            CreateSession(Max, bPrivate);
        }),
        ECVF_Default);

    // PT.Debug.Find [MaxResultados]
    DebugCmd_FindSessions = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("PT.Debug.Find"),
        TEXT("[Fase1 Debug] Buscar sesiones. Args: [Max=20]"),
        FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args)
        {
            const int32 Max = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 20;
            UE_LOG(LogPTSessions, Log, TEXT("[Debug] FindSessions(%d)"), Max);
            FindSessions(Max);
        }),
        ECVF_Default);

    // PT.Debug.Join [Índice] [Password]
    DebugCmd_JoinSession = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("PT.Debug.Join"),
        TEXT("[Fase1 Debug] Unirse a sesión por índice del último Find. Args: [Índice=0] [Password=]"),
        FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args)
        {
            const int32   Idx  = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
            const FString Pass = Args.Num() > 1 ? Args[1] : TEXT("");

            if (!CachedSearchResults.IsValidIndex(Idx))
            {
                UE_LOG(LogPTSessions, Warning,
                    TEXT("[Debug] Join: índice %d inválido (hay %d resultados cacheados). Ejecuta PT.Debug.Find primero."),
                    Idx, CachedSearchResults.Num());
                return;
            }
            UE_LOG(LogPTSessions, Log, TEXT("[Debug] JoinSession(índice=%d, pass=%s)"), Idx, *Pass);
            JoinSession(CachedSearchResults[Idx], Pass);
        }),
        ECVF_Default);

    // PT.Debug.InvitePopup [Nombre] — simula recibir una invitación para PROBAR el popup en PIE/standalone
    // (las invitaciones reales de Steam necesitan 2 cuentas/máquinas). Aceptar no une a nada (es de prueba).
    DebugCmd_InvitePopup = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("PT.Debug.InvitePopup"),
        TEXT("[Debug] Simula recibir una invitación y muestra el popup. Args: [Nombre=Amigo de prueba]"),
        FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args)
        {
            const FString Name = Args.Num() > 0 ? FString::Join(Args, TEXT(" ")) : TEXT("Amigo de prueba");
            UE_LOG(LogPTSessions, Log, TEXT("[Debug] InvitePopup de '%s'"), *Name);
            OnInviteReceived.Broadcast(Name);
        }),
        ECVF_Default);

    // PT.Debug.Destroy
    DebugCmd_DestroySession = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("PT.Debug.Destroy"),
        TEXT("[Fase1 Debug] Destruir la sesión actual."),
        FConsoleCommandDelegate::CreateUObject(this, &UMultiplayerSessionsSubsystem::DestroySession),
        ECVF_Default);

    UE_LOG(LogPTSessions, Log,
        TEXT("Comandos de debug registrados: PT.Debug.Login | PT.Debug.Create | PT.Debug.Find | PT.Debug.Join | PT.Debug.Destroy"));
}

void UMultiplayerSessionsSubsystem::UnregisterDebugCommands()
{
    // En PIE con múltiples instancias, ambas registran los mismos comandos y comparten
    // el mismo puntero subyacente. Al cerrar, la primera instancia lo libera; la segunda
    // debe verificar por nombre que el objeto todavía exista antes de intentar desregistrar.
    auto Unreg = [](IConsoleCommand*& Cmd, const TCHAR* Name)
    {
        if (Cmd && IConsoleManager::Get().FindConsoleObject(Name) == Cmd)
        {
            IConsoleManager::Get().UnregisterConsoleObject(Cmd);
        }
        Cmd = nullptr;
    };

    Unreg(DebugCmd_Login,          TEXT("PT.Debug.Login"));
    Unreg(DebugCmd_CreateSession,  TEXT("PT.Debug.Create"));
    Unreg(DebugCmd_FindSessions,   TEXT("PT.Debug.Find"));
    Unreg(DebugCmd_JoinSession,    TEXT("PT.Debug.Join"));
    Unreg(DebugCmd_DestroySession, TEXT("PT.Debug.Destroy"));
    Unreg(DebugCmd_InvitePopup,    TEXT("PT.Debug.InvitePopup"));
}

#endif // !UE_BUILD_SHIPPING
