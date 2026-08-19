// Copyright Epic Games, Inc. All Rights Reserved.
// Widget del level de arranque ("boot"): reproduce la animación del logo de la empresa (siempre,
// al abrir el juego) y — solo la primera vez — muestra la selección de idioma. Al terminar viaja
// al MainMenu. Reparentar el WBP a esta clase.

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTBootWidget.generated.h"

class UWidgetAnimation;
class UPTLanguageSelectWidget;

UCLASS()
class MYPARTYGAME_API UPTBootWidget : public UPTUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;

    // Animación del logo de la empresa. Crearla en el WBP con nombre "LogoAnim". Si no existe, se
    // espera FallbackLogoSeconds mostrando el logo estático y se continúa igual.
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) UWidgetAnimation* LogoAnim = nullptr;

    // Pantalla de idioma embebida (overlay a pantalla completa, Collapsed por default). Solo se
    // muestra en el PRIMER arranque; se oculta sola al confirmar y ahí se viaja al menú.
    UPROPERTY(meta = (BindWidgetOptional)) UPTLanguageSelectWidget* LanguageSelectPanel = nullptr;

    // Mapa del menú principal al que se viaja después del logo (+ idioma si es primer arranque).
    UPROPERTY(EditAnywhere, Category = "Boot") FString MainMenuMap = TEXT("MainMenu");
    // Si no hay LogoAnim (por ahora), cuántos segundos esperar antes de continuar. Bajo = arranca
    // rápido a idioma/menú (el logo es un static mesh del level, siempre visible). Subir cuando se
    // agregue la animación, o dejar que la propia LogoAnim marque el tiempo.
    UPROPERTY(EditAnywhere, Category = "Boot") float FallbackLogoSeconds = 0.2f;

    UFUNCTION() void OnLogoFinished();

private:
    void GoToMainMenu();
    void OnLanguageChosen();
    FTimerHandle LogoTimer;
};
