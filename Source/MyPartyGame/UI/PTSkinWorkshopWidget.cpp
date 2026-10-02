// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSkinWorkshopWidget.h"
#include "PTSkinWorkshopRowWidget.h"
#include "../PTTextTable.h"
#include "../PTGameInstance.h"
#include "Mods/PTWordPackSubsystem.h"
#include "Mods/PTMapModSubsystem.h"    // progreso de descarga de un item del Workshop (genérico por id)
#include "Lobby/PTLockerSubsystem.h"
#include "Lobby/PTLockerWidget.h"
#include "Lobby/PTLobbyPlayerController.h"
#include "Lobby/PTLobbyCharacter.h"     // MakeTextureFromPNG (miniatura del slot)
#include "Components/PanelWidget.h"
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

// Tag del Workshop de las skins (lo pone PublishSkin; además lleva "Head" o "Body").
static const TCHAR* PT_TAG_SKIN = TEXT("Skin");

UPTWordPackSubsystem* UPTSkinWorkshopWidget::Packs() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTWordPackSubsystem>() : nullptr;
}
UPTLockerSubsystem* UPTSkinWorkshopWidget::Locker() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLockerSubsystem>() : nullptr;
}
APTLobbyPlayerController* UPTSkinWorkshopWidget::LobbyPC() const
{
    return Cast<APTLobbyPlayerController>(GetOwningPlayer());
}

void UPTSkinWorkshopWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SearchButton)       SearchButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopWidget::OnSearchClicked);
    if (SearchBox)          SearchBox->OnTextCommitted.AddDynamic(this, &UPTSkinWorkshopWidget::OnSearchCommitted);
    if (AllTabButton)       AllTabButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopWidget::OnAllTabClicked);
    if (HeadTabButton)      HeadTabButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopWidget::OnHeadTabClicked);
    if (BodyTabButton)      BodyTabButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopWidget::OnBodyTabClicked);
    if (CloseButton)        CloseButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopWidget::OnCloseClicked);
    if (PublishSkinButton)  PublishSkinButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopWidget::OnPublishSkinClicked);
    if (SkinSelectCombo)    SkinSelectCombo->OnSelectionChanged.AddDynamic(this, &UPTSkinWorkshopWidget::OnSkinSelected);
    if (ThumbnailButton)    ThumbnailButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopWidget::OnThumbnailClicked);
    if (ApplyPublishButton) ApplyPublishButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopWidget::OnApplyPublishClicked);
    if (PublishCloseButton) PublishCloseButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopWidget::OnPublishCloseClicked);

    if (TitleText)       TitleText->SetText(PTText::Get(TEXT("SKINWS_TITLE")));
    if (PublishTitleBox) PublishTitleBox->SetHintText(PTText::Get(TEXT("UI_HINT_TITLE")));
    if (PublishDescBox)  PublishDescBox->SetHintText(PTText::Get(TEXT("UI_HINT_DESC")));
    if (PublishPanel)    PublishPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (StatusText)        StatusText->SetVisibility(ESlateVisibility::Collapsed);
    if (PublishStatusText) PublishStatusText->SetVisibility(ESlateVisibility::Collapsed);

    if (UPTWordPackSubsystem* P = Packs())
    {
        if (!bBound)
        {
            P->OnWorkshopSearchComplete.AddUObject(this, &UPTSkinWorkshopWidget::OnSearchComplete);
            P->OnWordPackPublished.AddUObject(this, &UPTSkinWorkshopWidget::OnPublished);
            P->OnWordPacksUpdated.AddUObject(this, &UPTSkinWorkshopWidget::OnPacksUpdated);
            bBound = true;
        }
    }
}

void UPTSkinWorkshopWidget::NativeDestruct()
{
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearTimer(RefreshTimer);
        W->GetTimerManager().ClearTimer(StatusHideTimer);
        W->GetTimerManager().ClearTimer(UploadAnimTimer);
    }
    if (UPTWordPackSubsystem* P = Packs())
    {
        if (bBound)
        {
            P->OnWorkshopSearchComplete.RemoveAll(this);
            P->OnWordPackPublished.RemoveAll(this);
            P->OnWordPacksUpdated.RemoveAll(this);
            bBound = false;
        }
    }
    Super::NativeDestruct();
}

void UPTSkinWorkshopWidget::ShowPanel(UPTLockerWidget* InOwnerLocker)
{
    OwnerLocker = InOwnerLocker;
    SetVisibility(ESlateVisibility::Visible);
    if (PublishPanel) PublishPanel->SetVisibility(ESlateVisibility::Collapsed);
    PlayPopIn();
    SetKeyboardFocus(); // Esc cierra el popup (no el Locker de fondo)
    SwitchFilter(0);
    RunSearch();

    // Refresco periódico mientras está abierto: % de descarga + importar al Locker las que terminaron.
    if (UWorld* W = GetWorld())
        W->GetTimerManager().SetTimer(RefreshTimer, this, &UPTSkinWorkshopWidget::TickRefresh, 0.5f, /*loop=*/true);
}

void UPTSkinWorkshopWidget::ClosePanel()
{
    if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(RefreshTimer);
    SetVisibility(ESlateVisibility::Collapsed);
    if (UPTLockerWidget* L = OwnerLocker.Get())
    {
        L->RefreshSlots();
        L->SetKeyboardFocus(); // devolver el teclado al Locker
    }
}

void UPTSkinWorkshopWidget::OnCloseClicked() { ClosePanel(); }

FReply UPTSkinWorkshopWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        // Primero cierra el panel de publicar si está abierto; si no, el popup entero.
        if (PublishPanel && PublishPanel->IsVisible()) OnPublishCloseClicked();
        else ClosePanel();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ── Búsqueda / filtro ─────────────────────────────────────────────────────────────────────────

void UPTSkinWorkshopWidget::OnAllTabClicked()  { SwitchFilter(0); }
void UPTSkinWorkshopWidget::OnHeadTabClicked() { SwitchFilter(1); }
void UPTSkinWorkshopWidget::OnBodyTabClicked() { SwitchFilter(2); }

void UPTSkinWorkshopWidget::SwitchFilter(int32 Filter)
{
    ActiveFilter = FMath::Clamp(Filter, 0, 2);
    ApplyFilterVisual();
    RebuildRows(); // filtra los últimos resultados en el cliente (no hace falta otra búsqueda)
}

void UPTSkinWorkshopWidget::ApplyFilterVisual()
{
    UButton* Tabs[3] = { AllTabButton, HeadTabButton, BodyTabButton };
    for (int32 i = 0; i < 3; ++i)
        if (Tabs[i])
        {
            Tabs[i]->SetBackgroundColor(i == ActiveFilter ? TabActiveColor : TabInactiveColor);
            Tabs[i]->SetRenderOpacity(i == ActiveFilter ? 1.0f : 0.45f);
        }
}

void UPTSkinWorkshopWidget::OnSearchClicked() { RunSearch(); }

void UPTSkinWorkshopWidget::OnSearchCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    if (CommitType == ETextCommit::OnEnter) RunSearch();
}

void UPTSkinWorkshopWidget::RunSearch()
{
    UPTWordPackSubsystem* P = Packs();
    if (!P) return;
    bSearching = true;
    if (EmptyText) EmptyText->SetVisibility(ESlateVisibility::Collapsed);
    ShowStatus(PTText::Get(TEXT("WORKSHOP_SEARCHING")), /*bInPublishPanel=*/false, /*AutoHide=*/0.f);
    P->SearchWorkshop(SearchBox ? SearchBox->GetText().ToString() : FString(), PT_TAG_SKIN);
}

void UPTSkinWorkshopWidget::OnSearchComplete(const TArray<FPTWorkshopItem>& Items, bool bOk)
{
    // El subsistema es compartido con el Workshop Browser: solo tomamos la búsqueda que lanzamos acá.
    if (!bSearching) return;
    bSearching = false;
    HideStatus();

    LastItems.Reset();
    if (bOk)
        for (const FPTWorkshopItem& It : Items)
            if (It.Tags.Contains(PT_TAG_SKIN)) LastItems.Add(It);
    RebuildRows();
}

void UPTSkinWorkshopWidget::RebuildRows()
{
    if (!ResultsBox) return;
    ResultsBox->ClearChildren();
    Rows.Reset();

    if (RowWidgetClass)
    {
        for (const FPTWorkshopItem& It : LastItems)
        {
            const bool bBody = It.Tags.Contains(TEXT("Body"));
            if (ActiveFilter == 1 && bBody)  continue; // solo cabezas
            if (ActiveFilter == 2 && !bBody) continue; // solo cuerpos
            UPTSkinWorkshopRowWidget* Row = CreateWidget<UPTSkinWorkshopRowWidget>(this, RowWidgetClass);
            if (!Row) continue;
            Row->Init(It, this);
            ResultsBox->AddChild(Row);
            Rows.Add(Row);
        }
    }

    if (EmptyText)
    {
        EmptyText->SetText(PTText::Get(TEXT("WORKSHOP_EMPTY")));
        EmptyText->SetVisibility((Rows.Num() == 0 && !bSearching) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UPTSkinWorkshopWidget::RefreshRows()
{
    for (UPTSkinWorkshopRowWidget* Row : Rows)
        if (Row) Row->RefreshState();
}

// ── Estado / descargar / equipar / editar ─────────────────────────────────────────────────────

EPTSkinRowState UPTSkinWorkshopWidget::GetSkinState(const FString& ItemId, bool bHead, float& OutProgress01) const
{
    OutProgress01 = 0.f;
    if (const UPTLockerSubsystem* L = Locker())
    {
        const int32 Slot = L->FindSlotByWorkshopId(bHead, ItemId);
        if (Slot >= 0)
            return ((bHead ? L->GetEquippedHead() : L->GetEquippedBody()) == Slot)
                ? EPTSkinRowState::Equipped : EPTSkinRowState::Ready;
    }
    const UPTWordPackSubsystem* P = Packs();
    if (!P) return EPTSkinRowState::NotDownloaded;

    FString Folder;
    if (P->GetInstalledItemFolder(ItemId, Folder)) return EPTSkinRowState::Ready;
    if (PendingDownloads.Contains(ItemId) || P->IsItemSubscribed(ItemId))
    {
        if (const UPTMapModSubsystem* MM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTMapModSubsystem>() : nullptr)
            MM->GetWorkshopDownloadProgress(ItemId, OutProgress01);
        return EPTSkinRowState::Downloading;
    }
    return EPTSkinRowState::NotDownloaded;
}

void UPTSkinWorkshopWidget::DownloadSkin(const FString& ItemId)
{
    UPTWordPackSubsystem* P = Packs();
    if (!P) return;
    PendingDownloads.Add(ItemId);
    P->SubscribeItem(ItemId); // suscribe + DownloadItem; al terminar, TickRefresh la importa al Locker
}

int32 UPTSkinWorkshopWidget::EnsureImported(const FString& ItemId, bool& InOutHead)
{
    UPTLockerSubsystem* L = Locker();
    UPTWordPackSubsystem* P = Packs();
    if (!L || !P) return -1;

    const int32 Existing = L->FindSlotByWorkshopId(InOutHead, ItemId);
    if (Existing >= 0) return Existing;

    FString Folder;
    if (!P->GetInstalledItemFolder(ItemId, Folder)) return -1;

    FString Error;
    const int32 Slot = L->ImportSkinFromFolder(Folder, ItemId, InOutHead, Error);
    if (Slot < 0)
        ShowStatus(PTText::Get(Error == TEXT("full") ? TEXT("SKINWS_LOCKER_FULL") : TEXT("SKINWS_INVALID")), false);
    else
        NotifyLockerChanged();
    return Slot;
}

void UPTSkinWorkshopWidget::EquipSkin(const FString& ItemId, bool bHead)
{
    const int32 Slot = EnsureImported(ItemId, bHead);
    if (Slot < 0) return;
    if (APTLobbyPlayerController* PC = LobbyPC())
    {
        if (bHead) PC->EquipHeadSlot(Slot); else PC->EquipBodySlot(Slot);
    }
    NotifyLockerChanged();
    RefreshRows();
}

void UPTSkinWorkshopWidget::EditSkin(const FString& ItemId, bool bHead)
{
    // Editar = traerla al Locker, equiparla y entrar al editor de ese slot (como "Editar" del Locker).
    const int32 Slot = EnsureImported(ItemId, bHead);
    if (Slot < 0) return;
    APTLobbyPlayerController* PC = LobbyPC();
    if (!PC) return;
    if (bHead) PC->EquipHeadSlot(Slot); else PC->EquipBodySlot(Slot);
    ClosePanel();
    if (bHead) PC->EnterHeadSculptForSlot(Slot);
    else       PC->EnterBodyPaintForSlot(Slot);
}

void UPTSkinWorkshopWidget::ImportFinishedDownloads()
{
    UPTWordPackSubsystem* P = Packs();
    UPTLockerSubsystem* L = Locker();
    if (!P || !L || PendingDownloads.Num() == 0) return;

    // Las que pediste descargar desde acá y ya terminaron → directo a un slot libre del Locker.
    TArray<FString> Done;
    for (const FString& Id : PendingDownloads)
    {
        FString Folder;
        if (!P->GetInstalledItemFolder(Id, Folder)) continue;
        Done.Add(Id);
        bool bHead = true;
        if (!UPTLockerSubsystem::ReadSkinType(Folder, bHead)) { ShowStatus(PTText::Get(TEXT("SKINWS_INVALID")), false); continue; }
        if (L->FindSlotByWorkshopId(bHead, Id) >= 0) continue;
        if (EnsureImported(Id, bHead) >= 0) ShowStatus(PTText::Get(TEXT("SKINWS_ADDED_TO_LOCKER")), false);
    }
    for (const FString& Id : Done) PendingDownloads.Remove(Id);
}

void UPTSkinWorkshopWidget::TickRefresh()
{
    if (!IsVisible()) return;
    ImportFinishedDownloads();
    RefreshRows();
}

void UPTSkinWorkshopWidget::OnPacksUpdated()
{
    // El subsistema re-escanea cuando Steam termina una descarga → importar y refrescar ya (sin esperar el tick).
    if (!IsVisible()) return;
    ImportFinishedDownloads();
    RefreshRows();
}

void UPTSkinWorkshopWidget::NotifyLockerChanged()
{
    if (UPTLockerWidget* L = OwnerLocker.Get()) L->RefreshSlots();
}

// ── Avisos ────────────────────────────────────────────────────────────────────────────────────

void UPTSkinWorkshopWidget::ShowStatus(const FText& Msg, bool bInPublishPanel, float AutoHideSeconds)
{
    UTextBlock* Where = (bInPublishPanel && PublishStatusText) ? PublishStatusText : StatusText;
    if (!Where) Where = PublishStatusText;
    if (!Where) return;
    Where->SetText(Msg);
    Where->SetVisibility(ESlateVisibility::Visible);
    if (UWorld* W = GetWorld())
    {
        if (AutoHideSeconds > 0.f)
            W->GetTimerManager().SetTimer(StatusHideTimer, this, &UPTSkinWorkshopWidget::HideStatus, AutoHideSeconds, false);
        else
            W->GetTimerManager().ClearTimer(StatusHideTimer);
    }
}

void UPTSkinWorkshopWidget::HideStatus()
{
    if (bUploading) return; // "Subiendo..." se queda hasta que termine
    if (StatusText)        StatusText->SetVisibility(ESlateVisibility::Collapsed);
    if (PublishStatusText) PublishStatusText->SetVisibility(ESlateVisibility::Collapsed);
}

// ── Publicar ──────────────────────────────────────────────────────────────────────────────────

void UPTSkinWorkshopWidget::OnPublishSkinClicked()
{
    if (!PublishPanel) return;
    if (!bUploading) ResetPublishForm();
    PublishPanel->SetVisibility(ESlateVisibility::Visible);
    PlayPopInOn(PublishPanel);
}

void UPTSkinWorkshopWidget::OnPublishCloseClicked()
{
    if (PublishPanel) PublishPanel->SetVisibility(ESlateVisibility::Collapsed);
    SetKeyboardFocus();
}

void UPTSkinWorkshopWidget::ResetPublishForm()
{
    PendingImagePath.Reset();
    if (PublishTitleBox) PublishTitleBox->SetText(FText::GetEmpty());
    if (PublishDescBox)  PublishDescBox->SetText(FText::GetEmpty());
    if (ApplyPublishButton) ApplyPublishButton->SetIsEnabled(true);
    if (PublishStatusText)  PublishStatusText->SetVisibility(ESlateVisibility::Collapsed);
    RefreshPublishList();
}

void UPTSkinWorkshopWidget::RefreshPublishList()
{
    PublishOptions.Reset();
    if (SkinSelectCombo) SkinSelectCombo->ClearOptions();
    UPTLockerSubsystem* L = Locker();
    if (!L) return;

    // Tus skins del Locker: cabezas y después cuerpos (mismo nombre que el tile: "Cabeza 3").
    auto AddAll = [&](bool bHead)
    {
        const int32 N = bHead ? L->NumHeadSlots() : L->NumBodySlots();
        for (int32 i = 0; i < N; ++i)
        {
            if (!L->IsSlotPublishable(bHead, i)) continue;
            FPTSkinPublishOption Opt; Opt.bHead = bHead; Opt.Slot = i;
            PublishOptions.Add(Opt);
            if (SkinSelectCombo)
                SkinSelectCombo->AddOption(FString::Printf(TEXT("%s %d"),
                    *PTText::GetStr(bHead ? TEXT("LOCKER_HEAD") : TEXT("LOCKER_BODY")), i + 1));
        }
    };
    AddAll(true);
    AddAll(false);

    if (SkinSelectCombo && PublishOptions.Num() > 0) SkinSelectCombo->SetSelectedIndex(0);
    ShowSlotPreview();
}

void UPTSkinWorkshopWidget::OnSkinSelected(FString SelectedItem, ESelectInfo::Type Type)
{
    ShowSlotPreview();
}

void UPTSkinWorkshopWidget::ShowSlotPreview()
{
    if (!SkinPreviewImage) return;
    // Imagen elegida a mano manda; si no, la miniatura del slot (la foto del Locker).
    UTexture2D* Tex = nullptr;
    if (!PendingImagePath.IsEmpty())
        Tex = FImageUtils::ImportFileAsTexture2D(PendingImagePath);
    else if (UPTLockerSubsystem* L = Locker())
    {
        const int32 Idx = SkinSelectCombo ? SkinSelectCombo->GetSelectedIndex() : 0;
        if (PublishOptions.IsValidIndex(Idx))
        {
            const FPTSkinPublishOption& O = PublishOptions[Idx];
            Tex = APTLobbyCharacter::MakeTextureFromPNG(this, O.bHead ? L->GetHeadThumb(O.Slot) : L->GetBodyThumb(O.Slot));
        }
    }
    if (Tex)
    {
        SkinPreviewImage->SetBrushFromTexture(Tex, /*bMatchSize=*/false);
        SkinPreviewImage->SetVisibility(ESlateVisibility::Visible);
    }
    else SkinPreviewImage->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTSkinWorkshopWidget::OnThumbnailClicked()
{
    if (bUploading) return;
    UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance());
    FString Path;
    if (!GI || !GI->PickImageFile(Path)) return;
    PendingImagePath = Path;
    ShowSlotPreview();
}

void UPTSkinWorkshopWidget::OnApplyPublishClicked()
{
    if (bUploading) return;
    const FString Title = PublishTitleBox ? PublishTitleBox->GetText().ToString().TrimStartAndEnd() : FString();
    const FString Desc  = PublishDescBox  ? PublishDescBox->GetText().ToString().TrimStartAndEnd()  : FString();
    const int32   Idx   = SkinSelectCombo ? SkinSelectCombo->GetSelectedIndex() : 0;

    // Obligatorios: la skin, título y descripción (la miniatura sale del slot si no elegís una).
    TArray<FString> Missing;
    if (!PublishOptions.IsValidIndex(Idx)) Missing.Add(PTText::GetStr(TEXT("SKINWS_F_SKIN")));
    if (Title.IsEmpty())                   Missing.Add(PTText::GetStr(TEXT("WORDPACK_F_TITLE")));
    if (Desc.IsEmpty())                    Missing.Add(PTText::GetStr(TEXT("WORDPACK_F_DESC")));
    if (Missing.Num() > 0)
    {
        FFormatOrderedArguments Args; Args.Add(FText::FromString(FString::Join(Missing, TEXT(", "))));
        ShowStatus(PTText::Format(TEXT("WORDPACK_MISSING"), Args), /*bInPublishPanel=*/true);
        return;
    }

    UPTWordPackSubsystem* P = Packs();
    if (!P) return;
    bUploading = true;
    UploadDots = 0;
    if (ApplyPublishButton) ApplyPublishButton->SetIsEnabled(false);
    TickUploadingText();
    if (UWorld* W = GetWorld())
        W->GetTimerManager().SetTimer(UploadAnimTimer, this, &UPTSkinWorkshopWidget::TickUploadingText, 0.35f, /*loop=*/true);

    const FPTSkinPublishOption Opt = PublishOptions[Idx];
    P->PublishSkin(Opt.bHead, Opt.Slot, Title, Desc, PendingImagePath);
}

void UPTSkinWorkshopWidget::TickUploadingText()
{
    UploadDots = (UploadDots + 1) % 4;
    FString Dots;
    for (int32 i = 0; i < UploadDots; ++i) Dots += TEXT(".");
    ShowStatus(FText::FromString(PTText::GetStr(TEXT("WORDPACK_UPLOADING")) + Dots), /*bInPublishPanel=*/true, 0.f);
}

void UPTSkinWorkshopWidget::OnPublished(bool bOk, const FString& Info)
{
    // El delegate es compartido (bancos/mapas también publican por acá): solo si la subida es nuestra.
    if (!bUploading) return;
    bUploading = false;
    if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(UploadAnimTimer);
    if (ApplyPublishButton) ApplyPublishButton->SetIsEnabled(true);

    if (bOk)
    {
        ShowStatus(PTText::Get(TEXT("SKINWS_PUB_OK")), /*bInPublishPanel=*/true, 4.f);
        PendingImagePath.Reset();
        if (PublishTitleBox) PublishTitleBox->SetText(FText::GetEmpty());
        if (PublishDescBox)  PublishDescBox->SetText(FText::GetEmpty());
        ShowSlotPreview();
        RunSearch(); // best-effort: Steam puede tardar en indexarla para las búsquedas
    }
    else
    {
        FFormatOrderedArguments Args; Args.Add(FText::FromString(Info));
        ShowStatus(PTText::Format(TEXT("WORDPACK_PUB_FAIL"), Args), /*bInPublishPanel=*/true, 6.f);
    }
}
