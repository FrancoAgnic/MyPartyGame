// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTWordPackWidget.h"
#include "PTWordPackRowWidget.h"
#include "PTWorkshopBrowserWidget.h"
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
    if (DefaultButton) DefaultButton->OnClicked.AddDynamic(this, &UPTWordPackWidget::OnDefaultClicked);
    if (BackButton)    BackButton->OnClicked.AddDynamic(this, &UPTWordPackWidget::OnBackClicked);
    if (WordsTabButton) WordsTabButton->OnClicked.AddDynamic(this, &UPTWordPackWidget::OnWordsTabClicked);
    if (MapsTabButton)  MapsTabButton->OnClicked.AddDynamic(this, &UPTWordPackWidget::OnMapsTabClicked);
    if (WorkshopButton) WorkshopButton->OnClicked.AddDynamic(this, &UPTWordPackWidget::OnWorkshopClicked);

    if (TitleText)      TitleText->SetText(PTText::Get(TEXT("WORDPACK_TITLE")));
    if (MapsLockedText) MapsLockedText->SetText(PTText::Get(TEXT("MAPS_SOON")));
    if (MapsBox)        MapsBox->ClearChildren(); // mapas bloqueados: lista vacía por ahora

    if (UPTWordPackSubsystem* P = Packs())
    {
        if (!bBound)
        {
            P->OnWordPacksUpdated.AddUObject(this, &UPTWordPackWidget::OnPacksUpdated);
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
    // Controles de bancos (actualizar/default) solo en la pestaña Palabras.
    if (RefreshButton)   RefreshButton->SetVisibility(bMaps ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
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

void UPTWordPackWidget::OnDefaultClicked()
{
    if (GI()) GI()->SelectDefaultWordBank();
    Rebuild();
}

void UPTWordPackWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTWordPackWidget::OnWorkshopClicked()
{
    if (!WorkshopBrowserClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WordPack] WorkshopButton sin WorkshopBrowserClass asignado (Details del WBP_WordPack)."));
        return;
    }
    // Crear la ventana una sola vez y reusarla; mostrarla por encima de la Biblioteca.
    if (!WorkshopBrowser)
    {
        WorkshopBrowser = CreateWidget<UPTWorkshopBrowserWidget>(this, WorkshopBrowserClass);
        if (!WorkshopBrowser) return;
        WorkshopBrowser->AddToViewport(60); // por encima del lobby y de la Biblioteca
    }
    WorkshopBrowser->ShowPanel();
}

void UPTWordPackWidget::OnPacksUpdated()
{
    Rebuild();
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
