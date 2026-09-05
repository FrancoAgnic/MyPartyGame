// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTWorkshopItemRowWidget.h"
#include "PTWorkshopBrowserWidget.h"
#include "../PTTextTable.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "ImageUtils.h"                 // ImportBufferAsTexture2D (miniatura)
#include "Engine/Texture2D.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

bool UPTWorkshopItemRowWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (AddButton) AddButton->OnClicked.AddDynamic(this, &UPTWorkshopItemRowWidget::OnAddClicked);
    return true;
}

void UPTWorkshopItemRowWidget::Init(const FPTWorkshopItem& InItem, UPTWorkshopBrowserWidget* InOwner)
{
    ItemId = InItem.Id;
    bAdded = InItem.bSubscribed;
    Owner  = InOwner;

    if (TitleText) TitleText->SetText(FText::FromString(InItem.Title));

    if (AddButton)     AddButton->SetIsEnabled(!bAdded);
    if (AddButtonText) AddButtonText->SetText(PTText::Get(bAdded ? TEXT("WORKSHOP_ADDED") : TEXT("WORKSHOP_ADD")));

    // Descripción (recortada para no agrandar la fila; el WBP puede envolver/limitar líneas igual).
    if (DescText)
    {
        FString D = InItem.Description.TrimStartAndEnd();
        if (D.Len() > 200) D = D.Left(197) + TEXT("...");
        DescText->SetText(FText::FromString(D));
    }

    // Tipo de item por su tag de Steam ("Map" → Mapa; si no, Banco de palabras).
    if (TypeTagText)
        TypeTagText->SetText(PTText::Get(InItem.Tags.Contains(TEXT("Map"))
            ? TEXT("WORKSHOP_TAG_MAP") : TEXT("WORKSHOP_TAG_WORDBANK")));

    // Miniatura: arranca oculta y se muestra cuando termina de bajar.
    if (ThumbnailImage) ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed);
    DownloadThumbnail(InItem.PreviewURL);
}

void UPTWorkshopItemRowWidget::DownloadThumbnail(const FString& Url)
{
    if (Url.IsEmpty() || !ThumbnailImage) return;

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("GET"));
    TWeakObjectPtr<UPTWorkshopItemRowWidget> WeakThis(this);
    Req->OnProcessRequestComplete().BindLambda(
        [WeakThis](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
        {
            if (!bOk || !Resp.IsValid()) return;
            UPTWorkshopItemRowWidget* Self = WeakThis.Get(); // la fila pudo destruirse (nueva búsqueda)
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

void UPTWorkshopItemRowWidget::OnAddClicked()
{
    if (Owner) Owner->AddItem(ItemId);
    bAdded = true;
    if (AddButton)     AddButton->SetIsEnabled(false);
    if (AddButtonText) AddButtonText->SetText(PTText::Get(TEXT("WORKSHOP_ADDED")));
}
