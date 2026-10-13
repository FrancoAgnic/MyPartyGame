// Cartel final del tutorial RÁPIDO: "¿Querés hacer el tutorial avanzado?" con Sí / No.
// Armado en C++ (sin WBP), como los otros paneles del juego. Lo crea APTTutorialDirector.

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTTutorialChoiceWidget.generated.h"

class UButton;
class UTextBlock;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE(FPTChoiceEvent);

UCLASS()
class MYPARTYGAME_API UPTTutorialChoiceWidget : public UPTUserWidget
{
    GENERATED_BODY()
public:
    /** Texto de la pregunta + etiquetas de los dos botones. */
    void Setup(const FText& Question, const FText& YesLabel, const FText& NoLabel);

    /** Meme "expectativa vs realidad" arriba de la pregunta: a la izquierda la foto de TU jirafa (resultado
     *  del tutorial básico), a la derecha una jirafa muy realista (resultado del avanzado), cada una con su
     *  cartelito. Llamar ANTES de AddToViewport (se arma en BuildTree). Si BasicImg es nulo, no se muestra. */
    void SetMeme(UTexture2D* BasicImg, UTexture2D* AdvancedImg, const FText& BasicCaption, const FText& AdvancedCaption,
                 const FText& AdvancedPlaceholder = FText::GetEmpty());

    FPTChoiceEvent OnYes;
    FPTChoiceEvent OnNo;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    void BuildTree();
    UButton* MakeButton(const FText& Label, FName Name, const FLinearColor& Fill, UTextBlock** OutText);

    UFUNCTION() void HandleYes();
    UFUNCTION() void HandleNo();

    /** Arma una columna del meme (imagen + cartelito debajo). Si Img es nulo, muestra Placeholder (texto)
     *  en el recuadro. Devuelve el contenedor para el slot. */
    class UWidget* MakeMemeColumn(UTexture2D* Img, const FText& Caption, const FText& Placeholder);

    UPROPERTY() UTextBlock* QuestionText = nullptr;
    UPROPERTY() UTextBlock* YesText = nullptr;
    UPROPERTY() UTextBlock* NoText = nullptr;

    FText PendingQuestion, PendingYes, PendingNo;

    // Meme del final (resultado básico vs avanzado). Se guardan hasta BuildTree.
    UPROPERTY() UTexture2D* PendingBasicImg = nullptr;
    UPROPERTY() UTexture2D* PendingAdvImg   = nullptr;
    FText PendingBasicCaption, PendingAdvCaption, PendingAdvPlaceholder;
};
