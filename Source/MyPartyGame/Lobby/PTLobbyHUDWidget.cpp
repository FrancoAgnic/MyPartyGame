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

void UPTLobbyHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Guard anti "UI de lobby en el menú principal": este HUD solo tiene sentido DENTRO de una
    // sesión (host tras ?listen o cliente tras join → NetMode ListenServer/Client). En Standalone
    // todavía estamos en el menú Crear/Unirse (sin sesión) y no corresponde mostrarlo. El flujo
    // normal solo lo crea vía APTLobbyPlayerController::ShowLobbyOverlay en la rama networked, así
    // que si aparece en Standalone es por alguna vía inesperada — auto-ocultarse y dejar aviso.
    if (const UWorld* World = GetWorld())
    {
        if (World->GetNetMode() == NM_Standalone)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Lobby] WBP_LobbyHUD construido en Standalone (menú, sin sesión) — auto-ocultando."));
            SetVisibility(ESlateVisibility::Collapsed);
            RemoveFromParent();
        }
    }
}

void UPTLobbyHUDWidget::ShowHUD()
{
    UE_LOG(LogTemp, Log, TEXT("[Lobby] UPTLobbyHUDWidget::ShowHUD llamado. NetMode=%d"),
        GetWorld() ? (int32)GetWorld()->GetNetMode() : -1);

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

    if (ReadyButtonText)
    {
        ReadyButtonText->SetText(FText::FromString(
            (LocalPS && LocalPS->bIsReady) ? TEXT("Listo ✓") : TEXT("Listo")));
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
