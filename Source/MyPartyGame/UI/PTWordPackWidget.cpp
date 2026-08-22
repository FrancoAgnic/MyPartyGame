// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTWordPackWidget.h"
#include "PTWordPackRowWidget.h"
#include "../PTGameInstance.h"
#include "../PTTextTable.h"
#include "Mods/PTWordPackSubsystem.h"
#include "Components/PanelWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

UPTWordPackSubsystem* UPTWordPackWidget::Packs() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTWordPackSubsystem>() : nullptr;
}

UPTGameInstance* UPTWordPackWidget::GI() const
{
    return Cast<UPTGameInstance>(GetGameInstance());
}

void UPTWordPackWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (RefreshButton) RefreshButton->OnClicked.AddDynamic(this, &UPTWordPackWidget::OnRefreshClicked);
    if (PublishButton) PublishButton->OnClicked.AddDynamic(this, &UPTWordPackWidget::OnPublishClicked);
    if (DefaultButton) DefaultButton->OnClicked.AddDynamic(this, &UPTWordPackWidget::OnDefaultClicked);
    if (BackButton)    BackButton->OnClicked.AddDynamic(this, &UPTWordPackWidget::OnBackClicked);
    if (WordsTabButton) WordsTabButton->OnClicked.AddDynamic(this, &UPTWordPackWidget::OnWordsTabClicked);
    if (MapsTabButton)  MapsTabButton->OnClicked.AddDynamic(this, &UPTWordPackWidget::OnMapsTabClicked);

    if (TitleText)      TitleText->SetText(PTText::Get(TEXT("WORDPACK_TITLE")));
    if (MapsLockedText) MapsLockedText->SetText(PTText::Get(TEXT("MAPS_SOON")));
    if (MapsBox)        MapsBox->ClearChildren(); // mapas bloqueados: lista vacía por ahora

    if (UPTWordPackSubsystem* P = Packs())
    {
        if (!bBound)
        {
            P->OnWordPacksUpdated.AddUObject(this, &UPTWordPackWidget::OnPacksUpdated);
            P->OnWordPackPublished.AddUObject(this, &UPTWordPackWidget::OnPublished);
            bBound = true;
        }
        P->RescanPacks();
    }
    SwitchTab(0); // arranca en Palabras
    Rebuild();
}

void UPTWordPackWidget::OnWordsTabClicked() { SwitchTab(0); }
void UPTWordPackWidget::OnMapsTabClicked()  { SwitchTab(1); }

void UPTWordPackWidget::SwitchTab(int32 Tab)
{
    ActiveTab = Tab;
    const bool bMaps = (ActiveTab == 1);

    // Palabras (funcional) vs Mapas (bloqueado): se muestra una lista u otra, como el Locker.
    if (PacksBox)        PacksBox->SetVisibility(bMaps ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (MapsBox)         MapsBox->SetVisibility(bMaps ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (MapsLockedPanel) MapsLockedPanel->SetVisibility(bMaps ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    // Controles de bancos (publicar/actualizar/default/estado) solo en la pestaña Palabras.
    if (RefreshButton)   RefreshButton->SetVisibility(bMaps ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (PublishButton)   PublishButton->SetVisibility(bMaps ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (DefaultButton)   DefaultButton->SetVisibility(bMaps ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (SelectedText)    SelectedText->SetVisibility(bMaps ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (EmptyText && bMaps) EmptyText->SetVisibility(ESlateVisibility::Collapsed);

    ApplyTabVisual();
}

void UPTWordPackWidget::ApplyTabVisual()
{
    const bool bWords = (ActiveTab == 0);
    if (WordsTabButton)
    {
        WordsTabButton->SetBackgroundColor(bWords ? TabActiveColor : TabInactiveColor);
        WordsTabButton->SetRenderOpacity(bWords ? 1.0f : 0.45f);
    }
    if (MapsTabButton)
    {
        MapsTabButton->SetBackgroundColor(!bWords ? TabActiveColor : TabInactiveColor);
        MapsTabButton->SetRenderOpacity(!bWords ? 1.0f : 0.45f);
    }
}

void UPTWordPackWidget::NativeDestruct()
{
    if (UPTWordPackSubsystem* P = Packs())
    {
        if (bBound)
        {
            P->OnWordPacksUpdated.RemoveAll(this);
            P->OnWordPackPublished.RemoveAll(this);
            bBound = false;
        }
    }
    Super::NativeDestruct();
}

void UPTWordPackWidget::ShowPanel()
{
    SetVisibility(ESlateVisibility::Visible);
    PlayPopIn();
    SwitchTab(0); // siempre abre en Palabras
    if (UPTWordPackSubsystem* P = Packs()) P->RescanPacks();
}

void UPTWordPackWidget::UsePack(const FString& PackId)
{
    if (GI()) GI()->SelectWordPack(PackId);
    Rebuild();
}

void UPTWordPackWidget::OnRefreshClicked()
{
    if (UPTWordPackSubsystem* P = Packs()) P->RescanPacks();
}

void UPTWordPackWidget::OnPublishClicked()
{
    if (GI()) GI()->PublishWordPackFromDialog();
}

void UPTWordPackWidget::OnDefaultClicked()
{
    if (GI()) GI()->SelectDefaultWordBank();
    Rebuild();
}

void UPTWordPackWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTWordPackWidget::OnPacksUpdated()
{
    Rebuild();
}

void UPTWordPackWidget::OnPublished(bool bOk, const FString& Info)
{
    if (!StatusText) return;
    if (bOk)
    {
        StatusText->SetText(PTText::Get(TEXT("WORDPACK_PUB_OK")));
    }
    else
    {
        FFormatOrderedArguments Args;
        Args.Add(FText::FromString(Info));
        StatusText->SetText(PTText::Format(TEXT("WORDPACK_PUB_FAIL"), Args));
    }
}

void UPTWordPackWidget::Rebuild()
{
    if (!PacksBox) return;
    PacksBox->ClearChildren();

    const FString CurTitle = GI() ? GI()->SelectedWordPackTitle : FString();

    int32 Count = 0;
    if (UPTWordPackSubsystem* P = Packs())
    {
        if (RowWidgetClass)
        {
            for (const FPTWordPack& Pack : P->GetPacks())
            {
                UPTWordPackRowWidget* Row = CreateWidget<UPTWordPackRowWidget>(this, RowWidgetClass);
                if (!Row) continue;
                const bool bSelected = !CurTitle.IsEmpty() && Pack.Title == CurTitle;
                Row->Init(Pack, bSelected, this);
                PacksBox->AddChild(Row);
                ++Count;
            }
        }
    }

    if (EmptyText)
    {
        EmptyText->SetText(PTText::Get(TEXT("WORDPACK_EMPTY")));
        // Solo en la pestaña Palabras (en Mapas manda el overlay "Próximamente").
        EmptyText->SetVisibility((ActiveTab == 0 && Count == 0) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (SelectedText)
    {
        if (CurTitle.IsEmpty())
        {
            SelectedText->SetText(PTText::Get(TEXT("WORDPACK_DEFAULT")));
        }
        else
        {
            FFormatOrderedArguments Args;
            Args.Add(FText::FromString(CurTitle));
            SelectedText->SetText(PTText::Format(TEXT("WORDPACK_SELECTED"), Args));
        }
    }
}
