// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLockerSlotWidget.h"
#include "PTLockerWidget.h"
#include "../PTTextTable.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UPTLockerSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (!bBound)
    {
        if (SlotButton)
        {
            SlotButton->OnClicked.AddDynamic(this, &UPTLockerSlotWidget::OnSlotClicked);
            SlotButton->OnHovered.AddDynamic(this, &UPTLockerSlotWidget::OnSlotHovered);
            SlotButton->OnUnhovered.AddDynamic(this, &UPTLockerSlotWidget::OnSlotUnhovered);
        }
        bBound = true;
    }
}

void UPTLockerSlotWidget::Setup(UPTLockerWidget* InOwner, int32 InIndex, bool bInHead, bool bInUsed, bool bEquipped)
{
    Owner = InOwner; SlotIndex = InIndex; bIsHead = bInHead; bUsed = bInUsed;

    if (LabelText)
    {
        const FString Base = FString::Printf(TEXT("%s %d"),
            *PTText::GetStr(bIsHead ? TEXT("LOCKER_HEAD") : TEXT("LOCKER_BODY")), SlotIndex + 1);
        LabelText->SetText(FText::FromString(
            bUsed ? Base : FString::Printf(TEXT("%s — %s"), *Base, *PTText::GetStr(TEXT("LOCKER_EMPTY")))));
    }
    if (EquippedMark)
        EquippedMark->SetVisibility(bEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    // La miniatura (Thumbnail) se setea en la Parte C; acá solo se deja como esté.
}

void UPTLockerSlotWidget::SetSelected(bool bSel)
{
    if (SelectionBorder)
        SelectionBorder->SetVisibility(bSel ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UPTLockerSlotWidget::SetThumbnailTexture(UTexture2D* Tex)
{
    if (!Thumbnail) return;
    if (Tex) { Thumbnail->SetBrushFromTexture(Tex); Thumbnail->SetVisibility(ESlateVisibility::HitTestInvisible); }
    else       Thumbnail->SetVisibility(ESlateVisibility::Hidden); // slot vacío → sin miniatura
}

void UPTLockerSlotWidget::OnSlotClicked()
{
    if (!Owner) return;
    if (bUsed) Owner->EquipSlotNow(SlotIndex, bIsHead);  // click en slot lleno = equipar directo
    else       Owner->CreateSlotNow(SlotIndex, bIsHead); // click en slot vacío = crear directo
}

void UPTLockerSlotWidget::OnSlotHovered()
{
    if (Owner && bUsed) Owner->HoverSlot(SlotIndex, bIsHead); // preview de la skin en el personaje
}

void UPTLockerSlotWidget::OnSlotUnhovered()
{
    // No revertimos acá: el tick del locker detecta cuando NO hay ningún slot bajo el mouse y ahí vuelve
    // a lo equipado. Así al pasar de un slot a otro no parpadea el equipado en el medio.
}

bool UPTLockerSlotWidget::IsSlotHovered() const
{
    return SlotButton && SlotButton->IsHovered();
}
