#include "PTPartyBotController.h"
#include "PTSculptVolume.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

APTPartyBotController::APTPartyBotController()
{
    PrimaryActorTick.bCanEverTick = true;
    bWantsPlayerState = false; // la PlayerState es la del celular (se le asigna al pawn)
}

bool APTPartyBotController::GetCanvas(FVector& OutCenter, FVector& OutHalfExtent) const
{
    if (!CachedVolume.IsValid())
        CachedVolume = Cast<APTSculptVolume>(UGameplayStatics::GetActorOfClass(GetWorld(), APTSculptVolume::StaticClass()));
    const APTSculptVolume* Vol = CachedVolume.Get();
    if (!Vol) return false;
    FTransform X;
    FVector Ext;
    if (!Vol->GetCanvasBox(X, Ext)) return false;
    OutCenter = X.GetLocation();
    // Tamaño real del cubo a escala 1 (durante el "rebote" de cada turno la escala pasa de 1: se le da margen).
    OutHalfExtent = Ext * FVector(FMath::Max(1.f, (float)X.GetScale3D().GetMax()), FMath::Max(1.f, (float)X.GetScale3D().GetMax()), 1.f);
    return true;
}

FVector APTPartyBotController::PickSeat() const
{
    FVector Center, Half;
    if (!GetCanvas(Center, Half))
    {
        const APawn* P = GetPawn();
        return P ? P->GetActorLocation() : FVector::ZeroVector;
    }

    // Un anillo alrededor del cubo; entre varios ángulos al azar, el más lejos de los demás bots.
    const float Radius = FMath::Max(Half.X, Half.Y) * 1.25f + SeatDistance;
    TArray<FVector> Others;
    for (TActorIterator<APTPartyBotController> It(GetWorld()); It; ++It)
        if (*It != this && It->bHasSeat) Others.Add(It->Seat);

    // Arco "detrás y a los costados" de la escultura vista desde la cámara de la TV: así enmarcan la
    // escultura sin taparla ni pegarse a la cámara. Sin cámara, cualquier ángulo.
    float BaseAng = 0.f, Spread = PI;
    if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
        if (const APawn* Cam = PC->GetPawn())
        {
            const FVector Away = Center - Cam->GetActorLocation();
            BaseAng = FMath::Atan2(Away.Y, Away.X);
            Spread = FMath::DegreesToRadians(80.f);
        }

    FVector Best = Center;
    float BestScore = -1.f;
    for (int32 Try = 0; Try < 10; ++Try)
    {
        const float Ang = BaseAng + FMath::FRandRange(-Spread, Spread);
        const FVector Cand = Center + FVector(FMath::Cos(Ang) * Radius, FMath::Sin(Ang) * Radius,
                                              FMath::FRandRange(SeatHeightMin, SeatHeightMax));
        float Nearest = TNumericLimits<float>::Max();
        for (const FVector& O : Others) Nearest = FMath::Min(Nearest, (float)FVector::Dist(Cand, O));
        if (Nearest > BestScore) { BestScore = Nearest; Best = Cand; }
        if (Nearest >= MinSpacing * 2.f) break; // ya está bien separado
    }
    return Best;
}

void APTPartyBotController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    APawn* P = GetPawn();
    if (!P) return;

    ReseatTimer -= DeltaSeconds;
    if (!bHasSeat || ReseatTimer <= 0.f)
    {
        SetSeat(PickSeat());
        ReseatTimer = FMath::FRandRange(ReseatMin, ReseatMax);
    }

    // Ir al asiento (más despacio al llegar) con un flotado suave.
    BobPhase += DeltaSeconds;
    const FVector Target = Seat + FVector(0.f, 0.f, FMath::Sin(BobPhase * 1.3f) * BobAmplitude);
    const FVector ToTarget = Target - P->GetActorLocation();
    const float Dist = ToTarget.Size();
    if (Dist > 20.f)
        P->AddMovementInput(ToTarget / Dist, FMath::Clamp(Dist / 400.f, 0.15f, 1.f));

    // Mirar la escultura (el personaje usa la rotación del controller en vuelo).
    FVector Center, Half;
    if (GetCanvas(Center, Half))
    {
        const FRotator Look = (Center - P->GetActorLocation()).Rotation();
        SetControlRotation(FMath::RInterpTo(GetControlRotation(), FRotator(0.f, Look.Yaw, 0.f), DeltaSeconds, 3.f));
    }
}
