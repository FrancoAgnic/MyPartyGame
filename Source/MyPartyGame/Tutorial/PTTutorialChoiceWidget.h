// Cartel final del tutorial RÁPIDO: "¿Querés hacer el tutorial avanzado?" con Sí / No.
// Armado en C++ (sin WBP), como los otros paneles del juego. Lo crea APTTutorialDirector.

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTTutorialChoiceWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE(FPTChoiceEvent);

UCLASS()
class MYPARTYGAME_API UPTTutorialChoiceWidget : public UPTUserWidget
{
    GENERATED_BODY()
public:
    /** Texto de la pregunta + etiquetas de los dos botones. */
    void Setup(const FText& Question, const FText& YesLabel, const FText& NoLabel);

    FPTChoiceEvent OnYes;
    FPTChoiceEvent OnNo;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    void BuildTree();
    UButton* MakeButton(const FText& Label, FName Name, const FLinearColor& Fill, UTextBlock** OutText);

    UFUNCTION() void HandleYes();
    UFUNCTION() void HandleNo();

    UPROPERTY() UTextBlock* QuestionText = nullptr;
    UPROPERTY() UTextBlock* YesText = nullptr;
    UPROPERTY() UTextBlock* NoText = nullptr;

    FText PendingQuestion, PendingYes, PendingNo;
};
