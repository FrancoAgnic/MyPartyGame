// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTWorkshopBrowserWidget.h"
#include "PTWorkshopItemRowWidget.h"
#include "../PTTextTable.h"
#include "Mods/PTWordPackSubsystem.h"
#include "Components/PanelWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"

// Tag del catálogo para cada sección.
static const TCHAR* PT_TAG_WORDBANK = TEXT("WordBank");

UPTWordPackSubsystem* UPTWorkshopBrowserWidget::Packs() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTWordPackSubsystem>() : nullptr;
}

void UPTWorkshopBrowserWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SearchButton)       SearchButton->OnClicked.AddDynamic(this, &UPTWorkshopBrowserWidget::OnSearchClicked);
    if (WordBanksTabButton) WordBanksTabButton->OnClicked.AddDynamic(this, &UPTWorkshopBrowserWidget::OnWordBanksTabClicked);
    if (MapsTabButton)      MapsTabButton->OnClicked.AddDynamic(this, &UPTWorkshopBrowserWidget::OnMapsTabClicked);
    if (BackButton)         BackButton->OnClicked.AddDynamic(this, &UPTWorkshopBrowserWidget::OnBackClicked);
    if (SearchBox)          SearchBox->OnTextCommitted.AddDynamic(this, &UPTWorkshopBrowserWidget::OnSearchCommitted);

    if (TitleText) TitleText->SetText(PTText::Get(TEXT("WORKSHOP_TITLE")));

    if (UPTWordPackSubsystem* P = Packs())
    {
        if (!bBound)
        {
            P->OnWorkshopSearchComplete.AddUObject(this, &UPTWorkshopBrowserWidget::OnSearchComplete);
            bBound = true;
        }
    }

    SwitchTab(0);
}

void UPTWorkshopBrowserWidget::NativeDestruct()
{
    if (UPTWordPackSubsystem* P = Packs())
    {
        if (bBound) { P->OnWorkshopSearchComplete.RemoveAll(this); bBound = false; }
    }
    Super::NativeDestruct();
}

void UPTWorkshopBrowserWidget::ShowPanel()
{
    SetVisibility(ESlateVisibility::Visible);
    SwitchTab(0);
}

void UPTWorkshopBrowserWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTWorkshopBrowserWidget::OnWordBanksTabClicked() { SwitchTab(0); }
void UPTWorkshopBrowserWidget::OnMapsTabClicked()      { SwitchTab(1); }

void UPTWorkshopBrowserWidget::SwitchTab(int32 Tab)
{
    ActiveTab = Tab;
    const bool bMaps = (ActiveTab == 1);

    // Mapas: BLOQUEADO por ahora → overlay "Próximamente", sin buscador ni resultados.
    if (MapsLockedPanel) MapsLockedPanel->SetVisibility(bMaps ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (ResultsBox)      ResultsBox->SetVisibility(bMaps ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (SearchBox)       SearchBox->SetIsEnabled(!bMaps);
    if (SearchButton)    SearchButton->SetIsEnabled(!bMaps);

    ApplyTabVisual();

    if (!bMaps)
    {
        RunSearch(); // al entrar a Bancos, mostrar populares (búsqueda vacía)
    }
    else if (EmptyText)
    {
        EmptyText->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UPTWorkshopBrowserWidget::ApplyTabVisual()
{
    const bool bWB = (ActiveTab == 0);
    if (WordBanksTabButton)
    {
        WordBanksTabButton->SetBackgroundColor(bWB ? TabActiveColor : TabInactiveColor);
        WordBanksTabButton->SetRenderOpacity(bWB ? 1.0f : 0.45f);
    }
    if (MapsTabButton)
    {
        MapsTabButton->SetBackgroundColor(!bWB ? TabActiveColor : TabInactiveColor);
        MapsTabButton->SetRenderOpacity(!bWB ? 1.0f : 0.45f);
    }
}

void UPTWorkshopBrowserWidget::OnSearchClicked() { RunSearch(); }

void UPTWorkshopBrowserWidget::OnSearchCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    if (CommitType == ETextCommit::OnEnter) RunSearch();
}

void UPTWorkshopBrowserWidget::RunSearch()
{
    if (ActiveTab != 0) return; // mapas bloqueado
    UPTWordPackSubsystem* P = Packs();
    if (!P) return;

    const FString Text = SearchBox ? SearchBox->GetText().ToString() : FString();
    if (StatusText)
    {
        StatusText->SetText(PTText::Get(TEXT("WORKSHOP_SEARCHING")));
        StatusText->SetVisibility(ESlateVisibility::Visible);
    }
    if (EmptyText) EmptyText->SetVisibility(ESlateVisibility::Collapsed);

    P->SearchWorkshop(Text, PT_TAG_WORDBANK);
}

void UPTWorkshopBrowserWidget::OnSearchComplete(const TArray<FPTWorkshopItem>& Items, bool bOk)
{
    if (StatusText) StatusText->SetVisibility(ESlateVisibility::Collapsed);
    if (!ResultsBox) return;

    ResultsBox->ClearChildren();

    int32 Count = 0;
    if (bOk && RowWidgetClass)
    {
        for (const FPTWorkshopItem& It : Items)
        {
            UPTWorkshopItemRowWidget* Row = CreateWidget<UPTWorkshopItemRowWidget>(this, RowWidgetClass);
            if (!Row) continue;
            Row->Init(It, this);
            ResultsBox->AddChild(Row);
            ++Count;
        }
    }

    if (EmptyText)
    {
        EmptyText->SetText(PTText::Get(TEXT("WORKSHOP_EMPTY")));
        EmptyText->SetVisibility(Count == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UPTWorkshopBrowserWidget::AddItem(const FString& ItemId)
{
    if (UPTWordPackSubsystem* P = Packs()) P->SubscribeItem(ItemId);
}
