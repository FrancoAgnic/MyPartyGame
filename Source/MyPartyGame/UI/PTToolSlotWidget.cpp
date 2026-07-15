// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTToolSlotWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

void UPTToolSlotWidget::SetSlot(UTexture2D* Icon, const FText& KeyName, const FText& Label)
{
    if (IconImage)
    {
        if (Icon) IconImage->SetBrushFromTexture(Icon);
        // Sin icono asignado el cuadrito igual sirve (queda solo la tecla + el nombre).
        IconImage->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (KeyText)   KeyText->SetText(KeyName);
    if (LabelText) LabelText->SetText(Label);

    // Los cuadritos normales no usan el círculo de progreso: arrancan sin él (solo el de
    // "borrar todo" lo activa, actualizándolo por frame desde el HUD).
    SetProgress(0.f, FText::GetEmpty());
}

void UPTToolSlotWidget::SetSelected(bool bInSelected)
{
    if (bSelected == bInSelected) return; // no re-disparar el evento de BP cada tick
    bSelected = bInSelected;

    if (SelectedMarker)
        SelectedMarker->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible
                                                : ESlateVisibility::Collapsed);
    OnSelectedChanged(bSelected);
}

void UPTToolSlotWidget::SetProgress(float Alpha01, const FText& CenterText)
{
    Alpha01 = FMath::Clamp(Alpha01, 0.f, 1.f);
    if (FMath::IsNearlyEqual(Alpha01, LastProgress, 0.005f)) return; // no spamear cada tick
    LastProgress = Alpha01;

    const bool bActive = Alpha01 > 0.f;

    if (ProgressRing)
    {
        ProgressRing->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible
                                            : ESlateVisibility::Collapsed);
        if (bActive)
        {
            // MID del material del brush: se crea una sola vez y se le va seteando el relleno.
            if (!ProgressMID)
                ProgressMID = ProgressRing->GetDynamicMaterial();
            if (ProgressMID)
                ProgressMID->SetScalarParameterValue(ProgressParamName, Alpha01);
        }
    }

    if (ProgressText)
    {
        ProgressText->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible
                                            : ESlateVisibility::Collapsed);
        if (bActive) ProgressText->SetText(CenterText);
    }

    OnProgressChanged(Alpha01);
}
