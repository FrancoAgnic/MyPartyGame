// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 4 — GameInstance personalizado. Captura fallos de red/viaje y vuelve al MainMenu.

#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineTypes.h"
#include "PTMatchSettings.h"
#include "PTGameInstance.generated.h"

UCLASS()
class MYPARTYGAME_API UPTGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

    /** El menú lo consume al reabrirse: devuelve el error y lo limpia. */
    UFUNCTION(BlueprintCallable, Category="Session")
    FString ConsumePendingConnectError();

    /** Llamar justo antes del ClientTravel exitoso a un servidor, para poder reintentar si se cae. */
    void NotifyJoinedServer(const FString& TravelURL);

    // Config de partida que arma el HOST en el lobby (tiempo/rondas/revelado/categorías/
    // dificultad/CSV). El GameInstance sobrevive al seamless travel, así que el host la setea
    // en el lobby y APTSculptGameMode la lee al arrancar en Lvl-01. Es del host/servidor; a los
    // clientes se les replica un resumen aparte por el GameState (para mostrarla en el lobby).
    UPROPERTY(BlueprintReadWrite, Category="Match")
    FPTMatchSettings PendingMatchSettings;

    // Abre un diálogo nativo para elegir un .csv y carga sus palabras en
    // PendingMatchSettings.CustomWords (+ bUseCustomWords=true). Formato por línea:
    // "Palabra,Categoria,Dificultad" (Dificultad: Facil/Media/Dificil o 1/2/3; categoría y
    // dificultad opcionales). Devuelve cuántas palabras cargó (0 = canceló o archivo vacío).
    UFUNCTION(BlueprintCallable, Category="Match")
    int32 LoadCustomWordsFromCSVDialog();

    // Parsea un CSV de una ruta concreta (lo usa el diálogo; separado para testear).
    int32 LoadCustomWordsFromCSVFile(const FString& Path);

    // Vuelve al banco default (descarta las palabras del CSV).
    UFUNCTION(BlueprintCallable, Category="Match")
    void ClearCustomWords();

private:
    void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
                              ENetworkFailure::Type FailureType, const FString& ErrorString);
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType,
                             const FString& ErrorString);
    void ReturnToMainMenuWithError(const FString& ErrorString);

    // Reconexión — solo aplica a clientes (el host no se reconecta a sí mismo).
    bool TryReconnect(UWorld* World);
    void DoReconnectAttempt();

    static constexpr int32 MaxReconnectAttempts      = 3;
    static constexpr float ReconnectRetryDelaySeconds = 2.0f;

    FString PendingConnectError;
    FString PendingReconnectURL;
    int32   ReconnectAttemptsRemaining = 0;
};
