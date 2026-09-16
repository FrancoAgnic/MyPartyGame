// Copyright Epic Games, Inc. All Rights Reserved.
// Popup del LOBBY: cuando el host elige un mapa custom que este cliente no tiene, aparece este popup
// con el nombre del mapa y un solo botón "Descargar". Al tocarlo, se suscribe+descarga por Steam.
//
// En el WBP derivado (parent = PTMapDownloadPromptWidget) — nombres EXACTOS (opcionales):
//   MapNameText    (TextBlock) → se rellena con el nombre del mapa a descargar
//   DownloadButton (Button)    → botón "Descargar" (al tocarlo baja el mapa y cierra el popup)
// No hace falta tocar el Graph del Blueprint: toda la lógica está en C++.

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTMapDownloadPromptWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class MYPARTYGAME_API UPTMapDownloadPromptWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Muestra el popup para descargar el mapa 'MapTitle'. */
    void ShowFor(const FString& MapTitle);
    /** Oculta el popup. */
    void HidePanel();

protected:
    virtual void NativeConstruct() override;

    // Nombres EXACTOS en el WBP:
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* MapNameText;    // nombre del mapa a descargar
    UPROPERTY(meta=(BindWidgetOptional)) UButton*    DownloadButton; // botón "Descargar"

    UFUNCTION() void OnDownloadClicked();
};
