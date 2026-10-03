// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSkinWorkshopWidget.h"
#include "PTSkinRowWidget.h"
#include "../PTTextTable.h"
#include "../Lobby/PTLobbyPlayerController.h"
#include "../Lobby/PTLockerSubsystem.h"
#include "Mods/PTWordPackSubsystem.h"
#include "Components/PanelWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Image.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

static const TCHAR* PT_TAG_SKIN = TEXT("Skin");

UPTWordPackSubsystem* UPTSkinWorkshopWidget::Packs() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTWordPackSubsystem>() : nullptr;
}
UPTLockerSubsystem* UPTSkinWorkshopWidget::Locker() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLockerSubsystem>() : nullptr;
}

void UPTSkinWorkshopWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Bindeos UNA SOLA VEZ: este widget se CACHEA y se re-agrega al viewport (NativeConstruct corre cada
    // vez), así que sin este guard los botones quedaban bindeados varias veces → OnPublishClicked se
    // disparaba 2x y el popup abría/cerraba ("a veces no abre"). bBound NO se resetea (los bindeos a
    // delegados UObject son débiles y mueren con el widget).
    if (!bBound)
    {
        if (SearchButton)       SearchButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopWidget::OnSearchClicked);
        if (BackButton)         BackButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopWidget::OnBackClicked);
        if (PublishButton)      PublishButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopWidget::OnPublishClicked);
        if (ApplyPublishButton) ApplyPublishButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopWidget::OnApplyPublishClicked);
        if (PopupCloseButton)   PopupCloseButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopWidget::OnPopupCloseClicked);
        if (SearchBox)          SearchBox->OnTextCommitted.AddDynamic(this, &UPTSkinWorkshopWidget::OnSearchCommitted);
        if (UPTWordPackSubsystem* P = Packs())
        {
            P->OnWorkshopSearchComplete.AddUObject(this, &UPTSkinWorkshopWidget::OnSearchComplete);
            P->OnWordPackPublished.AddUObject(this, &UPTSkinWorkshopWidget::OnPublished);
            // OJO: NO re-buscar en OnWordPacksUpdated. Ese evento dispara seguido (suscribir/descargar/
            // detalles) y reconstruía TODA la lista mientras interactuabas → las filas se recreaban y el
            // botón bajo el mouse desaparecía (no hacía hover/click). El estado del botón se actualiza
            // local (Añadir→Equipar) y Equipar chequea en vivo si ya está descargada.
        }
        bBound = true;
    }

    // Estado por apertura (sí cada vez): popup de publicar cerrado y sin avisos.
    if (PublishPopup)      PublishPopup->SetVisibility(ESlateVisibility::Collapsed);
    if (PublishStatusText) PublishStatusText->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTSkinWorkshopWidget::ShowPanel()
{
    SetVisibility(ESlateVisibility::Visible);
    if (PublishPopup) PublishPopup->SetVisibility(ESlateVisibility::Collapsed);
    // El Locker tenía el foco de teclado; dárselo al popup para que reciba la UI (y el mouse ya va al
    // widget de arriba por z-order). Aseguramos también cursor + input GameAndUI.
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->SetInputMode(FInputModeGameAndUI());
        PC->SetShowMouseCursor(true);
    }
    SetFocus();
    RunSearch();
}

void UPTSkinWorkshopWidget::OnSearchClicked() { RunSearch(); }
void UPTSkinWorkshopWidget::OnSearchCommitted(const FText&, ETextCommit::Type CommitType)
{
    if (CommitType == ETextCommit::OnEnter) RunSearch();
}

void UPTSkinWorkshopWidget::RunSearch()
{
    UPTWordPackSubsystem* P = Packs();
    if (!P) return;
    const FString Text = SearchBox ? SearchBox->GetText().ToString() : FString();
    if (StatusText)
    {
        StatusText->SetText(PTText::Get(TEXT("WORKSHOP_SEARCHING")));
        StatusText->SetVisibility(ESlateVisibility::Visible);
    }
    if (EmptyText) EmptyText->SetVisibility(ESlateVisibility::Collapsed);
    P->SearchWorkshop(Text, PT_TAG_SKIN);
}

void UPTSkinWorkshopWidget::OnSearchComplete(const TArray<FPTWorkshopItem>& Items, bool bOk)
{
    if (StatusText) StatusText->SetVisibility(ESlateVisibility::Collapsed);
    if (!ResultsBox) return;
    ResultsBox->ClearChildren();

    int32 Count = 0;
    if (bOk && RowWidgetClass)
    {
        for (const FPTWorkshopItem& It : Items)
        {
            UPTSkinRowWidget* Row = CreateWidget<UPTSkinRowWidget>(this, RowWidgetClass);
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

void UPTSkinWorkshopWidget::AddItem(const FString& ItemId)
{
    if (UPTWordPackSubsystem* P = Packs()) P->SubscribeItem(ItemId);
}

void UPTSkinWorkshopWidget::EquipItem(const FString& ItemId)
{
    UPTWordPackSubsystem* P = Packs();
    UPTLockerSubsystem*   L = Locker();
    if (!P || !L) return;

    TArray<uint8> Bytes;
    if (!P->GetInstalledSkinBundle(ItemId, Bytes))
    {
        SetStatus(PTText::Get(TEXT("SKIN_WS_DOWNLOADING")));
        return;
    }
    int32 BodyIdx = -1;
    const int32 HeadIdx = L->ImportSkinBundle(Bytes, BodyIdx);
    if (HeadIdx < 0)
    {
        SetStatus(PTText::Get(TEXT("SKIN_WS_IMPORT_FAIL")));
        return;
    }
    if (APTLobbyPlayerController* PC = Cast<APTLobbyPlayerController>(GetOwningPlayer()))
    {
        PC->EquipHeadSlot(HeadIdx);
        if (BodyIdx >= 0) PC->EquipBodySlot(BodyIdx);
    }
    SetStatus(PTText::Get(TEXT("SKIN_WS_EQUIPPED")));
    RefreshPublishPreview();
}

void UPTSkinWorkshopWidget::OnPublishClicked()
{
    // Siempre ABRIR el popup (antes togglaba → a veces lo cerraba). Se cierra con PopupCloseButton.
    if (PublishPopup)      PublishPopup->SetVisibility(ESlateVisibility::Visible);
    if (PublishStatusText) PublishStatusText->SetVisibility(ESlateVisibility::Collapsed);
    RefreshPublishPreview();
}

void UPTSkinWorkshopWidget::OnPopupCloseClicked()
{
    if (PublishPopup) PublishPopup->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTSkinWorkshopWidget::RefreshPublishPreview()
{
    if (!SkinPreviewImage) return;
    UPTLockerSubsystem* L = Locker();
    if (!L) return;
    const TArray<uint8>& Thumb = L->GetHeadThumb(L->GetEquippedHead());
    if (Thumb.Num() > 0)
    {
        if (UTexture2D* Tex = FImageUtils::ImportBufferAsTexture2D(Thumb))
        {
            SkinPreviewImage->SetBrushFromTexture(Tex, /*bMatchSize=*/false);
            SkinPreviewImage->SetVisibility(ESlateVisibility::Visible);
            return;
        }
    }
    SkinPreviewImage->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTSkinWorkshopWidget::OnApplyPublishClicked()
{
    UPTWordPackSubsystem* P = Packs();
    UPTLockerSubsystem*   L = Locker();
    if (!P || !L) return;

    const int32 HeadIdx = L->GetEquippedHead();
    const int32 BodyIdx = L->GetEquippedBody();
    TArray<uint8> Bytes;
    if (!L->ExportSkinBundle(HeadIdx, BodyIdx, Bytes))
    {
        if (PublishStatusText)
        {
            PublishStatusText->SetText(PTText::Get(TEXT("SKIN_WS_NEED_HEAD")));
            PublishStatusText->SetVisibility(ESlateVisibility::Visible);
        }
        return;
    }

    const FString Title = PublishTitleBox ? PublishTitleBox->GetText().ToString() : FString();
    const FString Desc  = PublishDescBox  ? PublishDescBox->GetText().ToString()  : FString();

    // Miniatura del Workshop = la miniatura de la cabeza equipada (a un .png temporal). Si no hay, el
    // subsistema cae a la imagen por defecto.
    FString Preview;
    const TArray<uint8>& Thumb = L->GetHeadThumb(HeadIdx);
    if (Thumb.Num() > 0)
    {
        const FString Tmp = FPaths::Combine(FString(FPlatformProcess::UserTempDir()), TEXT("SculpturilloSkinPreview.png"));
        if (FFileHelper::SaveArrayToFile(Thumb, *Tmp)) Preview = Tmp;
    }

    if (PublishStatusText)
    {
        PublishStatusText->SetText(PTText::Get(TEXT("SKIN_WS_UPLOADING")));
        PublishStatusText->SetVisibility(ESlateVisibility::Visible);
    }
    P->PublishSkin(Bytes, Title, Desc, Preview);
}

void UPTSkinWorkshopWidget::OnPublished(bool bOk, const FString& Info)
{
    if (PublishStatusText)
    {
        PublishStatusText->SetText(bOk
            ? PTText::Get(TEXT("SKIN_WS_PUBLISHED"))
            : PTText::Format(TEXT("SKIN_WS_PUBLISH_FAIL"), { FText::FromString(Info) }));
        PublishStatusText->SetVisibility(ESlateVisibility::Visible);
    }
    if (bOk) RunSearch();
}

void UPTSkinWorkshopWidget::OnBackClicked()
{
    RemoveFromParent();
}

void UPTSkinWorkshopWidget::SetStatus(const FText& Msg)
{
    if (StatusText)
    {
        StatusText->SetText(Msg);
        StatusText->SetVisibility(ESlateVisibility::Visible);
    }
}
