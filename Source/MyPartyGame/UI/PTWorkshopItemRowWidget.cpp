// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTWorkshopItemRowWidget.h"
#include "PTWorkshopBrowserWidget.h"
#include "../PTTextTable.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

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
}

void UPTWorkshopItemRowWidget::OnAddClicked()
{
    if (Owner) Owner->AddItem(ItemId);
    bAdded = true;
    if (AddButton)     AddButton->SetIsEnabled(false);
    if (AddButtonText) AddButtonText->SetText(PTText::Get(TEXT("WORKSHOP_ADDED")));
}
