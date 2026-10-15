#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInterface.h"
#include "ClaySculptField.h"
#include "ClaySculptVolume.generated.h"

// Formas de sello (SDF centrado en origen).
UENUM(BlueprintType)
enum class EClayStampShape : uint8 { Sphere, Cube, Cylinder, Cone };

// Modo del sello. Add usa PaintColor como color de la geometría nueva (vertex color).
UENUM(BlueprintType)
enum class EClayEditMode : uint8 { Add, Erase, Smooth };

/**
 * Volumen de esculpido tipo plastilina (SDF + Surface Nets + vertex color).
 * Reusa el core FClaySculptField. Sin networking ni atlas de pintura: es la base
 * para la herramienta de editor que modela escenario y hornea a Static Mesh.
 */
UCLASS()
class CLAYSCULPT_API AClaySculptVolume : public AActor
{
	GENERATED_BODY()
public:
	AClaySculptVolume();

	// Resolución del campo (tamaño de celda en UU). Menor = más geometría/detalle.
	UPROPERTY(EditAnywhere, Category="Clay Sculpt") float VoxelSize = 5.f;
	// Material del clay (usa Vertex Color; leerlo como LINEAL). Si es null, usa el default engine.
	UPROPERTY(EditAnywhere, Category="Clay Sculpt") UMaterialInterface* ClayMaterial = nullptr;

	// Suavizado visual de la malla al generarla (0 = off). No borra detalle guardado.
	UPROPERTY(EditAnywhere, Category="Clay Sculpt", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DisplaySmoothing = 0.5f;

	// ── Modo Smooth (tuneables) ──
	UPROPERTY(EditAnywhere, Category="Clay Sculpt|Smooth", meta=(ClampMin="0.0", ClampMax="1.0")) float SmoothStrength = 0.35f;
	UPROPERTY(EditAnywhere, Category="Clay Sculpt|Smooth", meta=(ClampMin="-0.2", ClampMax="0.2")) float SmoothBias     = 0.f;
	UPROPERTY(EditAnywhere, Category="Clay Sculpt|Smooth", meta=(ClampMin="0.0", ClampMax="1.0")) float SmoothMaxDelta  = 0.15f;

	/** Aplica un sello en posición de mundo (lo llama la herramienta de editor o gameplay). */
	void ApplyStamp(FVector WorldPos, EClayStampShape Shape, float Size, EClayEditMode Mode, FLinearColor PaintColor);

	/** Vacía toda la escultura (geometría + color). */
	void ClearAll();

	/** Reconstruye ya mismo los bricks sucios (para el editor, tras un sello). */
	void RebuildNow();

	/** Densidad SDF muestreada en una posición de mundo (para raycasts del editor). */
	float SampleWorldDensity(FVector WorldPos) const;

	/** SDF de cada forma, centrado en origen. HalfSize en unidades de celda. */
	static float StampSDF(EClayStampShape Shape, FVector LocalPos, float HalfSize);

	UProceduralMeshComponent* GetMesh() const { return Mesh; }

#if WITH_EDITORONLY_DATA
	/** Carpeta del Content Browser donde se guardan los Static Mesh horneados. */
	UPROPERTY(EditAnywhere, Category="Clay Sculpt|Bake") FString BakeFolder = TEXT("/Game/ClaySculpt");
	/** Si está activo, al hornear se spawnea un StaticMeshActor con la malla y se oculta este volumen. */
	UPROPERTY(EditAnywhere, Category="Clay Sculpt|Bake") bool bSpawnActorOnBake = true;
#endif

#if WITH_EDITOR
	/** Hornea la escultura actual a un Static Mesh (asset en BakeFolder) conservando el color de vértice, y
	 *  (opcional) reemplaza esta escultura por un StaticMeshActor con esa malla. Botón en el panel Details. */
	UFUNCTION(CallInEditor, Category="Clay Sculpt|Bake")
	void BakeToStaticMesh();
#endif

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(VisibleAnywhere) UProceduralMeshComponent* Mesh;
	UPROPERTY(VisibleAnywhere) UBoxComponent*            BoundsBox;

	FClaySculptField Field;
	bool  bFieldInit       = false;
	float TimeSinceRebuild = 0.f;
	static constexpr float RebuildInterval = 0.05f;

	void EnsureFieldInit();
	void RebuildDirty();
	void MarkStampDirty(int32 x0, int32 y0, int32 z0, int32 x1, int32 y1, int32 z1);
	FVector WorldToCell(FVector W) const;
	void    CellBounds(FIntVector& OutMin, FIntVector& OutMax) const;
};
