// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTGameSettingsWidget.h"
#include "../PTGameInstance.h"
#include "../PTTextTable.h"
#include "../Multiplayer/MultiplayerSessionsSubsystem.h"
#include "PTLobbyGameMode.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"

bool UPTGameSettingsWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (TurnTimeMinus) TurnTimeMinus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnTurnTimeMinus);
    if (TurnTimePlus)  TurnTimePlus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnTurnTimePlus);
    if (RoundsMinus)   RoundsMinus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnRoundsMinus);
    if (RoundsPlus)    RoundsPlus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnRoundsPlus);
    if (RevealMinus)   RevealMinus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnRevealMinus);
    if (RevealPlus)    RevealPlus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnRevealPlus);
    if (FriendsOnlyCheckbox) FriendsOnlyCheckbox->OnCheckStateChanged.AddDynamic(this, &UPTGameSettingsWidget::OnFriendsOnlyChanged);
    if (LibraryButton) LibraryButton->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnLibraryClicked);
    if (CloseButton)         CloseButton->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnCloseClicked);
    if (Btn_Back)            Btn_Back->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnCloseClicked);
    if (CloseSettingsButton) CloseSettingsButton->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnCloseClicked);

    // Refrescar los textos "Word:/Map:" cuando el host cambia de banco desde la Biblioteca.
    if (UPTGameInstance* GI = GetGI())
        GI->OnSelectedWordPackChanged.AddUObject(this, &UPTGameSettingsWidget::RefreshPackTexts);
    return true;
}

UPTGameInstance* UPTGameSettingsWidget::GetGI() const
{
    return GetWorld() ? GetWorld()->GetGameInstance<UPTGameInstance>() : nullptr;
}

void UPTGameSettingsWidget::ShowPanel()
{
    SetVisibility(ESlateVisibility::Visible);
    // Inicializar el toggle de visibilidad con el estado actual de la sesión.
    if (FriendsOnlyCheckbox)
        if (UMultiplayerSessionsSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>() : nullptr)
            FriendsOnlyCheckbox->SetIsChecked(S->IsSessionFriendsOnly());
    RefreshUI();
    PlayPopIn();

    // Avisar (host) que el panel quedó abierto → los clientes muestran su vista read-only en vivo.
    if (UWorld* W = GetWorld())
        if (APTLobbyGameMode* GM = W->GetAuthGameMode<APTLobbyGameMode>())
            GM->SetHostSettingsPanelOpen(true);
}

void UPTGameSettingsWidget::OnCloseClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);

    // Panel cerrado → ocultar la vista de los clientes.
    if (UWorld* W = GetWorld())
        if (APTLobbyGameMode* GM = W->GetAuthGameMode<APTLobbyGameMode>())
            GM->SetHostSettingsPanelOpen(false);
}

void UPTGameSettingsWidget::PushSettingsToState()
{
    // Solo el host tiene GameMode con autoridad; en clientes GetAuthGameMode == null (no-op).
    if (UWorld* W = GetWorld())
        if (APTLobbyGameMode* GM = W->GetAuthGameMode<APTLobbyGameMode>())
            GM->SyncMatchSettingsToState();
}

void UPTGameSettingsWidget::RefreshPackTexts()
{
    const FText Default = PTText::Get(TEXT("GS_DEFAULT"));
    if (WordBankText)
    {
        const FString Title = GetGI() ? GetGI()->SelectedWordPackTitle : FString();
        const FText Word = Title.IsEmpty() ? Default : FText::FromString(Title);
        FFormatOrderedArguments Args; Args.Add(Word);
        WordBankText->SetText(PTText::Format(TEXT("GS_WORD"), Args));
    }
    if (MapText)
    {
        // Mapas custom bloqueados por ahora → siempre "Default".
        FFormatOrderedArguments Args; Args.Add(Default);
        MapText->SetText(PTText::Format(TEXT("GS_MAP"), Args));
    }

    // Replicar a los clientes (banco de palabras + valores numéricos, ya que RefreshUI pasa por acá).
    PushSettingsToState();
}

void UPTGameSettingsWidget::OnFriendsOnlyChanged(bool bIsChecked)
{
    if (UMultiplayerSessionsSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>() : nullptr)
        S->SetSessionFriendsOnly(bIsChecked);
    PushSettingsToState(); // que los clientes vean el cambio de "Sala privada"
}

void UPTGameSettingsWidget::OnLibraryClicked()
{
    // El WBP_WordPack vive en el HUD; le pedimos que lo abra.
    OnRequestLibrary.ExecuteIfBound();
}

void UPTGameSettingsWidget::RefreshUI()
{
    UPTGameInstance* GI = GetGI();
    if (!GI) return;
    const FPTMatchSettings& S = GI->PendingMatchSettings;

    if (TurnTimeText) TurnTimeText->SetText(FText::FromString(FString::Printf(TEXT("%d s"), FMath::RoundToInt(S.TurnDuration))));
    if (RoundsText)   RoundsText->SetText(FText::FromString(FString::Printf(TEXT("%d"), S.NumRounds)));
    if (RevealText)   RevealText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(S.RevealFraction * 100.f))));

    RefreshPackTexts();
}

void UPTGameSettingsWidget::OnTurnTimeMinus() { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.TurnDuration = FMath::Clamp(GI->PendingMatchSettings.TurnDuration - 15.f, 15.f, 300.f); RefreshUI(); } }
void UPTGameSettingsWidget::OnTurnTimePlus()  { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.TurnDuration = FMath::Clamp(GI->PendingMatchSettings.TurnDuration + 15.f, 15.f, 300.f); RefreshUI(); } }
void UPTGameSettingsWidget::OnRoundsMinus()   { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.NumRounds = FMath::Clamp(GI->PendingMatchSettings.NumRounds - 1, 1, 10); RefreshUI(); } }
void UPTGameSettingsWidget::OnRoundsPlus()    { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.NumRounds = FMath::Clamp(GI->PendingMatchSettings.NumRounds + 1, 1, 10); RefreshUI(); } }
void UPTGameSettingsWidget::OnRevealMinus()   { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.RevealFraction = FMath::Clamp(GI->PendingMatchSettings.RevealFraction - 0.1f, 0.f, 0.9f); RefreshUI(); } }
void UPTGameSettingsWidget::OnRevealPlus()    { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.RevealFraction = FMath::Clamp(GI->PendingMatchSettings.RevealFraction + 0.1f, 0.f, 0.9f); RefreshUI(); } }
