// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyGameMode.h"
#include "PTLobbyCharacter.h"
#include "PTLobbyPlayerController.h"
#include "PTPlayerState.h"
#include "PTGameState.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Kismet/GameplayStatics.h"

APTLobbyGameMode::APTLobbyGameMode()
{
    DefaultPawnClass      = APTLobbyCharacter::StaticClass();
    PlayerControllerClass = APTLobbyPlayerController::StaticClass();
    PlayerStateClass      = APTPlayerState::StaticClass();
    GameStateClass        = APTGameState::StaticClass();
    bUseSeamlessTravel    = true; // Preparado para el viaje lobby→minijuego (cosa de cada juego, no del template).
    // El lobby no usa el flujo de MatchState de AGameMode (StartMatch/EndMatch); esto evita que
    // arranque "match" solo automáticamente con el primer jugador. Solo nos interesa heredar el
    // InactivePlayerArray para la reconexión.
    bDelayedStart         = true;
}

void APTLobbyGameMode::BeginPlay()
{
    Super::BeginPlay();

    // El nombre/código de sala vive en el subsistema del host (= GameInstance del servidor
    // en listen-server); se replica a GameState para que cualquier jugador lo pueda ver.
    if (APTGameState* PTGS = GetGameState<APTGameState>())
    {
        if (UMultiplayerSessionsSubsystem* Sessions =
                GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>())
        {
            PTGS->SessionDisplayName = Sessions->GetPendingSessionName();
            PTGS->SessionCode        = Sessions->GetGeneratedSessionCode();
            PTGS->MaxPlayers         = Sessions->GetPendingMaxPlayers();
        }
    }
}

void APTLobbyGameMode::PreLogin(const FString& Options, const FString& Address,
                                const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (!ErrorMessage.IsEmpty()) return; // ya rechazado por otra razón

    const FString Attempt = UGameplayStatics::ParseOption(Options, TEXT("Password"));

    if (UMultiplayerSessionsSubsystem* Sessions =
            GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>())
    {
        if (!Sessions->DoesHostPasswordMatch(Attempt))
        {
            ErrorMessage = TEXT("WrongPassword");
            UE_LOG(LogTemp, Warning, TEXT("[Lobby] PreLogin rechazado: contraseña incorrecta."));
        }
    }
}

void APTLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (APTPlayerState* PS = NewPlayer->GetPlayerState<APTPlayerState>())
    {
        // En listen server, el controlador del host es local en el servidor.
        const bool bIsHost = NewPlayer->IsLocalController();
        PS->Server_SetHost(bIsHost);

        // El nombre real de Steam llega como "?Name=" en la URL de travel (ver PTMainMenuWidget);
        // el motor ya lo deja en PlayerState->GetPlayerName() antes de PostLogin (InitNewPlayer).
        FString Name = NewPlayer->PlayerState ? NewPlayer->PlayerState->GetPlayerName() : FString();
        if (Name.IsEmpty())
        {
            Name = bIsHost ? TEXT("Host") : FString::Printf(TEXT("Player_%d"), PlayersJoined + 1);
        }
        PS->Server_SetDisplayName(Name);
    }

    ++PlayersJoined;
    UE_LOG(LogTemp, Log, TEXT("[Lobby] PostLogin. Jugadores conectados: %d"), PlayersJoined);
}

void APTLobbyGameMode::Logout(AController* Exiting)
{
    --PlayersJoined;
    UE_LOG(LogTemp, Log, TEXT("[Lobby] Logout. Jugadores conectados: %d"), PlayersJoined);

    // Se fue el último jugador: no dejar la sesión como sala fantasma.
    // (La migración de host —cuando se va el host pero quedan otros— es trabajo aparte, todavía no hecho.)
    if (PlayersJoined <= 0)
    {
        if (UMultiplayerSessionsSubsystem* Sessions =
                GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>())
        {
            UE_LOG(LogTemp, Log, TEXT("[Lobby] Último jugador se fue, destruyendo la sesión."));
            Sessions->DestroySession();
        }
    }

    Super::Logout(Exiting);
}
