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

    // Tope de jugadores que el template permite por sala; cada juego define su propio mínimo.
    static constexpr int32 MaxPlayersAllowed = 10;

    // Fase 5 — bPrivate=true genera un código aleatorio internamente (ver GetGeneratedSessionCode);
    // bPrivate=false crea una sesión pública sin código, visible en FindSessions sin filtrar.
    // El nombre de la sala no se pasa: se toma del nombre de Steam del host (GetLocalPlayerDisplayName).
    void CreateSession(int32 NumPublicConnections, bool bPrivate);
    void FindSessions(int32 MaxSearchResults);
    void JoinSession(const FOnlineSessionSearchResult& SessionResult, const FString& Password);

    // Fase 5 — busca entre las sesiones anunciadas la que coincide con este código de invitación
    // y, si la encuentra, se une directamente (sin pasar por una lista visible).
    // Si no hay coincidencia, OnJoinSessionComplete dispara con SessionDoesNotExist.
    void JoinSessionByCode(const FString& Code);

    void DestroySession();
    void StartSession();

    // Helper para que el PlayerController viaje tras crear/unirse (Fases 2-3).
    // Devuelve la dirección de conexión resuelta (ej: "192.168.1.5:7777").
    bool GetResolvedConnectString(FString& OutConnectString) const;

    // Getters de estado (para UI / PlayerController)
    FString GetPendingJoinPassword()    const { return PendingJoinPassword;  }
    FString GetPendingHostPassword()    const { return PendingPassword;      }
    FString GetPendingSessionName()     const { return PendingSessionName;   }
    bool    IsLoggedIn()                const { return bIsLoggedIn;          }

    // Fase 5 — código de invitación recién generado para la sesión propia (vacío si es pública).
    // Mostrar al host tras OnCreateSessionComplete(true) para que lo copie y comparta.
    FString GetGeneratedSessionCode()   const { return PendingPassword;      }

    // Helpers estáticos para leer settings de un resultado de búsqueda
    static FString GetServerNameFromResult(const FOnlineSessionSearchResult& Result);
    static bool    GetHasPasswordFromResult(const FOnlineSessionSearchResult& Result);

    // Fase 4 — Compara un intento de contraseña contra la del host.
    // Devuelve true si coincide, o si el host no puso contraseña.
    bool DoesHostPasswordMatch(const FString& Attempt) const;

    // ------------------------------------------------------------------
    // Delegates hacia la UI — suscribirse a estos, no a los de OSS
    // ------------------------------------------------------------------
    FPTOnLoginComplete           OnLoginComplete;
    FPTOnCreateSessionComplete   OnCreateSessionComplete;
    FPTOnFindSessionsComplete    OnFindSessionsComplete;
    FPTOnJoinSessionComplete     OnJoinSessionComplete;
    FPTOnDestroySessionComplete  OnDestroySessionComplete;
    FPTOnStartSessionComplete    OnStartSessionComplete;

protected:
    // Callbacks que OSS invoca internamente
    void HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful,
                             const FUniqueNetId& UserId, const FString& Error);
    void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void HandleFindSessionsComplete(bool bWasSuccessful);
    void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
    void HandleStartSessionComplete(FName SessionName, bool bWasSuccessful);

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

    // Estado interno
    bool    bIsLoggedIn               = false;
    bool    bCreateSessionOnDestroy   = false;  // si había sesión vieja, destruir y recrear
    int32   PendingNumPublicConnections = 0;
    FString PendingSessionName;
    FString PendingPassword;        // código/contraseña que guarda el host (NO se publica en OSS)
    FString PendingJoinPassword;    // código/contraseña que el cliente envía al hacer join

    // Fase 5 — true mientras un FindSessions en curso es para JoinSessionByCode (no para
    // poblar la lista pública); HandleFindSessionsComplete bifurca según este flag.
    bool    bSearchingByCode = false;
    FString PendingJoinCode;

    // Claves de settings de sesión (definidas como FName para evitar typos)
    static const FName KEY_SERVER_NAME;
    static const FName KEY_HAS_PASSWORD;
    static const FName KEY_MATCH_TYPE;
    static const FName KEY_CODE_HASH;    // Fase 5 — hash del código; nunca se anuncia en claro

    // Helpers privados
    IOnlineSessionPtr GetSessions() const;
    bool IsUsingNullSubsystem() const;                   // true → LAN (NULL subsystem)
    static FString HashPassword(const FString& Plain);   // MD5 simple; reforzar en producción
    static FString GenerateSessionCode();                // Fase 5 — código aleatorio de 6 caracteres
    FString GetLocalPlayerDisplayName() const;            // nombre de Steam del jugador local (host)
    void InternalCreateSession();                        // crea de verdad tras login/destroy
    void InternalFindSessions(int32 MaxSearchResults);    // Find compartido por FindSessions y JoinSessionByCode

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
    IConsoleCommand* DebugCmd_JoinByCode     = nullptr;
    IConsoleCommand* DebugCmd_DestroySession = nullptr;

    // Caché de resultados para PT.Debug.Join [índice]
    TArray<FOnlineSessionSearchResult> CachedSearchResults;
#endif
};
