// Copyright Epic Games, Inc. All Rights Reserved.
// Traduce la UI SOLO, sin bindear nada en el Editor.
//
// Idea: en los WBP se escribe el texto en español, como siempre. Este subsistema mira cada tanto
// los widgets que están en pantalla y cambia los textos que reconoce (los que están en DT_UITexts)
// al idioma elegido. Los textos dinámicos (nombres, puntajes, el reloj, la palabra a adivinar) no
// coinciden con ninguna fila, así que quedan intactos.
//
// Se revisa por sondeo y no por un evento porque Unreal no expone un aviso global de "se construyó
// un widget"; el barrido es barato (solo widgets en viewport) y corre 2 veces por segundo.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "PTLocalizationSubsystem.generated.h"

UCLASS()
class MYPARTYGAME_API UPTLocalizationSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Traduce ya mismo todo lo que esté en pantalla (se llama al cambiar de idioma). */
    void LocalizeAllOnScreen();

private:
    bool Tick(float DeltaTime);

    FTSTicker::FDelegateHandle TickerHandle;
    FDelegateHandle            LanguageHandle;
};
