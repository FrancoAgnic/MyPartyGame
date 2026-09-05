// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTWordPackRowWidget.h"
#include "PTWordPackWidget.h"
#include "../PTTextTable.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "ImageUtils.h"                 // ImportBufferAsTexture2D (miniatura)
#include "Engine/Texture2D.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

bool UPTWordPackRowWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (UseButton) UseButton->OnClicked.AddDynamic(this, &UPTWordPackRowWidget::OnUseClicked);
    return true;
}

void UPTWordPackRowWidget::Init(const FPTWordPack& InPack, bool bSelected, UPTWordPackWidget* InOwner)
{
    PackId = InPack.Id;
    Owner  = InOwner;

    if (TitleText) TitleText->SetText(FText::FromString(InPack.Title));

    if (AuthorText)
    {
        if (InPack.Author.IsEmpty())
        {
            AuthorText->SetText(FText::GetEmpty());
        }
        else
        {
            FFormatOrderedArguments Args;
            Args.Add(FText::FromString(InPack.Author));
            AuthorText->SetText(PTText::Format(TEXT("WORDPACK_BY"), Args));
        }
    }

    // Si ya es el banco elegido, deshabilitar el botón y mostrar "En uso".
    if (UseButton)     UseButton->SetIsEnabled(!bSelected);
    if (UseButtonText) UseButtonText->SetText(PTText::Get(bSelected ? TEXT("WORDPACK_INUSE") : TEXT("WORDPACK_USE")));

    // Mismo look que el browser: descripción, tag de tipo y miniatura (para packs del Workshop).
    if (DescText)
    {
        FString D = InPack.Description.TrimStartAndEnd();
        if (D.Len() > 200) D = D.Left(197) + TEXT("...");
        DescText->SetText(FText::FromString(D));
    }
    if (TypeTagText)
        TypeTagText->SetText(PTText::Get(InPack.Tags.Contains(TEXT("Map"))
            ? TEXT("WORKSHOP_TAG_MAP") : TEXT("WORKSHOP_TAG_WORDBANK")));

    if (ThumbnailImage) ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed);
    DownloadThumbnail(InPack.PreviewURL); // vacío en packs locales → no hace nada
}

void UPTWordPackRowWidget::DownloadThumbnail(const FString& Url)
{
    if (Url.IsEmpty() || !ThumbnailImage) return;

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("GET"));
    TWeakObjectPtr<UPTWordPackRowWidget> WeakThis(this);
    Req->OnProcessRequestComplete().BindLambda(
        [WeakThis](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
        {
            if (!bOk || !Resp.IsValid()) return;
            UPTWordPackRowWidget* Self = WeakThis.Get(); // la fila pudo destruirse (rebuild de la lista)
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

void UPTWordPackRowWidget::OnUseClicked()
{
    if (Owner) Owner->UsePack(PackId);
}
