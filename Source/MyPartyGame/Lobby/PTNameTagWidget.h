// Cartel con el nombre del jugador (sobre la cabeza). Toda la lógica en C++: el
// personaje le pasa el DisplayName. El WBP solo aporta el TextBlock "NameText".

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTNameTagWidget.generated.h"

class UTextBlock;

UCLASS()
class MYPARTYGAME_API UPTNameTagWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Setea el texto del cartel. Lo llama el personaje con el DisplayName replicado. */
    void SetPlayerName(const FString& InName);

protected:
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* NameText;
};
