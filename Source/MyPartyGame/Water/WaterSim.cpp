#include "WaterSim.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "WaterInteractorComponent.h"
#include "TextureResource.h"
#include "Engine/Engine.h"

AWaterSim::AWaterSim()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	DebugPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugPlane"));
	DebugPlane->SetupAttachment(Root);
	DebugPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded()) DebugPlane->SetStaticMesh(PlaneMesh.Object);
}

void AWaterSim::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Escalar el plano debug al tamaño del dominio YA en el editor (el Plane del engine mide 100 uu),
	// así no "salta" de 1x1 a gigante al simular.
	if (DebugPlane) DebugPlane->SetWorldScale3D(FVector(SimWorldSize / 100.f, SimWorldSize / 100.f, 1.f));
}

void AWaterSim::BeginPlay()
{
	Super::BeginPlay();
	InitRTs();

	// Diagnóstico: avisar en pantalla si falta algún material (causa típica de "todo gris / no anda").
	if (GEngine)
	{
		if (!SimMaterial)   GEngine->AddOnScreenDebugMessage(9001, 15.f, FColor::Red,   TEXT("[WaterSim] FALTA Sim Material (M_WaterSim)"));
		if (!BrushMaterial) GEngine->AddOnScreenDebugMessage(9002, 15.f, FColor::Red,   TEXT("[WaterSim] FALTA Brush Material (M_WaterBrush)"));
		if (!DebugMaterial) GEngine->AddOnScreenDebugMessage(9003, 15.f, FColor::Orange,TEXT("[WaterSim] FALTA Debug Material (M_WaterDebug) -> plano gris default"));
	}

	if (SimMaterial)   SimMID   = UMaterialInstanceDynamic::Create(SimMaterial, this);
	if (BrushMaterial) BrushMID = UMaterialInstanceDynamic::Create(BrushMaterial, this);

	// Plano debug: escala (el Plane del engine mide 100x100 uu → escalar a SimWorldSize) y, si hay RT,
	// mostrar la altura. El material lindo (C2) reemplaza esto después.
	if (DebugPlane)
	{
		DebugPlane->SetWorldScale3D(FVector(SimWorldSize / 100.f, SimWorldSize / 100.f, 1.f));
		DebugPlane->SetVisibility(bShowDebugPlane);
		if (bShowDebugPlane && DebugMaterial)
		{
			DebugMID = UMaterialInstanceDynamic::Create(DebugMaterial, this);
			DebugPlane->SetMaterial(0, DebugMID);
		}
	}
}

void AWaterSim::InitRTs()
{
	auto MakeRT = [this]() -> UTextureRenderTarget2D*
	{
		UTextureRenderTarget2D* RT = UKismetRenderingLibrary::CreateRenderTarget2D(
			this, SimResolution, SimResolution, RTF_RGBA16f);
		if (RT) UKismetRenderingLibrary::ClearRenderTarget2D(this, RT, FLinearColor::Black);
		return RT;
	};
	RTPrev  = MakeRT();
	RTCurr  = MakeRT();
	RTNext  = MakeRT();
	RTForce = MakeRT();
}

void AWaterSim::AddDisturbance(const FVector& WorldPos, float Strength, float Radius)
{
	if (SimWorldSize <= 0.f) return;
	const FVector2D UV = WorldToUV(WorldPos);
	// Fuera del dominio → ignorar.
	if (UV.X < -0.05f || UV.X > 1.05f || UV.Y < -0.05f || UV.Y > 1.05f) return;
	Pending.Add({ UV, Strength, Radius / SimWorldSize });
}

FVector2D AWaterSim::WorldToUV(const FVector& WorldPos) const
{
	const FVector Local = WorldPos - GetActorLocation();
	// Centro del dominio = actor; U = X, V = Y, mapeados a [0..1].
	const float Half = SimWorldSize * 0.5f;
	return FVector2D((Local.X + Half) / SimWorldSize, (Local.Y + Half) / SimWorldSize);
}

void AWaterSim::SimStep()
{
	if (!SimMID || !RTPrev || !RTCurr || !RTNext || !RTForce) return;

	// 1) Pintar las perturbaciones del frame en RTForce (aditivo). Limpiar primero.
	UKismetRenderingLibrary::ClearRenderTarget2D(this, RTForce, FLinearColor::Black);
	if (BrushMID)
	{
		for (const FPendingDisturb& D : Pending)
		{
			BrushMID->SetVectorParameterValue(TEXT("CenterUV"), FLinearColor(D.UV.X, D.UV.Y, 0.f, 0.f));
			BrushMID->SetScalarParameterValue(TEXT("RadiusUV"), FMath::Max(0.001f, D.RadiusUV));
			BrushMID->SetScalarParameterValue(TEXT("Strength"), D.Strength);
			UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, RTForce, BrushMID);
		}
	}
	Pending.Reset();

	// 2) Paso de onda: Next = f(Prev, Curr, Force). El material hace la ecuación de onda + damping.
	SimMID->SetTextureParameterValue(TEXT("TexPrev"),  RTPrev);
	SimMID->SetTextureParameterValue(TEXT("TexCurr"),  RTCurr);
	SimMID->SetTextureParameterValue(TEXT("TexForce"), RTForce);
	SimMID->SetScalarParameterValue(TEXT("WaveC2"),   WaveC2);
	SimMID->SetScalarParameterValue(TEXT("Damping"),  Damping);
	SimMID->SetScalarParameterValue(TEXT("TexelSize"), 1.f / FMath::Max(1, SimResolution));
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, RTNext, SimMID);

	// 3) Rotar: Prev <- Curr <- Next (reusamos el buffer viejo de Prev como el próximo Next).
	UTextureRenderTarget2D* OldPrev = RTPrev;
	RTPrev = RTCurr;
	RTCurr = RTNext;
	RTNext = OldPrev;
}

void AWaterSim::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Debug: gotas al azar para ver la sim propagarse/rebotar (sin piedra). Dev-only.
	if (bDebugAutoRipple)
	{
		DebugRippleAccum += DeltaSeconds;
		if (DebugRippleAccum >= DebugRippleInterval)
		{
			DebugRippleAccum = 0.f;
			const float Half = SimWorldSize * 0.45f;
			const FVector P = GetActorLocation() + FVector(FMath::FRandRange(-Half, Half), FMath::FRandRange(-Half, Half), 0.f);
			AddDisturbance(P, DebugRippleStrength, DebugRippleRadius);
		}
	}

	ProcessInteractors(DeltaSeconds); // objetos elegidos → ondas (antes del paso de sim)

	if (bDebugConstantDrip) // aislar brush vs sim: fuerza constante en el centro cada frame
		AddDisturbance(GetActorLocation(), DebugRippleStrength, DebugRippleRadius);

	for (int32 i = 0; i < FMath::Max(1, StepsPerFrame); ++i)
		SimStep();

	// Debug: mostrar la altura ACTUAL en el plano (el RT rota cada step, así que lo re-seteamos cada frame).
	if (DebugMID && RTCurr)
	{
		DebugMID->SetTextureParameterValue(TEXT("HeightTex"), RTCurr);
		DebugMID->SetScalarParameterValue(TEXT("HeightScale"), DebugHeightScale);
	}

	// Diagnóstico en pantalla: leer el RT de altura (min/max) para saber si la SIM produce ondas
	// (independiente de si el material debug las muestra). Throttled (~0.3s, readback es costoso).
	DebugReadAccum += DeltaSeconds;
	if (GEngine && RTCurr && DebugReadAccum >= 0.3f)
	{
		DebugReadAccum = 0.f;
		if (FTextureRenderTargetResource* Res = RTCurr->GameThread_GetRenderTargetResource())
		{
			TArray<FLinearColor> Px;
			if (Res->ReadLinearColorPixels(Px) && Px.Num() > 0)
			{
				float MinH = TNumericLimits<float>::Max(), MaxH = -TNumericLimits<float>::Max();
				for (const FLinearColor& C : Px) { MinH = FMath::Min(MinH, C.R); MaxH = FMath::Max(MaxH, C.R); }
				GEngine->AddOnScreenDebugMessage(9010, 1.f, FColor::Cyan,
					FString::Printf(TEXT("[WaterSim] altura min=%.3f max=%.3f  (si min=max=0 -> la sim NO produce ondas)"), MinH, MaxH));

				// Leer tambien el RT de FUERZA: aisla brush (fuerza) vs sim (altura).
				if (RTForce)
				{
					if (FTextureRenderTargetResource* FRes = RTForce->GameThread_GetRenderTargetResource())
					{
						TArray<FLinearColor> FP;
						if (FRes->ReadLinearColorPixels(FP) && FP.Num() > 0)
						{
							float Fmin = TNumericLimits<float>::Max(), Fmax = -TNumericLimits<float>::Max();
							for (const FLinearColor& C : FP) { Fmin = FMath::Min(Fmin, C.R); Fmax = FMath::Max(Fmax, C.R); }
							GEngine->AddOnScreenDebugMessage(9012, 1.f, FColor::Yellow,
								FString::Printf(TEXT("[WaterSim] FUERZA(brush) min=%.3f max=%.3f  (si 0 -> falla M_WaterBrush)"), Fmin, Fmax));
						}
					}
				}
				GEngine->AddOnScreenDebugMessage(9011, 1.f, FColor::White,
					FString::Printf(TEXT("[WaterSim] interactores=%d  pending=%d  autoRipple=%d"),
						Interactors.Num(), Pending.Num(), bDebugAutoRipple ? 1 : 0));
			}
		}
	}
}

void AWaterSim::RegisterInteractor(UWaterInteractorComponent* C)
{
	if (C) Interactors.AddUnique(C);
}
void AWaterSim::UnregisterInteractor(UWaterInteractorComponent* C)
{
	Interactors.Remove(C);
	LastInteractorPos.Remove(C);
}

void AWaterSim::ProcessInteractors(float Dt)
{
	if (Dt <= 0.f) return;
	const float WaterZ = GetActorLocation().Z;

	for (int32 i = Interactors.Num() - 1; i >= 0; --i)
	{
		UWaterInteractorComponent* C = Interactors[i].Get();
		if (!C) { Interactors.RemoveAt(i); continue; }

		const FVector Loc = C->GetComponentLocation();
		const FVector2D UV = WorldToUV(Loc);
		const bool bInside = (UV.X >= 0.f && UV.X <= 1.f && UV.Y >= 0.f && UV.Y <= 1.f);
		const bool bNearSurface = FMath::Abs(Loc.Z - WaterZ) <= C->SurfaceTolerance;

		// Velocidad desde el frame anterior.
		FVector Vel = FVector::ZeroVector;
		if (const FVector* Last = LastInteractorPos.Find(C)) Vel = (Loc - *Last) / Dt;
		LastInteractorPos.Add(C, Loc);

		if (!bInside || !bNearSurface) continue;

		const float Planar = Vel.Size2D();
		const float Vert   = FMath::Abs(Vel.Z);
		const float Speed  = FMath::Max(Planar, Vert);
		if (Speed < C->MinSpeed) continue;

		const float Norm = FMath::Clamp(Speed / FMath::Max(1.f, C->RefSpeed), 0.15f, 2.5f);
		// Empuja el agua hacia ABAJO donde el objeto la "pisa" → nace una onda que se propaga y rebota.
		AddDisturbance(Loc, -C->Strength * Norm, C->Radius);
	}
}

float AWaterSim::GetHeightAtWorld(const FVector& WorldPos) const
{
	// TODO (C3 — flotar la piedra sobre la ola): leer la altura del RT en UV por readback de 1 texel.
	// Se implementa cuando conectemos la flotabilidad a la superficie simulada. Por ahora, superficie
	// en reposo (0) para no pagar un readback de GPU cada frame sin usarlo todavía.
	return 0.f;
}
