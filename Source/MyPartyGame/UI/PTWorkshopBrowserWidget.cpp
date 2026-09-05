// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTWorkshopBrowserWidget.h"
#include "PTWorkshopItemRowWidget.h"
#include "../PTTextTable.h"
#include "../PTGameInstance.h"
#include "Mods/PTWordPackSubsystem.h"
#include "Components/PanelWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Image.h"
#include "ImageUtils.h"              // ImportFileAsTexture2D (preview de la miniatura)
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/Paths.h"          // GetBaseFilename
#include "HAL/PlatformProcess.h" // LaunchURL

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
    if (PublishButton)      PublishButton->OnClicked.AddDynamic(this, &UPTWorkshopBrowserWidget::OnPublishClicked);
    if (UploadCsvButton)    UploadCsvButton->OnClicked.AddDynamic(this, &UPTWorkshopBrowserWidget::OnUploadCsvClicked);
    if (ThumbnailButton)    ThumbnailButton->OnClicked.AddDynamic(this, &UPTWorkshopBrowserWidget::OnThumbnailClicked);
    if (ApplyPublishButton) ApplyPublishButton->OnClicked.AddDynamic(this, &UPTWorkshopBrowserWidget::OnApplyPublishClicked);
    if (ThumbnailImage)     ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed); // hasta que elijan una
    if (GuideButton)        GuideButton->OnClicked.AddDynamic(this, &UPTWorkshopBrowserWidget::OnGuideClicked);
    if (PopupCloseButton)   PopupCloseButton->OnClicked.AddDynamic(this, &UPTWorkshopBrowserWidget::OnPopupCloseClicked);
    if (PublishPopup)       PublishPopup->SetVisibility(ESlateVisibility::Collapsed);
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
            P->OnWordPackPublished.AddUObject(this, &UPTWorkshopBrowserWidget::OnPublished);
            bBound = true;
        }
    }

    SwitchTab(0);
}

void UPTWorkshopBrowserWidget::NativeDestruct()
{
    if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(UploadAnimTimer);
    if (UPTWordPackSubsystem* P = Packs())
    {
        if (bBound)
        {
            P->OnWorkshopSearchComplete.RemoveAll(this);
            P->OnWordPackPublished.RemoveAll(this);
            bBound = false;
        }
    }
    Super::NativeDestruct();
}

void UPTWorkshopBrowserWidget::ShowPanel()
{
    SetVisibility(ESlateVisibility::Visible);
    if (PublishPopup) PublishPopup->SetVisibility(ESlateVisibility::Collapsed); // arranca cerrado
    PlayPopIn();
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

void UPTWorkshopBrowserWidget::OnPublishClicked()
{
    // Abre el popup (elegir CSV → miniatura → Aplicar). Arranca con el formulario limpio.
    if (PublishPopup)
    {
        ResetPublishForm();
        PublishPopup->SetVisibility(ESlateVisibility::Visible);
        PlayPopInOn(PublishPopup);
        return;
    }
    OnUploadCsvClicked(); // WBP sin popup: al menos deja elegir el CSV
}

void UPTWorkshopBrowserWidget::ResetPublishForm()
{
    PendingCsvPath.Reset();
    PendingImagePath.Reset();
    if (CsvButtonLabel)  CsvButtonLabel->SetText(PTText::Get(TEXT("WORDPACK_CHOOSE_CSV")));
    if (ThumbnailImage)  ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed);
    if (ApplyPublishButton) ApplyPublishButton->SetIsEnabled(true);
    if (StatusText)      StatusText->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTWorkshopBrowserWidget::OnPopupCloseClicked()
{
    if (PublishPopup) PublishPopup->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTWorkshopBrowserWidget::OnGuideClicked()
{
    // Abre la guía (GitHub Pages) en el navegador del sistema.
    if (!GuideUrl.IsEmpty())
        FPlatformProcess::LaunchURL(*GuideUrl, nullptr, nullptr);
}

void UPTWorkshopBrowserWidget::OnUploadCsvClicked()
{
    // Paso 1: elegir el CSV del banco. NO publica: solo guarda la selección y pone el nombre del
    // archivo en el botón. Publicar es después, con "Aplicar".
    if (bUploading) return;
    UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance());
    if (!GI) return;
    FString Path;
    if (!GI->PickCsvFile(Path)) return;
    PendingCsvPath = Path;
    if (CsvButtonLabel) CsvButtonLabel->SetText(FText::FromString(FPaths::GetBaseFilename(Path)));
    if (StatusText) StatusText->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTWorkshopBrowserWidget::OnThumbnailClicked()
{
    // Paso 2: elegir la miniatura (imagen). La mostramos en el popup como preview.
    if (bUploading) return;
    UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance());
    if (!GI) return;
    FString Path;
    if (!GI->PickImageFile(Path)) return;
    PendingImagePath = Path;
    if (ThumbnailImage)
    {
        if (UTexture2D* Tex = FImageUtils::ImportFileAsTexture2D(Path))
        {
            ThumbnailImage->SetBrushFromTexture(Tex, /*bMatchSize=*/false);
            ThumbnailImage->SetVisibility(ESlateVisibility::Visible);
        }
    }
}

void UPTWorkshopBrowserWidget::OnApplyPublishClicked()
{
    // Paso 3: APLICAR → recién acá se publica al Workshop, con feedback de "Subiendo archivo...".
    if (bUploading) return;
    if (PendingCsvPath.IsEmpty())
    {
        if (StatusText) { StatusText->SetText(PTText::Get(TEXT("WORDPACK_NO_CSV"))); StatusText->SetVisibility(ESlateVisibility::Visible); }
        return;
    }
    const FString Title = PublishTitleBox ? PublishTitleBox->GetText().ToString() : FString();
    const FString Desc  = PublishDescBox  ? PublishDescBox->GetText().ToString()  : FString();

    bUploading = true;
    UploadDots = 0;
    if (ApplyPublishButton) ApplyPublishButton->SetIsEnabled(false); // evita doble click
    if (StatusText) StatusText->SetVisibility(ESlateVisibility::Visible);
    TickUploadingText(); // pinta "Subiendo archivo" ya mismo
    if (UWorld* W = GetWorld())
        W->GetTimerManager().SetTimer(UploadAnimTimer, this, &UPTWorkshopBrowserWidget::TickUploadingText, 0.35f, /*loop=*/true);

    if (UPTWordPackSubsystem* WP = Packs())
        WP->PublishWordPack(PendingCsvPath, Title, Desc, PendingImagePath);
}

void UPTWorkshopBrowserWidget::TickUploadingText()
{
    if (!StatusText) return;
    UploadDots = (UploadDots + 1) % 4;
    FString Dots;
    for (int32 i = 0; i < UploadDots; ++i) Dots += TEXT(".");
    StatusText->SetText(FText::FromString(PTText::GetStr(TEXT("WORDPACK_UPLOADING")) + Dots));
}

void UPTWorkshopBrowserWidget::OnPublished(bool bOk, const FString& Info)
{
    // Terminó la subida: cortar el texto animado y reactivar Aplicar.
    bUploading = false;
    if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(UploadAnimTimer);
    if (ApplyPublishButton) ApplyPublishButton->SetIsEnabled(true);

    if (StatusText)
    {
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
        StatusText->SetVisibility(ESlateVisibility::Visible);
    }

    // En éxito, limpiar la selección (CSV/imagen/preview) para dejar el popup listo para otro banco.
    if (bOk)
    {
        PendingCsvPath.Reset();
        PendingImagePath.Reset();
        if (CsvButtonLabel) CsvButtonLabel->SetText(PTText::Get(TEXT("WORDPACK_CHOOSE_CSV")));
        if (ThumbnailImage) ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed);
        if (PublishTitleBox) PublishTitleBox->SetText(FText::GetEmpty());
        if (PublishDescBox)  PublishDescBox->SetText(FText::GetEmpty());
    }
}
