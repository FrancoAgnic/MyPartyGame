#include "PTWidgetUtils.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

namespace
{
    template<typename TSlot>
    void CopySlot(UPanelSlot* DstSlot, UPanelSlot* SrcSlot)
    {
        TSlot* Dst = Cast<TSlot>(DstSlot);
        const TSlot* Src = Cast<TSlot>(SrcSlot);
        if (!Dst || !Src) return;
        Dst->SetPadding(Src->GetPadding());
        Dst->SetSize(Src->GetSize());
        Dst->SetHorizontalAlignment(Src->GetHorizontalAlignment());
        Dst->SetVerticalAlignment(Src->GetVerticalAlignment());
    }
}

UButton* PTWidgetUtils::CloneButtonAfter(UUserWidget* Owner, UButton* Src, FName Name, const FText& Label)
{
    if (!Owner || !Src || !Owner->WidgetTree) return nullptr;
    UPanelWidget* Parent = Src->GetParent();
    if (!Parent || !(Parent->IsA<UVerticalBox>() || Parent->IsA<UHorizontalBox>()))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UI] No se pudo crear '%s' al lado de '%s' en %s (no está en una Vertical/Horizontal Box): agregarlo en el WBP."),
            *Name.ToString(), *Src->GetName(), *Owner->GetClass()->GetName());
        return nullptr;
    }

    UButton* Btn = Owner->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    Btn->SetStyle(Src->GetStyle());
    Btn->SetColorAndOpacity(Src->GetColorAndOpacity());
    Btn->SetBackgroundColor(Src->GetBackgroundColor());

    UTextBlock* Text = Owner->WidgetTree->ConstructWidget<UTextBlock>();
    if (const UTextBlock* SrcText = Cast<UTextBlock>(Src->GetChildAt(0)))
    {
        Text->SetFont(SrcText->GetFont());
        Text->SetColorAndOpacity(SrcText->GetColorAndOpacity());
        Text->SetShadowOffset(SrcText->GetShadowOffset());
        Text->SetShadowColorAndOpacity(SrcText->GetShadowColorAndOpacity());
    }
    Text->SetText(Label);
    Btn->AddChild(Text);

    UPanelSlot* NewSlot = Parent->InsertChildAt(Parent->GetChildIndex(Src) + 1, Btn);
    CopySlot<UVerticalBoxSlot>(NewSlot, Src->Slot);
    CopySlot<UHorizontalBoxSlot>(NewSlot, Src->Slot);
    return Btn;
}

namespace
{
    // Busca el PRIMER UTextBlock dentro de un widget (él mismo o descendiente). El label de un botón
    // suele ser su hijo directo, pero a veces el WBP lo envuelve en una caja (icono + texto).
    UTextBlock* FindFirstTextBlock(UWidget* W)
    {
        if (!W) return nullptr;
        if (UTextBlock* T = Cast<UTextBlock>(W)) return T;
        if (UPanelWidget* P = Cast<UPanelWidget>(W))
            for (int32 i = 0; i < P->GetChildrenCount(); ++i)
                if (UTextBlock* Found = FindFirstTextBlock(P->GetChildAt(i)))
                    return Found;
        return nullptr;
    }
}

void PTWidgetUtils::SetButtonLabel(UButton* Btn, const FText& Label)
{
    if (!Btn) return;
    if (UTextBlock* Text = FindFirstTextBlock(Btn->GetChildAt(0)))
        Text->SetText(Label);
}
