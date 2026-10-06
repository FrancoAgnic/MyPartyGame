// Borde de foco del joystick (lo maneja UPTGamepadUINavigator). Armado en C++, sin WBP.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTGamepadFocusWidget.generated.h"

class UBorder;

UCLASS()
class MYPARTYGAME_API UPTGamepadFocusWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Ubica el borde sobre un rectángulo en coordenadas de VIEWPORT (unidades de UMG). */
    void SetTarget(const FVector2D& ViewportPos, const FVector2D& ViewportSize, float Time);
    void HideTarget();

    UPROPERTY(EditAnywhere, Category="Gamepad") FLinearColor OutlineColor = FLinearColor(1.f, 0.78f, 0.2f, 1.f);
    UPROPERTY(EditAnywhere, Category="Gamepad") float OutlineWidth = 4.f;
    UPROPERTY(EditAnywhere, Category="Gamepad") float Margin = 6.f;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    UPROPERTY() UBorder* Frame = nullptr;
};
