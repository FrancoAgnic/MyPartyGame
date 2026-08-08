// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 4 — GameInstance personalizado. Captura fallos de red/viaje y vuelve al MainMenu.

#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineTypes.h"
#include "PTMatchSettings.h"
#include "PTGameInstance.generated.h"

class USoundBase;
class UUserWidget;

UCLASS()
class MYPARTYGAME_API UPTGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

    /** El menú lo consume al reabrirse: devuelve el error y lo limpia. */
    UFUNCTION(BlueprintCallable, Category="Session")
    FString ConsumePendingConnectError();

    /** Deja un mensaje para mostrar al volver al menú (p.ej. "el anfitrión cerró la partida").
     *  También cancela cualquier reintento de reconexión pendiente: la partida ya no existe. */
    void SetPendingConnectError(const FString& InError);

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

    // ── Sonidos globales de UI ──────────────────────────────────────────────
    // Se asignan UNA vez (en BP_GameInstance o los class defaults) y se aplican a TODOS los botones
    // que ya existen, sin tocar cada uno: ApplyUIButtonSounds recorre el árbol del widget y setea el
    // Hovered/Pressed del estilo de cada UButton (Slate los reproduce solo). Los widgets raíz lo
    // llaman en su NativeConstruct.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI Sound") USoundBase* UIHoverSound = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI Sound") USoundBase* UIClickSound = nullptr;

    /** Aplica los sonidos de UI a todos los botones del widget (recursivo, entra a sub-widgets). */
    UFUNCTION(BlueprintCallable, Category="UI Sound")
    void ApplyUIButtonSounds(class UUserWidget* Root) const;

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
