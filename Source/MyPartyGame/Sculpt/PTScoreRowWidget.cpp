#include "PTScoreRowWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UPTScoreRowWidget::SetRow(const FString& Name, int32 Score, bool bSculptor)
{
    if (TxtName)
    {
        TxtName->SetText(FText::FromString(FString::Printf(TEXT("%s: %d"), *Name.Left(10), Score)));

        // Contorno amarillo solo para el escultor (0 = sin contorno para el resto).
        FSlateFontInfo Font = TxtName->GetFont();
        Font.OutlineSettings.OutlineSize  = bSculptor ? SculptorOutlineSize : 0;
        Font.OutlineSettings.OutlineColor = SculptorOutlineColor;
        TxtName->SetFont(Font);
    }

    // La imagen del pincel solo se ve en la fila del que esculpe.
    if (ImgBrush)
        ImgBrush->SetVisibility(bSculptor ? ESlateVisibility::HitTestInvisible
                                          : ESlateVisibility::Collapsed);
}
