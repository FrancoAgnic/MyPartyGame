// Copyright Epic Games, Inc. All Rights Reserved.
// Clase base de TODOS los widgets de UI del juego. Su único trabajo extra: al construirse, aplicar
// los sonidos de UI (hover/click) del GameInstance a TODOS los botones del widget, sin tener que
// tocar cada botón a mano. Reproducir el sonido lo hace Slate solo (via el estilo del botón).

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTUserWidget.generated.h"

UCLASS()
class MYPARTYGAME_API UPTUserWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
};
