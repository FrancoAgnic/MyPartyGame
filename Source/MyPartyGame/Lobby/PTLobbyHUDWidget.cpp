// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyHUDWidget.h"
#include "PTGameState.h"
#include "PTPlayerState.h"
#include "PTLobbyPlayerController.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Kismet/GameplayStatics.h"

bool UPTLobbyHUDWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (CopyCodeButton)  CopyCodeButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnCopyCodeClicked);
    if (LeaveGameButton) LeaveGameButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnLeaveGameClicked);
    if (StartGameButton) StartGameButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnStartGameClicked);
    if (ReadyButton)      ReadyButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnReadyClicked);

    return true;
}

void UPTLobbyHUDWidget::ShowHUD()
{
    // Este HUD solo tiene sentido DENTRO de una sesión (host tras ?listen o cliente tras join →
    // NetMode ListenServer/Client). En Standalone todavía estamos en el menú Crear/Unirse (sin
    // sesión) y mostrarlo lo encimaría al MainMenu. El flujo correcto lo dispara C++ desde
    // APTLobbyPlayerController::ShowLobbyOverlay (solo en la rama networked); si ShowHUD igual
    // llega en Standalone es por un llamado de más (p.ej. un nodo BP viejo "Show HUD" en el
    // Event BeginPlay de BP_LobbyPlayerController) → ignorarlo.
    if (GetWorld() && GetWorld()->GetNetMode() == NM_Standalone)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Lobby] ShowHUD ignorado: NetMode Standalone (menú, sin sesión). ¿Nodo BP 'Show HUD' de más?"));
        return;
    }

    // Idempotente: si ya está en el viewport (ej: lo llamaron dos veces — un nodo BP + el C++),
    // no re-agregarlo ni reiniciar el timer.
    if (IsInViewport()) return;

    AddToViewport();
    SetVisibility(ESlateVisibility::Visible);

    RefreshPlayerList();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            RefreshTimerHandle, this, &UPTLobbyHUDWidget::RefreshPlayerList, 1.0f, true);
    }
}

void UPTLobbyHUDWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RefreshTimerHandle);
    }
    Super::NativeDestruct();
}

void UPTLobbyHUDWidget::RefreshPlayerList()
{
    APTGameState* PTGS = GetWorld() ? GetWorld()->GetGameState<APTGameState>() : nullptr;
    if (!PTGS) return;

    const APTPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APTPlayerState>() : nullptr;
    const bool bLocalIsHost = LocalPS && LocalPS->bIsHost;

    if (PlayersBox)
    {
        PlayersBox->ClearChildren();

        for (APlayerState* PS : PTGS->PlayerArray)
        {
            APTPlayerState* PTPS = Cast<APTPlayerState>(PS);
            if (!PTPS) continue;

            UTextBlock* Row = NewObject<UTextBlock>(this);
            FString Label = PTPS->DisplayName;
            if (PTPS->bIsHost) Label += TEXT(" (Host)");
            Label += PTPS->bIsReady ? TEXT(" — Listo") : TEXT(" — Esperando");
            Row->SetText(FText::FromString(Label));
            PlayersBox->AddChildToVerticalBox(Row);
        }
    }

    if (PlayersCountText)
    {
        PlayersCountText->SetText(FText::FromString(
            FString::Printf(TEXT("%d/%d"), PTGS->PlayerArray.Num(), PTGS->MaxPlayers)));
    }

    if (LobbyStatusText)
    {
        FString Status;
        switch (PTGS->LobbyState)
        {
        case EPTLobbyState::Starting: Status = TEXT("Starting..."); break;
        case EPTLobbyState::InGame:   Status = TEXT("In game");     break;
        default:                      Status = TEXT("Waiting for players...");
        }
        LobbyStatusText->SetText(FText::FromString(Status));
    }

    CachedRoomCode = PTGS->SessionCode;
    const bool bPrivate = !CachedRoomCode.IsEmpty();

    if (PrivateRoomPanel) PrivateRoomPanel->SetVisibility(bPrivate ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (RoomCodeText)
    {
        RoomCodeText->SetVisibility(bPrivate ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (bPrivate) RoomCodeText->SetText(FText::FromString(CachedRoomCode));
    }

    if (StartGameButton)
    {
        StartGameButton->SetVisibility(bLocalIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    // Botón Ready: refleja el estado propio. No listo → "Not Ready" en rojo pastel; listo →
    // "Ready" en verde pastel. Clickearlo alterna (ver OnReadyClicked). SetBackgroundColor tiñe
    // el brush del botón, así que conviene que en el WBP el botón tenga brushes blancos.
    {
        const bool bReady = (LocalPS && LocalPS->bIsReady);
        if (ReadyButtonText)
            ReadyButtonText->SetText(FText::FromString(bReady ? TEXT("Ready") : TEXT("Not Ready")));
        if (ReadyButton)
            ReadyButton->SetBackgroundColor(bReady
                ? FLinearColor(0.60f, 0.87f, 0.62f)    // verde pastel
                : FLinearColor(0.94f, 0.55f, 0.55f));  // rojo pastel
    }

    if (CountdownText)
    {
        const int32 Seconds = PTGS->CountdownSecondsRemaining;
        if (Seconds >= 0)
        {
            CountdownText->SetVisibility(ESlateVisibility::Visible);
            CountdownText->SetText(FText::FromString(FString::Printf(TEXT("Arranca en %d..."), Seconds)));
        }
        else
        {
            CountdownText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void UPTLobbyHUDWidget::OnCopyCodeClicked()
{
    if (!CachedRoomCode.IsEmpty())
    {
        FPlatformApplicationMisc::ClipboardCopy(*CachedRoomCode);
    }
}

void UPTLobbyHUDWidget::OnLeaveGameClicked()
{
    UGameplayStatics::OpenLevel(this, FName("MainMenu"));
}

void UPTLobbyHUDWidget::OnStartGameClicked()
{
    if (APTLobbyPlayerController* PC = Cast<APTLobbyPlayerController>(GetOwningPlayer()))
    {
        PC->Server_RequestStartGame();
    }
}

void UPTLobbyHUDWidget::OnReadyClicked()
{
    const APTPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APTPlayerState>() : nullptr;
    if (!LocalPS) return;

    if (APTLobbyPlayerController* PC = Cast<APTLobbyPlayerController>(GetOwningPlayer()))
    {
        PC->Server_SetReady(!LocalPS->bIsReady);
    }
}
