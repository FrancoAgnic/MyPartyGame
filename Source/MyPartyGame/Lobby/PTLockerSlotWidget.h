// Copyright Epic Games, Inc. All Rights Reserved.
// Un slot del Locker: un "tile" que se SELECCIONA (click). Los botones de acción (Asignar/Editar) son
// globales, abajo, y operan sobre el slot seleccionado. El contenedor (UPTLockerWidget) crea 6 de
// cabeza + 6 de cuerpo por código.
// En el WBP derivado (opcionales, nombres EXACTOS):
//   SlotButton      (Button)    → todo el tile clickeable; al tocarlo selecciona el slot
//   Thumbnail       (Image)     → miniatura de la creación (se llena en la Parte C)
//   LabelText       (TextBlock) → "Cabeza 1" / "Vacío"
//   EquippedMark    (Image)     → se muestra solo si este slot está equipado
//   SelectionBorder (Border/Image) → resaltado; se muestra solo cuando este slot está seleccionado

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../UI/PTUserWidget.h"
#include "PTLockerSlotWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UWidget;
class UPTLockerWidget;

UCLASS()
class MYPARTYGAME_API UPTLockerSlotWidget : public UPTUserWidget
{
    GENERATED_BODY()
public:
    void Setup(UPTLockerWidget* InOwner, int32 InIndex, bool bInHead, bool bUsed, bool bEquipped);
    void SetSelected(bool bSel);
    void SetThumbnailTexture(UTexture2D* Tex); // miniatura renderizada de la creación
    int32 GetIndex() const { return SlotIndex; }
    bool  IsHeadSlot() const { return bIsHead; }
    bool  IsUsed() const { return bUsed; }
    bool  IsSlotHovered() const; // true si el mouse está sobre este tile (SlotButton)

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional)) UButton*    SlotButton;
    UPROPERTY(meta = (BindWidgetOptional)) UImage*     Thumbnail;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* LabelText;
    UPROPERTY(meta = (BindWidgetOptional)) UImage*     EquippedMark;
    UPROPERTY(meta = (BindWidgetOptional)) UWidget*    SelectionBorder;

    UFUNCTION() void OnSlotClicked();
    UFUNCTION() void OnSlotHovered();
    UFUNCTION() void OnSlotUnhovered();

private:
    UPROPERTY() UPTLockerWidget* Owner = nullptr;
    int32 SlotIndex = 0;
    bool  bIsHead   = true;
    bool  bUsed     = false;
    bool  bBound    = false;
    bool  bSelected = false; // para restaurar el borde al salir del hover
};
