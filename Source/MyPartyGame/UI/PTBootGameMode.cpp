// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTBootGameMode.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"

APTBootGameMode::APTBootGameMode()
{
    // Level de solo-UI: no hace falta pawn (el widget del boot tapa la pantalla).
    DefaultPawnClass = nullptr;
}

void APTBootGameMode::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;

    // Fijar la vista a la cámara del level (ACameraActor con tag "DioramaCam"), igual que el lobby.
    // Sin esto el PlayerController (que no tiene pawn) mira desde el origen y no se ve el diorama.
    TArray<AActor*> Cams;
    UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), ACameraActor::StaticClass(), BootCameraTag, Cams);
    if (Cams.Num() > 0)
    {
        PC->SetViewTargetWithBlend(Cams[0], 0.f);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Boot] No encontré una ACameraActor con tag '%s' en el level."),
               *BootCameraTag.ToString());
    }

    // Widget del boot (logo/título + idioma).
    if (BootWidgetClass)
    {
        if (UUserWidget* W = CreateWidget<UUserWidget>(PC, BootWidgetClass))
            W->AddToViewport(1000); // arriba de todo
    }

    // Cursor visible + input a la UI (para el combo de idioma del primer arranque).
    PC->SetShowMouseCursor(true);
    FInputModeUIOnly Mode;
    PC->SetInputMode(Mode);
}
