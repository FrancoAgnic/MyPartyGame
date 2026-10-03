// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSkinRowWidget.h"
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

bool UPTSkinRowWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (AddButton)   AddButton->OnClicked.AddDynamic(this, &UPTSkinRowWidget::OnAddClicked);
    if (EquipButton) EquipButton->OnClicked.AddDynamic(this, &UPTSkinRowWidget::OnEquipClicked);
    return true;
}

void UPTSkinRowWidget::Init(const FPTWorkshopItem& InItem, UPTSkinWorkshopWidget* InOwner)
{
    ItemId = InItem.Id;
    bAdded = InItem.bSubscribed;
    Owner  = InOwner;

    if (TitleText) TitleText->SetText(FText::FromString(InItem.Title));
    // Botón inteligente SIEMPRE clickable: "Añadir" si no estás suscrito (suscribe/descarga); "Equipar"
    // si ya lo tenés (tu propia skin queda auto-suscrita por Steam, o una que ya añadiste) → al tocarlo
    // la importa al Locker y la equipa. Antes se deshabilitaba si ya estabas suscrito → no se podía tocar.
    if (AddButton)     AddButton->SetIsEnabled(true);
    if (AddButtonText) AddButtonText->SetText(PTText::Get(bAdded ? TEXT("LOCKER_EQUIP") : TEXT("WORKSHOP_ADD")));

    if (DescText)
    {
        FString D = InItem.Description.TrimStartAndEnd();
        if (D.Len() > 200) D = D.Left(197) + TEXT("...");
        DescText->SetText(FText::FromString(D));
    }

    if (ThumbnailImage) ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed);
    DownloadThumbnail(InItem.PreviewURL);
}

void UPTSkinRowWidget::DownloadThumbnail(const FString& Url)
{
    if (Url.IsEmpty() || !ThumbnailImage) return;
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("GET"));
    TWeakObjectPtr<UPTSkinRowWidget> WeakThis(this);
    Req->OnProcessRequestComplete().BindLambda(
        [WeakThis](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
        {
            if (!bOk || !Resp.IsValid()) return;
            UPTSkinRowWidget* Self = WeakThis.Get();
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

void UPTSkinRowWidget::OnAddClicked()
{
    if (!bAdded)
    {
        // No suscrito → suscribir (Steam lo descarga). El botón pasa a "Equipar" para cuando termine.
        if (Owner) Owner->AddItem(ItemId);
        bAdded = true;
        if (AddButtonText) AddButtonText->SetText(PTText::Get(TEXT("LOCKER_EQUIP")));
    }
    else if (Owner)
    {
        // Ya suscrito/descargado → importar al Locker + equipar (si todavía descarga, avisa).
        Owner->EquipItem(ItemId);
    }
}

void UPTSkinRowWidget::OnEquipClicked()
{
    if (Owner) Owner->EquipItem(ItemId);
}
