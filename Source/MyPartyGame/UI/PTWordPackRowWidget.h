// Copyright Epic Games, Inc. All Rights Reserved.
// Fila de un banco de palabras en el panel del lobby (título + autor + botón Usar).
// La llena UPTWordPackWidget con un FPTWordPack.
//
// En el WBP derivado (nombres EXACTOS):
//   TitleText  (TextBlock) → título del banco
//   AuthorText (TextBlock) → "por <autor>"  (opcional)
//   UseButton  (Button)    → elegir este banco para la partida
//   UseButtonText (TextBlock) → texto del botón (opcional; "Usar"/"En uso")

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "Mods/PTWordPackSubsystem.h" // FPTWordPack
#include "PTWordPackRowWidget.generated.h"

class UTextBlock;
class UButton;
class UPTWordPackWidget;

UCLASS()
class MYPARTYGAME_API UPTWordPackRowWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Carga la fila. bSelected = es el banco elegido ahora (deshabilita el botón y muestra "En uso"). */
    void Init(const FPTWordPack& InPack, bool bSelected, UPTWordPackWidget* InOwner);

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget))         UTextBlock* TitleText;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* AuthorText;
    UPROPERTY(meta = (BindWidget))         UButton*    UseButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* UseButtonText;

    UFUNCTION() void OnUseClicked();

private:
    FString PackId;
    UPROPERTY() UPTWordPackWidget* Owner = nullptr;
};
