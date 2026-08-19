// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTBootGameMode.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

APTBootGameMode::APTBootGameMode()
{
    // Level de solo-UI: no hace falta pawn (el widget del boot tapa la pantalla).
    DefaultPawnClass = nullptr;
}

void APTBootGameMode::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC || !BootWidgetClass) return;

    if (UUserWidget* W = CreateWidget<UUserWidget>(PC, BootWidgetClass))
        W->AddToViewport(1000); // arriba de todo

    // Cursor visible + input a la UI (para el combo de idioma del primer arranque).
    PC->SetShowMouseCursor(true);
    FInputModeUIOnly Mode;
    PC->SetInputMode(Mode);
}
