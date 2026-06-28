// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTGameInstance.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

void UPTGameInstance::Init()
{
    Super::Init();

    if (GEngine)
    {
        GEngine->OnNetworkFailure().AddUObject(this, &UPTGameInstance::HandleNetworkFailure);
        GEngine->OnTravelFailure().AddUObject(this, &UPTGameInstance::HandleTravelFailure);
    }
}

void UPTGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
                                           ENetworkFailure::Type FailureType,
                                           const FString& ErrorString)
{
    // Con varios clientes en PIE comparten el mismo GEngine: ignorar fallos de otros mundos.
    if (World != GetWorld()) return;

    // FailureReceived = el servidor cerró la conexión con un ErrorMessage explícito
    // (ej. el "WrongPassword" que pone PTLobbyGameMode::PreLogin). Reintentar no arregla
    // una contraseña incorrecta, así que va directo al menú.
    if (FailureType == ENetworkFailure::FailureReceived)
    {
        ReturnToMainMenuWithError(ErrorString);
        return;
    }

    // Estos son caídas reales de la conexión (no eventos de red durante ServerTravel) —
    // acá sí vale la pena reintentar antes de rendirse y volver al menú.
    const bool bDroppedConnection =
        FailureType == ENetworkFailure::PendingConnectionFailure ||
        FailureType == ENetworkFailure::ConnectionTimeout         ||
        FailureType == ENetworkFailure::ConnectionLost;

    if (bDroppedConnection && TryReconnect(World))
    {
        return; // reintento en curso, todavía no volver al menú
    }

    if (bDroppedConnection)
    {
        ReturnToMainMenuWithError(ErrorString);
    }
}

bool UPTGameInstance::TryReconnect(UWorld* World)
{
    if (PendingReconnectURL.IsEmpty() || ReconnectAttemptsRemaining <= 0)
    {
        return false;
    }

    --ReconnectAttemptsRemaining;
    UE_LOG(LogTemp, Warning, TEXT("[GameInstance] Conexión perdida. Reintentando en %.0fs (quedan %d intento(s))..."),
        ReconnectRetryDelaySeconds, ReconnectAttemptsRemaining);

    FTimerHandle Unused;
    World->GetTimerManager().SetTimer(
        Unused, FTimerDelegate::CreateUObject(this, &UPTGameInstance::DoReconnectAttempt),
        ReconnectRetryDelaySeconds, false);

    return true;
}

void UPTGameInstance::DoReconnectAttempt()
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (!PC)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[GameInstance] Reintentando conexión: %s"), *PendingReconnectURL);
    PC->ClientTravel(PendingReconnectURL, ETravelType::TRAVEL_Absolute);
}

void UPTGameInstance::NotifyJoinedServer(const FString& TravelURL)
{
    PendingReconnectURL      = TravelURL;
    ReconnectAttemptsRemaining = MaxReconnectAttempts;
}

void UPTGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType,
                                          const FString& ErrorString)
{
    // Con varios clientes en PIE comparten el mismo GEngine: ignorar fallos de otros mundos.
    if (World != GetWorld()) return;

    // Solo actuar si hay un error real (string no vacío).
    if (!ErrorString.IsEmpty())
    {
        ReturnToMainMenuWithError(ErrorString);
    }
}

void UPTGameInstance::ReturnToMainMenuWithError(const FString& ErrorString)
{
    // Traducir el token del servidor a un mensaje amigable.
    if (ErrorString.Contains(TEXT("WrongPassword")))
        PendingConnectError = TEXT("Contraseña incorrecta");
    else if (!ErrorString.IsEmpty())
        PendingConnectError = TEXT("No se pudo conectar a la sesión");

    // Se rindió definitivamente: no dejar un intento de reconexión colgado para la próxima sesión.
    PendingReconnectURL.Reset();
    ReconnectAttemptsRemaining = 0;

    // Volver al mapa del menú principal.
    UGameplayStatics::OpenLevel(this, FName("MainMenu"));
}

FString UPTGameInstance::ConsumePendingConnectError()
{
    const FString Out = PendingConnectError;
    PendingConnectError.Reset();
    return Out;
}
