// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTWordPackRowWidget.h"
#include "PTWordPackWidget.h"
#include "../PTTextTable.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

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
}

void UPTWordPackRowWidget::OnUseClicked()
{
    if (Owner) Owner->UsePack(PackId);
}
