// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSkinWorkshopRowWidget.h"
#include "PTSkinWorkshopWidget.h"
#include "../PTTextTable.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

bool UPTSkinWorkshopRowWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (ActionButton) ActionButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopRowWidget::OnActionClicked);
    if (EditButton)   EditButton->OnClicked.AddDynamic(this, &UPTSkinWorkshopRowWidget::OnEditClicked);
    return true;
}

void UPTSkinWorkshopRowWidget::Init(const FPTWorkshopItem& InItem, UPTSkinWorkshopWidget* InOwner)
{
    ItemId = InItem.Id;
    Owner  = InOwner;
    // Tipo por los tags de Steam ("Skin,Body" → cuerpo; si no, cabeza).
    bHead  = !InItem.Tags.Contains(TEXT("Body"));

    if (TitleText) TitleText->SetText(FText::FromString(InItem.Title));
    if (DescText)
    {
        FString D = InItem.Description.TrimStartAndEnd();
        if (D.Len() > 200) D = D.Left(197) + TEXT("...");
        DescText->SetText(FText::FromString(D));
    }
    if (TypeTagText) TypeTagText->SetText(PTText::Get(bHead ? TEXT("LOCKER_HEAD") : TEXT("LOCKER_BODY")));

    if (ThumbnailImage) ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed);
    DownloadThumbnail(InItem.PreviewURL);
    RefreshState();
}

void UPTSkinWorkshopRowWidget::RefreshState()
{
    if (!Owner) return;
    float Progress = 0.f;
    const EPTSkinRowState State = Owner->GetSkinState(ItemId, bHead, Progress);

    FText Label;
    bool bEnabled = true;
    switch (State)
    {
    case EPTSkinRowState::NotDownloaded:
        Label = PTText::Get(TEXT("SKINWS_DOWNLOAD"));
        break;
    case EPTSkinRowState::Downloading:
    {
        FFormatOrderedArguments Args; Args.Add(FText::AsNumber(FMath::RoundToInt(Progress * 100.f)));
        Label = PTText::Format(TEXT("SKINWS_DOWNLOADING"), Args);
        bEnabled = false;
        break;
    }
    case EPTSkinRowState::Ready:
        Label = PTText::Get(TEXT("LOCKER_EQUIP"));
        break;
    case EPTSkinRowState::Equipped:
        Label = PTText::Get(TEXT("SKINWS_EQUIPPED"));
        bEnabled = false;
        break;
    }
    if (ActionButton)     ActionButton->SetIsEnabled(bEnabled);
    if (ActionButtonText) ActionButtonText->SetText(Label);

    // Editar solo cuando ya está descargada (para editarla hay que tenerla en el Locker).
    const bool bCanEdit = (State == EPTSkinRowState::Ready || State == EPTSkinRowState::Equipped);
    if (EditButton) EditButton->SetVisibility(bCanEdit ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UPTSkinWorkshopRowWidget::OnActionClicked()
{
    if (!Owner) return;
    float Progress = 0.f;
    const EPTSkinRowState State = Owner->GetSkinState(ItemId, bHead, Progress);
    if (State == EPTSkinRowState::NotDownloaded) Owner->DownloadSkin(ItemId);
    else if (State == EPTSkinRowState::Ready)    Owner->EquipSkin(ItemId, bHead);
    RefreshState();
}

void UPTSkinWorkshopRowWidget::OnEditClicked()
{
    if (Owner) Owner->EditSkin(ItemId, bHead);
}

void UPTSkinWorkshopRowWidget::DownloadThumbnail(const FString& Url)
{
    if (Url.IsEmpty() || !ThumbnailImage) return;

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("GET"));
    TWeakObjectPtr<UPTSkinWorkshopRowWidget> WeakThis(this);
    Req->OnProcessRequestComplete().BindLambda(
        [WeakThis](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
        {
            if (!bOk || !Resp.IsValid()) return;
            UPTSkinWorkshopRowWidget* Self = WeakThis.Get(); // la fila pudo destruirse (nueva búsqueda)
            if (!Self || !Self->ThumbnailImage) return;
            const TArray<uint8>& Bytes = Resp->GetContent();
            if (Bytes.Num() == 0) return;
            if (UTexture2D* Tex = FImageUtils::ImportBufferAsTexture2D(Bytes))
            {
                Self->ThumbnailImage->SetBrushFromTexture(Tex, /*bMatchSize=*/false);
                Self->ThumbnailImage->SetVisibility(ESlateVisibility::Visible);
            }
        });
    Req->ProcessRequest();
}
