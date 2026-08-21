// Copyright Epic Games, Inc. All Rights Reserved.
// Una fila de la lista de jugadores del lobby (arriba a la derecha). El HUD crea una por jugador y las
// apila. En el WBP derivado (nombres EXACTOS; opcionales):
//   NameText   (TextBlock) → nombre del jugador (se recorta a un máximo de caracteres)
//   HostCrown  (Image)     → corona; se muestra SOLO si el jugador es el anfitrión
//   ReadyCheck (Image)     → tilde/aspa de listo; cambia de textura y se tiñe verde(listo)/rojo(no listo)
//   KickButton (Button)    → expulsar; se muestra SOLO en las filas de OTROS cuando vos sos el host
// Las texturas de listo/no-listo se asignan en Details del WBP (ReadyTex / NotReadyTex).

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTPlayerRowWidget.generated.h"

class UTextBlock;
class UImage;
class UButton;
class APTPlayerState;

UCLASS()
class MYPARTYGAME_API UPTPlayerRowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    /** Configura la fila. MaxChars recorta el nombre para que no se salga de la pantalla.
     *  Target = PlayerState de esta fila; bCanKick = mostrar el botón Kick (vos sos host y no es host/vos). */
    void SetRow(const FString& Name, bool bHost, bool bReady,
                const FLinearColor& ReadyColor, const FLinearColor& NotReadyColor, int32 MaxChars,
                APTPlayerState* Target = nullptr, bool bCanKick = false);

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* NameText;
    UPROPERTY(meta = (BindWidgetOptional)) UImage*     HostCrown;
    UPROPERTY(meta = (BindWidgetOptional)) UImage*     ReadyCheck;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    KickButton;

    // Texturas del check (asignar en el WBP de la fila): con tilde = listo; vacía/aspa = no listo.
    UPROPERTY(EditAnywhere, Category = "Row") UTexture2D* ReadyTex    = nullptr;
    UPROPERTY(EditAnywhere, Category = "Row") UTexture2D* NotReadyTex = nullptr;

    UFUNCTION() void OnKickClicked();

private:
    TWeakObjectPtr<APTPlayerState> TargetPS;
};
