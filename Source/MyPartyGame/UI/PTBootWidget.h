// Copyright Epic Games, Inc. All Rights Reserved.
// Widget del level de arranque ("boot"). Flujo:
//   1) Si es el PRIMER arranque → primero la selección de idioma.
//   2) Al aplicar el idioma (o directo si ya se eligió) → aparece el TÍTULO y se desvanece.
//   3) Al terminar la animación del título → viaja al MainMenu (que reproduce su propia animación
//      de entrada, "SpawnMainMenu").

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

    // Animación del título: aparece y se DESVANECE (varios segundos). Crearla en el WBP con nombre
    // "TitleAnim". El título debe arrancar OCULTO (opacity 0) para que no se vea durante el idioma;
    // esta animación lo muestra y lo funde. Al terminar → MainMenu.
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) UWidgetAnimation* TitleAnim = nullptr;

    // Selección de idioma (overlay a pantalla completa, Collapsed por default). Solo en el 1er arranque.
    UPROPERTY(meta = (BindWidgetOptional)) UPTLanguageSelectWidget* LanguageSelectPanel = nullptr;

    // Mapa del menú principal al que se viaja después del título.
    UPROPERTY(EditAnywhere, Category = "Boot") FString MainMenuMap = TEXT("MainMenu");
    // Si no hay TitleAnim, cuántos segundos mostrar el título antes de ir al menú.
    UPROPERTY(EditAnywhere, Category = "Boot") float FallbackTitleSeconds = 3.0f;

    UFUNCTION() void OnTitleFinished();

private:
    void StartTitleSequence(); // muestra el título (o su animación) y al terminar va al menú
    void GoToMainMenu();
    FTimerHandle TitleTimer;
};
