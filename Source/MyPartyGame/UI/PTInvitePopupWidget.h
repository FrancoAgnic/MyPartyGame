// Copyright Epic Games, Inc. All Rights Reserved.
// Popup chico (abajo a la derecha) que aparece cuando un amigo te invita a su partida. Muestra el
// nombre del que invita + Aceptar (te une) / Rechazar (lo cierra). Se auto-cierra tras un tiempo.
// Lo crea y muestra el GameInstance (para que salga en cualquier momento: menú, partida, otro popup).
//
// En el WBP derivado (nombres EXACTOS; casi todo opcional):
//   MessageText  (TextBlock) → "<Nombre> te invitó a jugar"
//   AcceptButton (Button)    → aceptar (unirse)
//   RejectButton (Button)    → rechazar (cerrar)
//   TimeBar      (ProgressBar)→ barra de tiempo restante (opcional)

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTInvitePopupWidget.generated.h"

class UTextBlock;
class UButton;
class UProgressBar;
class UMultiplayerSessionsSubsystem;

UCLASS()
class MYPARTYGAME_API UPTInvitePopupWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Configura el popup con el nombre del que invita y el tiempo antes de auto-cerrarse. */
    void Setup(const FString& FromName, float TimeoutSeconds);

protected:
    virtual bool Initialize() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   MessageText;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      AcceptButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      RejectButton;
    UPROPERTY(meta = (BindWidgetOptional)) UProgressBar* TimeBar;

    UFUNCTION() void OnAcceptClicked();
    UFUNCTION() void OnRejectClicked();

private:
    void Dismiss();
    UMultiplayerSessionsSubsystem* Sessions() const;

    float TimeLeft  = 0.f;
    float TimeTotal = 0.f;
    bool  bClosed   = false;
};
