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
    void CreateSession(int32 NumPublicConnections, const FString& SessionName, const FString& Password);
    void FindSessions(int32 MaxSearchResults);
    void JoinSession(const FOnlineSessionSearchResult& SessionResult, const FString& Password);
    void DestroySession();
    void StartSession();

    // Helper para que el PlayerController viaje tras crear/unirse (Fases 2-3).
    // Devuelve la dirección de conexión resuelta (ej: "192.168.1.5:7777").
    bool GetResolvedConnectString(FString& OutConnectString) const;

    // Getters de estado (para UI / PlayerController)
    FString GetPendingJoinPassword()  const { return PendingJoinPassword;  }
    FString GetPendingHostPassword()  const { return PendingPassword;      }
    FString GetPendingSessionName()   const { return PendingSessionName;   }
    bool    IsLoggedIn()              const { return bIsLoggedIn;          }

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
    FString PendingPassword;        // contraseña que guarda el host (NO se publica en OSS)
    FString PendingJoinPassword;    // contraseña que el cliente envía al hacer join

    // Claves de settings de sesión (definidas como FName para evitar typos)
    static const FName KEY_SERVER_NAME;
    static const FName KEY_HAS_PASSWORD;
    static const FName KEY_MATCH_TYPE;

    // Helpers privados
    IOnlineSessionPtr GetSessions() const;
    bool IsUsingNullSubsystem() const;                   // true → LAN (NULL subsystem)
    static FString HashPassword(const FString& Plain);   // MD5 simple; reforzar en producción
    void InternalCreateSession();                        // crea de verdad tras login/destroy

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

    // Caché de resultados para PT.Debug.Join [índice]
    TArray<FOnlineSessionSearchResult> CachedSearchResults;
#endif
};
