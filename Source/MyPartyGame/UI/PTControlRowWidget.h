// Copyright Epic Games, Inc. All Rights Reserved.
// Una fila de la lista de controles del HUD: "TECLA — Acción".
// El HUD crea una por cada entrada de PTInput::GetBindings() (fuente de verdad única), así que
// cuando exista el rebind en Settings, la UI se actualiza sola.
//
// En el WBP derivado nombrar los TextBlock: "KeyText" y "ActionText" (ambos opcionales).

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTControlRowWidget.generated.h"

class UTextBlock;

UCLASS()
class MYPARTYGAME_API UPTControlRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Llena la fila. KeyName ya viene con el nombre lindo de la tecla (ej: "Left Mouse Button"). */
    UFUNCTION(BlueprintCallable, Category="UI")
    void SetRow(const FText& ActionLabel, const FText& KeyName);

protected:
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* KeyText;
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* ActionText;
};
