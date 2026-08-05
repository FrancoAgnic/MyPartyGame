// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyGameMode.h"
#include "PTLobbyCharacter.h"
#include "PTLobbyPlayerController.h"
#include "PTPlayerState.h"
#include "PTGameState.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

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
            const int32 M = Sessions->GetPendingMaxPlayers();
            PTGS->MaxPlayers         = (M > 0) ? M : 10; // sin sesión (PIE/local) → mostrar /10 igual
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

    // bDelayedStart evita que AGameMode arranque el "Match" solo (ver comentario en el
    // constructor), pero como efecto secundario también le impide a HandleStartingNewPlayer
    // llamar a RestartPlayer — sin esto el jugador nunca posee a su Pawn (DefaultPawnClass).
    // Acá sí queremos el Pawn enseguida (el lobby es la sala de espera visible), así que lo
    // forzamos manualmente.
    RestartPlayer(NewPlayer);

    // Un jugador nuevo entra sin listo (bIsReady=false por default): si había countdown en
    // curso, esto lo cancela (CheckReadyState ve que ya no están todos listos).
    CheckReadyState();
}

void APTLobbyGameMode::Logout(AController* Exiting)
{
    --PlayersJoined;
    UE_LOG(LogTemp, Log, TEXT("[Lobby] Logout. Jugadores conectados: %d"), PlayersJoined);

    // Se fue el último jugador: no dejar la sesión como sala fantasma. Pero si este Logout es
    // producto de nuestro propio self-travel (bTravelInProgress), no es un abandono real —
    // no destruir la sesión que se acaba de crear (ver comentario en el header).
    // (La migración de host —cuando se va el host pero quedan otros— es trabajo aparte, todavía no hecho.)
    if (PlayersJoined <= 0 && !bTravelInProgress)
    {
        if (UMultiplayerSessionsSubsystem* Sessions =
                GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>())
        {
            UE_LOG(LogTemp, Log, TEXT("[Lobby] Último jugador se fue, destruyendo la sesión."));
            Sessions->DestroySession();
        }
    }

    Super::Logout(Exiting);

    // Recién ahora Exiting ya no figura en GameState->PlayerArray (Super::Logout dispara su
    // remoción); si quedaba un countdown en curso puede haber que cancelarlo (bajó del mínimo).
    CheckReadyState();
}

void APTLobbyGameMode::HostLeaveGame()
{
    if (!HasAuthority()) return;

    // 1) Avisar a TODOS los clientes para que se vayan al menú por su cuenta. Es un RPC confiable,
    //    así que sale antes de que el mundo del servidor se cierre.
    int32 Notified = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || PC->IsLocalController()) continue; // el host se va en el paso 2
        if (APTPlayerState* PS = PC->GetPlayerState<APTPlayerState>())
        {
            PS->Client_HostClosedGame();
            ++Notified;
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[Lobby] El anfitrión cierra la partida: avisando a %d cliente(s)."), Notified);

    // 2) Darles un instante para procesar el aviso y desconectarse solos. Recién ahí se destruye
    //    la sesión y se va el host. Sin esta espera volvemos al bug: el mundo desaparece con los
    //    clientes todavía conectados.
    const float Delay = (Notified > 0) ? 1.0f : 0.f;
    GetWorldTimerManager().ClearTimer(HostLeaveTimer);
    GetWorldTimerManager().SetTimer(HostLeaveTimer, this, &APTLobbyGameMode::FinishHostLeave, FMath::Max(Delay, 0.01f), false);
}

void APTLobbyGameMode::FinishHostLeave()
{
    if (UMultiplayerSessionsSubsystem* Sessions =
            GetGameInstance() ? GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>() : nullptr)
    {
        Sessions->DestroySession(); // ahora sí: ya no queda nadie conectado
    }
    UGameplayStatics::OpenLevel(this, FName("MainMenu"));
}

void APTLobbyGameMode::TravelToGame()
{
    // Acá sí queremos seamless (sin flash) — puede haberse desactivado para el self-travel
    // de "Crear sesión" en MainMenu (ver PTMainMenuWidget::OnCreateSession).
    bUseSeamlessTravel = true;

    const FString URL = GameMapPath + TEXT("?listen");
    UE_LOG(LogTemp, Log, TEXT("[LobbyGameMode] ServerTravel → %s"), *URL);
    GetWorld()->ServerTravel(URL, /*bAbsolute=*/true);
}

void APTLobbyGameMode::CheckReadyState()
{
    APTGameState* PTGS = GetGameState<APTGameState>();
    if (!PTGS) return;

    bool bAllReady = PTGS->PlayerArray.Num() >= MinPlayersToStart;
    for (APlayerState* PS : PTGS->PlayerArray)
    {
        const APTPlayerState* PTPS = Cast<APTPlayerState>(PS);
        if (!PTPS || !PTPS->bIsReady) { bAllReady = false; break; }
    }

    const bool bCounting = GetWorldTimerManager().IsTimerActive(CountdownTimerHandle);

    if (bAllReady && !bCounting)
    {
        PTGS->CountdownSecondsRemaining = ReadyCountdownSeconds;
        PTGS->LobbyState = EPTLobbyState::Starting;
        GetWorldTimerManager().SetTimer(CountdownTimerHandle, this,
            &APTLobbyGameMode::CountdownTick, 1.f, true);
        UE_LOG(LogTemp, Log, TEXT("[Lobby] Todos listos (%d/%d) — countdown de %d s."),
            PTGS->PlayerArray.Num(), PTGS->MaxPlayers, ReadyCountdownSeconds);
    }
    else if (!bAllReady && bCounting)
    {
        GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
        PTGS->CountdownSecondsRemaining = -1;
        PTGS->LobbyState = EPTLobbyState::WaitingForPlayers;
        UE_LOG(LogTemp, Log, TEXT("[Lobby] Countdown cancelado."));
    }
}

void APTLobbyGameMode::CountdownTick()
{
    APTGameState* PTGS = GetGameState<APTGameState>();
    if (!PTGS) return;

    --PTGS->CountdownSecondsRemaining;
    if (PTGS->CountdownSecondsRemaining <= 0)
    {
        GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
        TravelToGame();
    }
}
