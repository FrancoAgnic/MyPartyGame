// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTPublishWidget.h"
#include "../PTTextTable.h"
#include "../PTGameInstance.h"
#include "../PTGameUserSettings.h"
#include "Mods/PTWordPackSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

UPTWordPackSubsystem* UPTPublishWidget::Packs() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTWordPackSubsystem>() : nullptr;
}
UPTGameInstance* UPTPublishWidget::GI() const
{
    return Cast<UPTGameInstance>(GetGameInstance());
}

void UPTPublishWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (BankTabButton)   BankTabButton->OnClicked.AddDynamic(this, &UPTPublishWidget::OnBankTabClicked);
    if (MapTabButton)    MapTabButton->OnClicked.AddDynamic(this, &UPTPublishWidget::OnMapTabClicked);
    if (UploadCsvButton) UploadCsvButton->OnClicked.AddDynamic(this, &UPTPublishWidget::OnUploadCsvClicked);
    if (ThumbnailButton) ThumbnailButton->OnClicked.AddDynamic(this, &UPTPublishWidget::OnThumbnailClicked);
    if (ApplyButton)     ApplyButton->OnClicked.AddDynamic(this, &UPTPublishWidget::OnApplyClicked);
    if (CloseButton)     CloseButton->OnClicked.AddDynamic(this, &UPTPublishWidget::OnCloseClicked);
    if (GuideButton)     GuideButton->OnClicked.AddDynamic(this, &UPTPublishWidget::OnGuideClicked);
    if (MapSelectCombo)  MapSelectCombo->OnSelectionChanged.AddDynamic(this, &UPTPublishWidget::OnMapSelected);
    if (EditMapButton)   EditMapButton->OnClicked.AddDynamic(this, &UPTPublishWidget::OnEditMapClicked);

    if (UPTWordPackSubsystem* P = Packs())
    {
        if (!bBound) { P->OnWordPackPublished.AddUObject(this, &UPTPublishWidget::OnPublished); bBound = true; }
    }

    SwitchTab(0);
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTPublishWidget::NativeDestruct()
{
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearTimer(UploadAnimTimer);
        W->GetTimerManager().ClearTimer(MsgHideTimer);
    }
    if (UPTWordPackSubsystem* P = Packs())
        if (bBound) { P->OnWordPackPublished.RemoveAll(this); bBound = false; }
    Super::NativeDestruct();
}

void UPTPublishWidget::ShowPanel()
{
    SetVisibility(ESlateVisibility::Visible);
    ResetForm();
    SwitchTab(0);
    PlayPopIn();
}

void UPTPublishWidget::OnBankTabClicked() { SwitchTab(0); }
void UPTPublishWidget::OnMapTabClicked()  { SwitchTab(1); }

void UPTPublishWidget::SwitchTab(int32 Tab)
{
    ActiveTab = Tab;
    const bool bMaps = (Tab == 1);
    if (BankPanel) BankPanel->SetVisibility(bMaps ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (MapPanel)  MapPanel->SetVisibility(bMaps ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    // Controles sueltos (por si el WBP no usa contenedores BankPanel/MapPanel).
    if (UploadCsvButton) UploadCsvButton->SetVisibility(bMaps ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (MapSelectCombo)  MapSelectCombo->SetVisibility(bMaps ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (EditMapButton)   EditMapButton->SetVisibility(bMaps ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ResetForm();
    if (bMaps) RefreshMapList();
    ApplyTabVisual();
}

void UPTPublishWidget::ApplyTabVisual()
{
    const bool bBank = (ActiveTab == 0);
    if (BankTabButton) { BankTabButton->SetBackgroundColor(bBank ? TabActiveColor : TabInactiveColor); BankTabButton->SetRenderOpacity(bBank ? 1.f : 0.45f); }
    if (MapTabButton)  { MapTabButton->SetBackgroundColor(!bBank ? TabActiveColor : TabInactiveColor); MapTabButton->SetRenderOpacity(!bBank ? 1.f : 0.45f); }
}

void UPTPublishWidget::ResetForm()
{
    PendingCsvPath.Reset();
    PendingImagePath.Reset();
    if (CsvButtonLabel)   CsvButtonLabel->SetText(PTText::Get(TEXT("WORDPACK_CHOOSE_CSV")));
    if (ThumbnailImage)   ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed);
    if (ApplyButton)      ApplyButton->SetIsEnabled(true);
    if (StatusText)       StatusText->SetVisibility(ESlateVisibility::Collapsed);
    if (PublishStatusText)PublishStatusText->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTPublishWidget::RefreshMapList()
{
    if (!MapSelectCombo) return;
    UPTGameInstance* G = GI();
    if (!G) return;
    MapSelectCombo->ClearOptions();
    AuthoredMapSlugs.Reset();
    TArray<FString> Slugs, Titles;
    G->ListAuthoredMaps(Slugs, Titles);
    for (int32 i = 0; i < Slugs.Num(); ++i)
    {
        AuthoredMapSlugs.Add(Slugs[i]);
        MapSelectCombo->AddOption(Titles.IsValidIndex(i) ? Titles[i] : Slugs[i]);
    }
    if (AuthoredMapSlugs.Num() > 0)
    {
        MapSelectCombo->SetSelectedIndex(0);
        PendingCsvPath = G->AuthoredMapDir(AuthoredMapSlugs[0]); // carpeta del mapa a publicar
    }
}

void UPTPublishWidget::OnMapSelected(FString SelectedItem, ESelectInfo::Type Type)
{
    UPTGameInstance* G = GI();
    const int32 Idx = MapSelectCombo ? MapSelectCombo->GetSelectedIndex() : INDEX_NONE;
    if (G && AuthoredMapSlugs.IsValidIndex(Idx))
        PendingCsvPath = G->AuthoredMapDir(AuthoredMapSlugs[Idx]);
}

void UPTPublishWidget::OnEditMapClicked()
{
    UPTGameInstance* G = GI();
    const int32 Idx = MapSelectCombo ? MapSelectCombo->GetSelectedIndex() : INDEX_NONE;
    if (G && AuthoredMapSlugs.IsValidIndex(Idx))
        G->EditLevel(AuthoredMapSlugs[Idx]); // travel a autoría con ese mapa
}

void UPTPublishWidget::OnUploadCsvClicked()
{
    if (bUploading || ActiveTab != 0) return;
    UPTGameInstance* G = GI();
    if (!G) return;
    FString Path;
    if (!G->PickCsvFile(Path)) return;
    PendingCsvPath = Path;
    if (CsvButtonLabel) CsvButtonLabel->SetText(FText::FromString(FPaths::GetBaseFilename(Path)));
    if (PublishStatusText) PublishStatusText->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTPublishWidget::OnThumbnailClicked()
{
    if (bUploading) return;
    UPTGameInstance* G = GI();
    if (!G) return;
    FString Path;
    if (!G->PickImageFile(Path)) return;
    PendingImagePath = Path;
    if (ThumbnailImage)
    {
        if (UTexture2D* Tex = FImageUtils::ImportFileAsTexture2D(Path))
        {
            ThumbnailImage->SetBrushFromTexture(Tex, /*bMatchSize=*/false);
            ThumbnailImage->SetVisibility(ESlateVisibility::Visible);
        }
    }
    if (PublishStatusText) PublishStatusText->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTPublishWidget::OnApplyClicked()
{
    if (bUploading) return;
    const FString Title = TitleBox ? TitleBox->GetText().ToString().TrimStartAndEnd() : FString();
    const FString Desc  = DescBox  ? DescBox->GetText().ToString().TrimStartAndEnd()  : FString();

    // Obligatorios: título, miniatura, descripción y el contenido (CSV o mapa).
    TArray<FString> Missing;
    if (Title.IsEmpty())            Missing.Add(PTText::GetStr(TEXT("WORDPACK_F_TITLE")));
    if (PendingImagePath.IsEmpty()) Missing.Add(PTText::GetStr(TEXT("WORDPACK_F_THUMB")));
    if (Desc.IsEmpty())             Missing.Add(PTText::GetStr(TEXT("WORDPACK_F_DESC")));
    if (PendingCsvPath.IsEmpty())   Missing.Add(PTText::GetStr(ActiveTab == 1 ? TEXT("WORDPACK_F_MAP") : TEXT("WORDPACK_F_CSV")));
    if (Missing.Num() > 0)
    {
        FFormatOrderedArguments Args; Args.Add(FText::FromString(FString::Join(Missing, TEXT(", "))));
        ShowMsg(PTText::Format(TEXT("WORDPACK_MISSING"), Args));
        return;
    }

    if (PublishStatusText) PublishStatusText->SetVisibility(ESlateVisibility::Collapsed);
    bUploading = true;
    UploadDots = 0;
    if (ApplyButton) ApplyButton->SetIsEnabled(false);
    if (StatusText)  StatusText->SetVisibility(ESlateVisibility::Visible);
    TickUploadingText();
    if (UWorld* W = GetWorld())
        W->GetTimerManager().SetTimer(UploadAnimTimer, this, &UPTPublishWidget::TickUploadingText, 0.35f, /*loop=*/true);

    if (UPTWordPackSubsystem* WP = Packs())
    {
        if (ActiveTab == 1) WP->PublishMap(PendingCsvPath, Title, Desc, PendingImagePath);   // carpeta del mapa
        else                WP->PublishWordPack(PendingCsvPath, Title, Desc, PendingImagePath); // CSV
    }
}

void UPTPublishWidget::OnCloseClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTPublishWidget::OnGuideClicked()
{
    if (GuideUrl.IsEmpty()) return;
    FString Url = GuideUrl, Lang = TEXT("en");
    if (const UPTGameUserSettings* S = UPTGameUserSettings::Get())
    {
        const FString Code = S->GetLanguageCode().Left(2).ToLower();
        if (!Code.IsEmpty()) Lang = Code;
    }
    Url += (Url.Contains(TEXT("?")) ? TEXT("&") : TEXT("?"));
    Url += TEXT("lang=") + Lang;
    FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}

void UPTPublishWidget::ShowMsg(const FText& Msg)
{
    UTextBlock* Where = PublishStatusText ? PublishStatusText : StatusText;
    if (!Where) return;
    Where->SetText(Msg);
    Where->SetVisibility(ESlateVisibility::Visible);
    if (UWorld* W = GetWorld())
        W->GetTimerManager().SetTimer(MsgHideTimer, this, &UPTPublishWidget::HideMsg, 3.0f, false);
}
void UPTPublishWidget::HideMsg()
{
    if (PublishStatusText) PublishStatusText->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTPublishWidget::TickUploadingText()
{
    if (!StatusText) return;
    UploadDots = (UploadDots + 1) % 4;
    FString Dots; for (int32 i = 0; i < UploadDots; ++i) Dots += TEXT(".");
    StatusText->SetText(FText::FromString(PTText::GetStr(TEXT("WORDPACK_UPLOADING")) + Dots));
}

void UPTPublishWidget::OnPublished(bool bOk, const FString& Info)
{
    bUploading = false;
    if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(UploadAnimTimer);
    if (ApplyButton) ApplyButton->SetIsEnabled(true);
    if (StatusText)
    {
        if (bOk) StatusText->SetText(PTText::Get(TEXT("WORDPACK_PUB_OK")));
        else { FFormatOrderedArguments A; A.Add(FText::FromString(Info)); StatusText->SetText(PTText::Format(TEXT("WORDPACK_PUB_FAIL"), A)); }
        StatusText->SetVisibility(ESlateVisibility::Visible);
    }
    if (bOk)
    {
        // Auto-suscribir el item recién publicado (Info trae el id) → queda usable sin reiniciar.
        const FString ItemId = Info.TrimStartAndEnd();
        if (!ItemId.IsEmpty() && ItemId.IsNumeric())
            if (UPTWordPackSubsystem* P = Packs()) P->SubscribeItem(ItemId);
        // Limpiar el formulario para publicar otro.
        PendingCsvPath.Reset(); PendingImagePath.Reset();
        if (CsvButtonLabel) CsvButtonLabel->SetText(PTText::Get(TEXT("WORDPACK_CHOOSE_CSV")));
        if (ThumbnailImage) ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed);
        if (TitleBox) TitleBox->SetText(FText::GetEmpty());
        if (DescBox)  DescBox->SetText(FText::GetEmpty());
        if (ActiveTab == 1) RefreshMapList();
    }
}
