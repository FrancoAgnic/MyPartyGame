// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTHeadSculptHUDWidget.h"
#include "PTLobbyPlayerController.h"
#include "../UI/PTToolSlotWidget.h"
#include "../PTTextTable.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UPTHeadSculptHUDWidget::BuildOnce()
{
    if (bBuilt || !ToolSlotClass) return;
    bBuilt = true;

    // El modo cabeza usa teclas FIJAS (no la tabla rebindeable del gameplay): es un modo aparte
    // del lobby con su propio input, así que las teclas van a mano acá.
    auto Make = [this](UTexture2D* Icon, const FText& Key, const FText& Label) -> UPTToolSlotWidget*
    {
        UPTToolSlotWidget* S = CreateWidget<UPTToolSlotWidget>(this, ToolSlotClass);
        if (S) S->SetSlot(Icon, Key, Label);
        return S;
    };

    // ── Herramientas (1/2/3/4) ──
    if (ToolsBox)
    {
        ToolsBox->ClearChildren();
        ToolSlots.Reset();
        auto AddTool = [&](UTexture2D* Icon, const TCHAR* Key, const TCHAR* LabelKey)
        {
            if (UPTToolSlotWidget* S = Make(Icon, FText::FromString(Key), PTText::Get(LabelKey)))
            { ToolsBox->AddChild(S); ToolSlots.Add(S); }
        };
        AddTool(IconAdd,   TEXT("1"), TEXT("TOOL_ADD"));
        AddTool(IconErase, TEXT("2"), TEXT("TOOL_ERASE"));
        AddTool(IconPaint, TEXT("3"), TEXT("TOOL_PAINT"));
        AddTool(IconEyes,  TEXT("4"), TEXT("TOOL_EYES"));
    }

    // ── Formas (TAB) ── todas muestran TAB; se resalta la equipada.
    if (ShapesBox)
    {
        ShapesBox->ClearChildren();
        ShapeSlots.Reset();
        auto AddShape = [&](UTexture2D* Icon, const TCHAR* LabelKey)
        {
            if (UPTToolSlotWidget* S = Make(Icon, FText::FromString(TEXT("TAB")), PTText::Get(LabelKey)))
            { ShapesBox->AddChild(S); ShapeSlots.Add(S); }
        };
        AddShape(IconSphere,   TEXT("SHAPE_SPHERE"));
        AddShape(IconCube,     TEXT("SHAPE_CUBE"));
        AddShape(IconCylinder, TEXT("SHAPE_CYLINDER"));
        AddShape(IconCone,     TEXT("SHAPE_CONE"));
    }

    // ── Slots contextuales fijos (color / salir) y cruz WASD ──
    if (ColorSlot) ColorSlot->SetSlot(IconColor, FText::FromString(TEXT("RMB")), PTText::Get(TEXT("KEY_COLOR_PICK")));
    if (ExitSlot)  ExitSlot->SetSlot(IconExit,   FText::FromString(TEXT("G")),   PTText::Get(TEXT("HEAD_APPLY")));

    // La cruz WASD: cada tecla en su lugar (W arriba, S abajo, A izq, D der). El "glow" es el
    // resaltado (SetSelected) que se prende mientras la tecla está apretada.
    if (WasdUp)    WasdUp->SetSlot(nullptr,    FText::FromString(TEXT("W")), FText::GetEmpty());
    if (WasdDown)  WasdDown->SetSlot(nullptr,  FText::FromString(TEXT("S")), FText::GetEmpty());
    if (WasdLeft)  WasdLeft->SetSlot(nullptr,  FText::FromString(TEXT("A")), FText::GetEmpty());
    if (WasdRight) WasdRight->SetSlot(nullptr, FText::FromString(TEXT("D")), FText::GetEmpty());

    if (OutOfBoundsIcon) OutOfBoundsIcon->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTHeadSculptHUDWidget::RefreshControlsText()
{
    if (!ControlsText) return;
    TArray<FString> Lines;
    Lines.Add(PTText::GetStr(TEXT("HEAD_CTRL_CAMERA")));
    Lines.Add(PTText::GetStr(TEXT("HEAD_CTRL_BRUSH")));
    Lines.Add(PTText::GetStr(TEXT("HEAD_CTRL_COLOR")));
    Lines.Add(PTText::GetStr(TEXT("HEAD_CTRL_EXIT")));
    ControlsText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

void UPTHeadSculptHUDWidget::Refresh(APTLobbyPlayerController* PC)
{
    BuildOnce();
    RefreshControlsText();
    if (!PC) return;

    // ── Herramienta equipada: 0=Add 1=Erase 2=Paint 3=Eyes ──
    const int32 Equipped = PC->IsHeadEyesToolActive() ? 3
        : (PC->GetHeadEditMode() == EPTEditMode::Add   ? 0
        :  PC->GetHeadEditMode() == EPTEditMode::Erase ? 1
        :  PC->GetHeadEditMode() == EPTEditMode::Paint ? 2 : -1);
    for (int32 i = 0; i < ToolSlots.Num(); ++i)
        if (ToolSlots[i]) ToolSlots[i]->SetSelected(i == Equipped);

    // ── Formas: solo se muestran con Add/Erase/Paint (no con Ojos); se resalta la equipada ──
    const bool bShowShapes = PC->HeadToolUsesShapes();
    if (ShapesBox)
        ShapesBox->SetVisibility(bShowShapes ? ESlateVisibility::HitTestInvisible
                                             : ESlateVisibility::Collapsed);
    if (bShowShapes)
    {
        const int32 ShapeIdx = (int32)PC->GetHeadStampShape(); // Sphere, Cube, Cylinder, TriPrism
        for (int32 i = 0; i < ShapeSlots.Num(); ++i)
            if (ShapeSlots[i]) ShapeSlots[i]->SetSelected(i == ShapeIdx);
    }

    // ── El slot de color se resalta mientras el picker está abierto ──
    if (ColorSlot) ColorSlot->SetSelected(PC->IsHeadColorPickerOpen());

    // ── Cruz WASD: brillan mientras se mantiene la tecla ──
    if (WasdUp)    WasdUp->SetSelected(PC->IsInputKeyDown(EKeys::W));
    if (WasdDown)  WasdDown->SetSelected(PC->IsInputKeyDown(EKeys::S));
    if (WasdLeft)  WasdLeft->SetSelected(PC->IsInputKeyDown(EKeys::A));
    if (WasdRight) WasdRight->SetSelected(PC->IsInputKeyDown(EKeys::D));

    // ── 🚫 fuera del área de esculpido ──
    if (OutOfBoundsIcon)
        OutOfBoundsIcon->SetVisibility(PC->IsHeadStampOutside() ? ESlateVisibility::HitTestInvisible
                                                                : ESlateVisibility::Collapsed);
}
