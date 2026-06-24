// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 2 — Diálogo para pedir contraseña al unirse a una sesión protegida.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTPasswordPromptWidget.generated.h"

class UButton;
class UEditableTextBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPasswordConfirmed, const FString&, Password);

UCLASS()
class MYPARTYGAME_API UPTPasswordPromptWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Sessions")
    FOnPasswordConfirmed OnPasswordConfirmed;

    UFUNCTION(BlueprintCallable, Category = "Sessions")
    void ShowPrompt();

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget)) UEditableTextBox* PasswordInput;
    UPROPERTY(meta = (BindWidget)) UButton* ConfirmButton;
    UPROPERTY(meta = (BindWidget)) UButton* CancelButton;

    UFUNCTION() void OnConfirmClicked();
    UFUNCTION() void OnCancelClicked();
};
