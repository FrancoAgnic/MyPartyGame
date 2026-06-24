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

    // Solo manejar fallos reales de conexión, no eventos de red durante ServerTravel.
    // FailureReceived = el servidor cerró la conexión con un ErrorMessage explícito
    // (ej. el "WrongPassword" que pone PTLobbyGameMode::PreLogin).
    const bool bRealFailure =
        FailureType == ENetworkFailure::PendingConnectionFailure ||
        FailureType == ENetworkFailure::ConnectionTimeout         ||
        FailureType == ENetworkFailure::ConnectionLost            ||
        FailureType == ENetworkFailure::FailureReceived;

    if (bRealFailure)
    {
        ReturnToMainMenu(ErrorString);
    }
}

void UPTGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType,
                                          const FString& ErrorString)
{
    // Con varios clientes en PIE comparten el mismo GEngine: ignorar fallos de otros mundos.
    if (World != GetWorld()) return;

    // Solo actuar si hay un error real (string no vacío).
    if (!ErrorString.IsEmpty())
    {
        ReturnToMainMenu(ErrorString);
    }
}

void UPTGameInstance::ReturnToMainMenu(const FString& ErrorString)
{
    // Traducir el token del servidor a un mensaje amigable.
    if (ErrorString.Contains(TEXT("WrongPassword")))
        PendingConnectError = TEXT("Contraseña incorrecta");
    else if (!ErrorString.IsEmpty())
        PendingConnectError = TEXT("No se pudo conectar a la sesión");

    // Volver al mapa del menú principal.
    UGameplayStatics::OpenLevel(this, FName("MainMenu"));
}

FString UPTGameInstance::ConsumePendingConnectError()
{
    const FString Out = PendingConnectError;
    PendingConnectError.Reset();
    return Out;
}
