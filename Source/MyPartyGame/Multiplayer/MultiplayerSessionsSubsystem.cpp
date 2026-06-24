// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 1 — Implementación de UMultiplayerSessionsSubsystem.
// INVARIANTE: único archivo del proyecto que toca IOnlineSubsystem / IOnlineSession.

#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"              // FOnlineSessionSettings, SEARCH_PRESENCE (UE 5.8)
#include "Interfaces/OnlineIdentityInterface.h"
#include "Misc/SecureHash.h"                    // FMD5

DEFINE_LOG_CATEGORY_STATIC(LogPTSessions, Log, All);

// --- Claves de settings de sesión ---
const FName UMultiplayerSessionsSubsystem::KEY_SERVER_NAME  = FName("SERVER_NAME");
const FName UMultiplayerSessionsSubsystem::KEY_HAS_PASSWORD = FName("HAS_PASSWORD");
const FName UMultiplayerSessionsSubsystem::KEY_MATCH_TYPE   = FName("MATCH_TYPE");

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

    // Limpiar cualquier handle de OSS pendiente por seguridad
    if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld()))
    {
        if (IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface())
        {
            Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteHandle);
        }
    }

    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
        SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteHandle);
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

// ==========================================================================
// LOGIN — agnóstico (NULL resuelve al instante, EOS autentica de verdad)
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
    // En EOS, el método concreto (Device ID / Account Portal) se configura por .ini en la Fase 7
    // sin tocar este código — así se mantiene el agnosticismo de backend.
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

void UMultiplayerSessionsSubsystem::CreateSession(
    int32 NumPublicConnections, const FString& SessionName, const FString& Password)
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

    PendingNumPublicConnections = NumPublicConnections;
    PendingSessionName          = SessionName;
    PendingPassword             = Password;

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
    LastSessionSettings->bIsLANMatch            = bIsNULL;   // true en NULL, false en EOS
    LastSessionSettings->NumPublicConnections   = PendingNumPublicConnections;
    LastSessionSettings->bAllowJoinInProgress   = true;
    LastSessionSettings->bShouldAdvertise       = true;
    // Presence y lobbies solo en backends que los soportan (EOS). NULL los ignora o rompe el Find.
    LastSessionSettings->bUsesPresence          = !bIsNULL;
    LastSessionSettings->bUseLobbiesIfAvailable = !bIsNULL;
    LastSessionSettings->bAllowJoinViaPresence  = !bIsNULL;
    LastSessionSettings->BuildUniqueId          = 1;

    // Nombre visible elegido por el usuario (el FName interno siempre es NAME_GameSession)
    LastSessionSettings->Set(KEY_SERVER_NAME, PendingSessionName,
        EOnlineDataAdvertisementType::ViaOnlineService);

    // Solo se publica si tiene contraseña (booleano), NUNCA la contraseña real
    LastSessionSettings->Set(KEY_HAS_PASSWORD, (bool)!PendingPassword.IsEmpty(),
        EOnlineDataAdvertisementType::ViaOnlineService);

    // Clave de tipo para filtrar en búsquedas (solo sesiones de este template)
    LastSessionSettings->Set(KEY_MATCH_TYPE, FString("PartyLobby"),
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
    if (!GetSessions().IsValid())
    {
        UE_LOG(LogPTSessions, Error, TEXT("FindSessions: SessionInterface no válida."));
        OnFindSessionsComplete.Broadcast({}, false);
        return;
    }
    if (!bIsLoggedIn)
    {
        UE_LOG(LogPTSessions, Warning, TEXT("FindSessions: llamado sin login previo."));
        OnFindSessionsComplete.Broadcast({}, false);
        return;
    }

    LastSessionSearch                    = MakeShareable(new FOnlineSessionSearch());
    LastSessionSearch->MaxSearchResults  = MaxSearchResults;
    LastSessionSearch->bIsLanQuery       = IsUsingNullSubsystem();
    // El filtro "presence" solo aplica a backends que lo soportan (EOS). En NULL rompe el Find.
    if (!IsUsingNullSubsystem())
    {
        LastSessionSearch->QuerySettings.Set(FName(TEXT("presence")), true, EOnlineComparisonOp::Equals);
    }

    FindSessionsCompleteHandle = GetSessions()->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(
            this, &UMultiplayerSessionsSubsystem::HandleFindSessionsComplete));

    const ULocalPlayer* LP = GetWorld()->GetFirstLocalPlayerFromController();
    if (!LP)
    {
        GetSessions()->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
        UE_LOG(LogPTSessions, Error, TEXT("FindSessions: no hay LocalPlayer."));
        OnFindSessionsComplete.Broadcast({}, false);
        return;
    }

    if (!GetSessions()->FindSessions(*LP->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
    {
        GetSessions()->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
        UE_LOG(LogPTSessions, Warning, TEXT("FindSessions: FindSessions() devolvió false."));
        OnFindSessionsComplete.Broadcast({}, false);
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
    if (!GetSessions().IsValid())
    {
        UE_LOG(LogPTSessions, Error, TEXT("JoinSession: SessionInterface no válida."));
        OnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
        return;
    }

    PendingJoinPassword = Password;
    PendingSessionName  = GetServerNameFromResult(SessionResult);

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
    bool bHas = false;
    Result.Session.SessionSettings.Get(KEY_HAS_PASSWORD, bHas);
    return bHas;
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

    // PT.Debug.Create [Nombre] [Password] [MaxJugadores]
    DebugCmd_CreateSession = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("PT.Debug.Create"),
        TEXT("[Fase1 Debug] Crear sesión. Args: [Nombre=TestRoom] [Password=] [Max=4]"),
        FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args)
        {
            const FString Name = Args.Num() > 0 ? Args[0] : TEXT("TestRoom");
            const FString Pass = Args.Num() > 1 ? Args[1] : TEXT("");
            const int32   Max  = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 4;
            UE_LOG(LogPTSessions, Log, TEXT("[Debug] CreateSession(%s, %s, %d)"), *Name, *Pass, Max);
            CreateSession(Max, Name, Pass);
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
}

#endif // !UE_BUILD_SHIPPING
