// Copyright Epic Games, Inc. All Rights Reserved.
// Menú de Escape del lobby: Settings (reusa UPTSettingsWidget) y Leave Game (vuelve al Main Menu).

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../UI/PTUserWidget.h"
#include "PTLobbyEscapeMenuWidget.generated.h"

class UButton;
class UPTSettingsWidget;

UCLASS()
class MYPARTYGAME_API UPTLobbyEscapeMenuWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Alternar abierto/cerrado. Llamar desde el PlayerController al togglear el input de Escape. */
    UFUNCTION(BlueprintCallable, Category = "Lobby")
    void ToggleMenu();

    /** Navegación por Esc de dos niveles: si el settings está abierto vuelve al menú;
     *  si no, abre/cierra el menú completo. Ambos levels llaman a esto. */
    UFUNCTION(BlueprintCallable, Category = "Lobby")
    void HandleEscape();

    bool IsMenuOpen() const;

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget))         UButton* LeaveGameButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* SettingsButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* ResumeButton;
    // Solo en modo AUTORÍA de mapa: guarda el escenario esculpido. Se muestra únicamente en ese modo.
    UPROPERTY(meta = (BindWidgetOptional)) UButton* SaveMapButton;

    // Popup "¿salir sin guardar?" (solo autoría, al tocar Salir). Arranca oculto.
    UPROPERTY(meta = (BindWidgetOptional)) UWidget* DiscardPopup;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* DiscardDontSaveButton; // salir sin guardar
    UPROPERTY(meta = (BindWidgetOptional)) UButton* DiscardSaveButton;     // abre el form de Guardar

    UPROPERTY(meta = (BindWidgetOptional)) UPTSettingsWidget* SettingsPanel;
    // Formulario de Guardar mapa (título/desc/miniatura). Asignar WBP_SaveMap (deriva de PTSaveMapWidget).
    UPROPERTY(EditAnywhere, Category = "MapMod") TSubclassOf<class UPTSaveMapWidget> SaveMapClass;
    UPROPERTY(Transient)   class UPTSaveMapWidget* SaveMapForm = nullptr;

    UFUNCTION() void OnLeaveGameClicked();
    UFUNCTION() void OnSettingsClicked();
    UFUNCTION() void OnResumeClicked();
    UFUNCTION() void OnSaveMapClicked();
    UFUNCTION() void OnDiscardDontSave(); // salir sin guardar
    UFUNCTION() void OnDiscardSave();     // abre el form de Guardar

private:
    bool IsMapAuthorMode() const; // el GameMode actual es APTMapAuthorGameMode
    void DoLeaveGame();           // muestra la transición (AnimIn) y al taparse hace la salida real
    UFUNCTION() void DoLeaveGameNow(); // la salida real (host-leave o OpenLevel MainMenu)
    bool bLeaving = false;             // guard: no salir dos veces (OnCovered + timer de seguridad)
    FTimerHandle LeaveSafetyTimer;
};
