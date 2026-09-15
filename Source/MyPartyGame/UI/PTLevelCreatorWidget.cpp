// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLevelCreatorWidget.h"
#include "../PTGameInstance.h"
#include "../PTTextTable.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Misc/Paths.h"

UPTGameInstance* UPTLevelCreatorWidget::GI() const
{
    return Cast<UPTGameInstance>(GetGameInstance());
}

void UPTLevelCreatorWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (CreateButton) CreateButton->OnClicked.AddDynamic(this, &UPTLevelCreatorWidget::OnCreateClicked);
    if (EditButton)   EditButton->OnClicked.AddDynamic(this, &UPTLevelCreatorWidget::OnEditClicked);
    if (CloseButton)  CloseButton->OnClicked.AddDynamic(this, &UPTLevelCreatorWidget::OnCloseClicked);
    if (MapSelectCombo) MapSelectCombo->OnSelectionChanged.AddDynamic(this, &UPTLevelCreatorWidget::OnMapSelected);
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTLevelCreatorWidget::ShowPanel()
{
    SetVisibility(ESlateVisibility::Visible);
    if (NewTitleBox) NewTitleBox->SetText(FText::GetEmpty());
    if (NewDescBox)  NewDescBox->SetText(FText::GetEmpty());
    if (StatusText)  StatusText->SetVisibility(ESlateVisibility::Collapsed);
    RefreshList();
    PlayPopIn();
}

void UPTLevelCreatorWidget::RefreshList()
{
    if (!MapSelectCombo) return;
    UPTGameInstance* G = GI();
    if (!G) return;
    MapSelectCombo->ClearOptions();
    Slugs.Reset();
    TArray<FString> S, T;
    G->ListAuthoredMaps(S, T);
    for (int32 i = 0; i < S.Num(); ++i)
    {
        Slugs.Add(S[i]);
        MapSelectCombo->AddOption(T.IsValidIndex(i) ? T[i] : S[i]);
    }
    if (Slugs.Num() > 0)
    {
        MapSelectCombo->SetSelectedIndex(0);
        OnMapSelected(FString(), ESelectInfo::Direct); // autocompletar la miniatura del 1ro
    }
    else if (ThumbnailImage) ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTLevelCreatorWidget::OnMapSelected(FString SelectedItem, ESelectInfo::Type Type)
{
    if (!ThumbnailImage) return;
    UPTGameInstance* G = GI();
    const int32 Idx = MapSelectCombo ? MapSelectCombo->GetSelectedIndex() : INDEX_NONE;
    if (!G || !Slugs.IsValidIndex(Idx)) { ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed); return; }
    // Miniatura guardada del mapa (preview.png, la que aplicaste en el form de Save).
    const FString Prev = G->AuthoredMapPreviewPath(Slugs[Idx]);
    if (FPaths::FileExists(Prev))
    {
        if (UTexture2D* Tex = FImageUtils::ImportFileAsTexture2D(Prev))
        { ThumbnailImage->SetBrushFromTexture(Tex, false); ThumbnailImage->SetVisibility(ESlateVisibility::Visible); return; }
    }
    ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed); // sin miniatura guardada
}

void UPTLevelCreatorWidget::OnCreateClicked()
{
    UPTGameInstance* G = GI();
    if (!G) return;
    const FString Title = NewTitleBox ? NewTitleBox->GetText().ToString().TrimStartAndEnd() : FString();
    const FString Desc  = NewDescBox  ? NewDescBox->GetText().ToString().TrimStartAndEnd()  : FString();
    if (Title.IsEmpty()) // el título es obligatorio al crear
    {
        if (StatusText)
        {
            FFormatOrderedArguments A; A.Add(PTText::Get(TEXT("WORDPACK_F_TITLE")));
            StatusText->SetText(PTText::Format(TEXT("WORDPACK_MISSING"), A));
            StatusText->SetVisibility(ESlateVisibility::Visible);
        }
        return;
    }
    G->CreateNewLevel(Title, Desc); // crea el mapa (mod.json) y entra a autoría (travel)
}

void UPTLevelCreatorWidget::OnEditClicked()
{
    UPTGameInstance* G = GI();
    const int32 Idx = MapSelectCombo ? MapSelectCombo->GetSelectedIndex() : INDEX_NONE;
    if (G && Slugs.IsValidIndex(Idx))
        G->EditLevel(Slugs[Idx]); // entra a autoría cargando ese mapa
}

void UPTLevelCreatorWidget::OnCloseClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}
