#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PTSculptVolume.h"
#include "PTSculptPlayerController.generated.h"

UCLASS()
class MYPARTYGAME_API APTSculptPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    APTSculptPlayerController();

    UPROPERTY(EditAnywhere, Category="Sculpt")
    float BrushRadius = 80.f;

    UPROPERTY(EditAnywhere, Category="Sculpt")
    float BrushStrength = 2.5f;

    UPROPERTY(EditAnywhere, Category="Sculpt")
    float RadiusStep = 10.f;

    UPROPERTY(EditAnywhere, Category="Sculpt")
    float TraceDistance = 5000.f;

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;

private:
    EPTBrushMode   CurrentMode = EPTBrushMode::Add;
    APTSculptVolume* Volume    = nullptr;
    bool bIsSculpting          = false;

    void SetModeAdd()     { CurrentMode = EPTBrushMode::Add;    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Mode: Add")); }
    void SetModeRemove()  { CurrentMode = EPTBrushMode::Remove; UE_LOG(LogTemp, Log, TEXT("[Sculpt] Mode: Remove")); }
    void SetModeSmooth()  { CurrentMode = EPTBrushMode::Smooth; UE_LOG(LogTemp, Log, TEXT("[Sculpt] Mode: Smooth")); }
    void OnSculptPressed()  { bIsSculpting = true;  }
    void OnSculptReleased() { bIsSculpting = false; }
    void OnScrollUp()   { BrushRadius = FMath::Max(20.f, BrushRadius + RadiusStep); UE_LOG(LogTemp, Log, TEXT("[Sculpt] Radius: %.0f"), BrushRadius); }
    void OnScrollDown() { BrushRadius = FMath::Max(20.f, BrushRadius - RadiusStep); UE_LOG(LogTemp, Log, TEXT("[Sculpt] Radius: %.0f"), BrushRadius); }

    bool GetSculptHitPoint(FVector& OutHit) const;
};
