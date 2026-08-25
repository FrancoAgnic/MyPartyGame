// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTGameSettingsWidget.h"
#include "../PTGameInstance.h"
#include "../PTWordBank.h"
#include "../PTTextTable.h"
#include "../Multiplayer/MultiplayerSessionsSubsystem.h"
#include "../UI/PTWordPackWidget.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Blueprint/WidgetTree.h"

bool UPTGameSettingsWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (TurnTimeMinus) TurnTimeMinus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnTurnTimeMinus);
    if (TurnTimePlus)  TurnTimePlus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnTurnTimePlus);
    if (RoundsMinus)   RoundsMinus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnRoundsMinus);
    if (RoundsPlus)    RoundsPlus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnRoundsPlus);
    if (RevealMinus)   RevealMinus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnRevealMinus);
    if (RevealPlus)    RevealPlus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnRevealPlus);
    if (DiffFacilButton)   DiffFacilButton->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnDiffFacil);
    if (DiffMediaButton)   DiffMediaButton->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnDiffMedia);
    if (DiffDificilButton) DiffDificilButton->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnDiffDificil);
    if (FriendsOnlyCheckbox) FriendsOnlyCheckbox->OnCheckStateChanged.AddDynamic(this, &UPTGameSettingsWidget::OnFriendsOnlyChanged);
    if (LibraryButton) LibraryButton->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnLibraryClicked);
    if (CloseButton)   CloseButton->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnCloseClicked);

    if (LibraryPanel) LibraryPanel->SetVisibility(ESlateVisibility::Collapsed);
    return true;
}

UPTGameInstance* UPTGameSettingsWidget::GetGI() const
{
    return GetWorld() ? GetWorld()->GetGameInstance<UPTGameInstance>() : nullptr;
}

void UPTGameSettingsWidget::ShowPanel()
{
    SetVisibility(ESlateVisibility::Visible);
    if (!bCategoryChecksBuilt) BuildCategoryChecks();
    // Inicializar el toggle de visibilidad con el estado actual de la sesión.
    if (FriendsOnlyCheckbox)
        if (UMultiplayerSessionsSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>() : nullptr)
            FriendsOnlyCheckbox->SetIsChecked(S->IsSessionFriendsOnly());
    RefreshUI();
    PlayPopIn();
}

void UPTGameSettingsWidget::OnCloseClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTGameSettingsWidget::OnFriendsOnlyChanged(bool bIsChecked)
{
    if (UMultiplayerSessionsSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>() : nullptr)
        S->SetSessionFriendsOnly(bIsChecked);
}

void UPTGameSettingsWidget::OnLibraryClicked()
{
    if (LibraryPanel) LibraryPanel->ShowPanel();
}

void UPTGameSettingsWidget::BuildCategoryChecks()
{
    if (bCategoryChecksBuilt || !CategoriesBox || !WidgetTree) return;
    bCategoryChecksBuilt = true;

    CategoryNames = PTWordBank::GetDefaultCategories();
    CategoryChecks.Reset();

    const int32 Cols = 3;
    UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
    if (!Grid) return;
    Grid->SetSlotPadding(FMargin(2.f, 2.f));
    CategoriesBox->AddChild(Grid);

    UPTGameInstance* GI = GetGI();
    for (int32 i = 0; i < CategoryNames.Num(); ++i)
    {
        const FName Cat = CategoryNames[i];
        UHorizontalBox* Cell  = WidgetTree->ConstructWidget<UHorizontalBox>();
        UCheckBox*      Chk   = WidgetTree->ConstructWidget<UCheckBox>();
        UTextBlock*     Label = WidgetTree->ConstructWidget<UTextBlock>();
        if (!Cell || !Chk || !Label) continue;

        const bool bActive = !GI || GI->PendingMatchSettings.ActiveCategories.Num() == 0
                          || GI->PendingMatchSettings.ActiveCategories.Contains(Cat);
        Chk->SetIsChecked(bActive);
        Chk->OnCheckStateChanged.AddDynamic(this, &UPTGameSettingsWidget::OnCategoryChanged);

        Label->SetText(FText::FromString(Cat.ToString().Replace(TEXT("_"), TEXT(" "))));

        Cell->AddChild(Chk);
        if (UHorizontalBoxSlot* LSlot = Cast<UHorizontalBoxSlot>(Cell->AddChild(Label)))
            LSlot->SetPadding(FMargin(4.f, 0.f, 8.f, 0.f));

        if (UUniformGridSlot* GSlot = Cast<UUniformGridSlot>(Grid->AddChildToUniformGrid(Cell, i / Cols, i % Cols)))
            GSlot->SetHorizontalAlignment(HAlign_Left);

        CategoryChecks.Add(Chk);
    }
}

void UPTGameSettingsWidget::OnCategoryChanged(bool /*bChecked*/)
{
    UPTGameInstance* GI = GetGI();
    if (!GI) return;

    TArray<FName> Active;
    for (int32 i = 0; i < CategoryChecks.Num(); ++i)
        if (CategoryChecks[i] && CategoryChecks[i]->IsChecked() && CategoryNames.IsValidIndex(i))
            Active.Add(CategoryNames[i]);

    if (Active.Num() == CategoryChecks.Num()) Active.Reset();
    GI->PendingMatchSettings.ActiveCategories = Active;
}

void UPTGameSettingsWidget::RefreshUI()
{
    UPTGameInstance* GI = GetGI();
    if (!GI) return;
    const FPTMatchSettings& S = GI->PendingMatchSettings;

    if (TurnTimeText) TurnTimeText->SetText(FText::FromString(FString::Printf(TEXT("%d s"), FMath::RoundToInt(S.TurnDuration))));
    if (RoundsText)   RoundsText->SetText(FText::FromString(FString::Printf(TEXT("%d"), S.NumRounds)));
    if (RevealText)   RevealText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(S.RevealFraction * 100.f))));

    auto DiffActive = [&S](EPTWordDifficulty D){ return S.ActiveDifficulties.Num() == 0 || S.ActiveDifficulties.Contains(D); };
    auto Paint = [](UButton* B, bool bOn){ if (B) B->SetBackgroundColor(bOn ? FLinearColor(0.47f, 0.87f, 0.50f) : FLinearColor(0.45f, 0.45f, 0.45f)); };
    Paint(DiffFacilButton,   DiffActive(EPTWordDifficulty::Facil));
    Paint(DiffMediaButton,   DiffActive(EPTWordDifficulty::Media));
    Paint(DiffDificilButton, DiffActive(EPTWordDifficulty::Dificil));

    const bool bCustom = S.bUseCustomWords && S.CustomWords.Num() > 0;
    for (UCheckBox* C : CategoryChecks) if (C) C->SetIsEnabled(!bCustom);
}

void UPTGameSettingsWidget::ToggleDifficulty(EPTWordDifficulty Diff)
{
    UPTGameInstance* GI = GetGI();
    if (!GI) return;
    TArray<EPTWordDifficulty>& A = GI->PendingMatchSettings.ActiveDifficulties;

    if (A.Num() == 0) A = { EPTWordDifficulty::Facil, EPTWordDifficulty::Media, EPTWordDifficulty::Dificil };
    if (A.Contains(Diff)) A.Remove(Diff); else A.AddUnique(Diff);
    if (A.Num() == 0) A = { EPTWordDifficulty::Facil, EPTWordDifficulty::Media, EPTWordDifficulty::Dificil };
    if (A.Num() == 3) A.Reset();
    RefreshUI();
}

void UPTGameSettingsWidget::OnTurnTimeMinus() { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.TurnDuration = FMath::Clamp(GI->PendingMatchSettings.TurnDuration - 15.f, 15.f, 300.f); RefreshUI(); } }
void UPTGameSettingsWidget::OnTurnTimePlus()  { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.TurnDuration = FMath::Clamp(GI->PendingMatchSettings.TurnDuration + 15.f, 15.f, 300.f); RefreshUI(); } }
void UPTGameSettingsWidget::OnRoundsMinus()   { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.NumRounds = FMath::Clamp(GI->PendingMatchSettings.NumRounds - 1, 1, 10); RefreshUI(); } }
void UPTGameSettingsWidget::OnRoundsPlus()    { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.NumRounds = FMath::Clamp(GI->PendingMatchSettings.NumRounds + 1, 1, 10); RefreshUI(); } }
void UPTGameSettingsWidget::OnRevealMinus()   { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.RevealFraction = FMath::Clamp(GI->PendingMatchSettings.RevealFraction - 0.1f, 0.f, 0.9f); RefreshUI(); } }
void UPTGameSettingsWidget::OnRevealPlus()    { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.RevealFraction = FMath::Clamp(GI->PendingMatchSettings.RevealFraction + 0.1f, 0.f, 0.9f); RefreshUI(); } }
void UPTGameSettingsWidget::OnDiffFacil()   { ToggleDifficulty(EPTWordDifficulty::Facil); }
void UPTGameSettingsWidget::OnDiffMedia()   { ToggleDifficulty(EPTWordDifficulty::Media); }
void UPTGameSettingsWidget::OnDiffDificil() { ToggleDifficulty(EPTWordDifficulty::Dificil); }
