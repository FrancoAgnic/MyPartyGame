// Copyright Epic Games, Inc. All Rights Reserved.
// Fila de un banco de palabras en el panel del lobby (título + autor + botón Usar).
// La llena UPTWordPackWidget con un FPTWordPack.
//
// En el WBP derivado (nombres EXACTOS):
//   TitleText  (TextBlock) → título del banco
//   AuthorText (TextBlock) → "por <autor>"  (opcional)
//   UseButton  (Button)    → elegir este banco para la partida
//   UseButtonText (TextBlock) → texto del botón (opcional; "Usar"/"En uso")

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "Mods/PTWordPackSubsystem.h" // FPTWordPack
#include "Mods/PTMapModSubsystem.h"   // FPTMapMod (la misma fila sirve para mapas)
#include "PTWordPackRowWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;
class UPTWordPackWidget;

UCLASS()
class MYPARTYGAME_API UPTWordPackRowWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Carga la fila. bSelected = es el banco elegido ahora (deshabilita el botón y muestra "En uso"). */
    void Init(const FPTWordPack& InPack, bool bSelected, UPTWordPackWidget* InOwner);
    /** Igual pero para un MAPA de mod (misma fila). "Usar" enruta a SelectMapMod. */
    void InitMap(const FPTMapMod& InMap, bool bSelected, UPTWordPackWidget* InOwner);

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget))         UTextBlock* TitleText;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* AuthorText;
    UPROPERTY(meta = (BindWidget))         UButton*    UseButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* UseButtonText;
    // Mismos datos que el browser (opcionales): miniatura, descripción y tag de tipo.
    UPROPERTY(meta = (BindWidgetOptional)) UImage*     ThumbnailImage;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* DescText;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TypeTagText;

    UFUNCTION() void OnUseClicked();

private:
    FString PackId;
    bool    bIsMap = false; // esta fila representa un MAPA (no un banco) → "Usar" llama SelectMapMod
    UPROPERTY() UPTWordPackWidget* Owner = nullptr;

    void DownloadThumbnail(const FString& Url); // baja el preview por HTTP → ThumbnailImage
};
