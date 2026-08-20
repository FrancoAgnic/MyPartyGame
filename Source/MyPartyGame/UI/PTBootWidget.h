// Copyright Epic Games, Inc. All Rights Reserved.
// Widget del level de arranque ("boot"). Flujo:
//   1) 1er arranque → selección de idioma.
//   2) Al aplicar el idioma (o directo si ya se eligió) → reproduce TitleAnim (aparición + borrado
//      del título; el borrado se keyframea en la propia animación, sobre el param del material).
//   3) Al terminar TitleAnim → viaja al MainMenu (que reproduce su animación de entrada).

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

    // Animación del título: aparición + borrado (efecto goma). Crearla en el WBP como "TitleAnim".
    // El borrado se maneja keyframeando el ScalarParameter del material del título DENTRO de esta anim.
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) UWidgetAnimation* TitleAnim = nullptr;

    // Selección de idioma (overlay, Collapsed por default). Solo el 1er arranque.
    UPROPERTY(meta = (BindWidgetOptional)) UPTLanguageSelectWidget* LanguageSelectPanel = nullptr;

    UPROPERTY(EditAnywhere, Category = "Boot") FString MainMenuMap = TEXT("MainMenu");
    // Si no hay TitleAnim: cuántos segundos esperar antes de ir al menú.
    UPROPERTY(EditAnywhere, Category = "Boot") float FallbackTitleSeconds = 3.0f;

    UFUNCTION() void OnTitleFinished();

private:
    void StartTitleSequence();
    void GoToMainMenu();
    FTimerHandle TitleTimer;
};
