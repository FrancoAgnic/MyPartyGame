// Copyright Epic Games, Inc. All Rights Reserved.
// Entorno de un mapa creado in-game: una PALETA de assets (props horneados de la escultura del box) y
// las INSTANCIAS colocadas alrededor. Cada asset único = un UStaticMesh (horneado en runtime) dibujado
// con un HierarchicalInstancedStaticMeshComponent (1 draw call para todas sus copias → liviano/rápido).
//
// Autoría: el jugador esculpe en el box, hornea (Enter 3s) → AddAsset; fuera del box coloca instancias
// (PlaceInstance) y borra (RemoveInstanceNear). Guardar/cargar y jugar reusan el mismo actor.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PTMapEnvironment.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UTextureRenderTarget2D;
class APTSculptVolume;

// Ajustes de ambiente del mapa (momento del día + colores del cielo + niebla/ambiente). Se editan en el
// editor de mapas, se guardan con el mapa y se aplican en todas las máquinas al cargarlo.
USTRUCT(BlueprintType)
struct FPTSkySettings
{
    GENERATED_BODY()

    // Momento del día: 0=amanecer (sol en el horizonte este), 0.5=mediodía (sol arriba), 1=atardecer.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky") float TimeOfDay = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky") float SunYaw    = 0.f; // orientación del sol

    // Colores del cielo (para el material del Sky Sphere: parámetros "SkyTop" / "SkyHorizon").
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky") FLinearColor SkyTopColor     = FLinearColor(0.10f, 0.35f, 0.85f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky") FLinearColor SkyHorizonColor = FLinearColor(0.70f, 0.85f, 1.00f);

    // Sol (DirectionalLight).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky") FLinearColor SunColor     = FLinearColor(1.0f, 0.96f, 0.85f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky") float        SunIntensity = 3.0f;

    // Niebla (ExponentialHeightFog) + luz ambiental (SkyLight).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky") FLinearColor FogColor     = FLinearColor(0.60f, 0.75f, 0.90f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky") float        FogDensity   = 0.02f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky") FLinearColor AmbientColor = FLinearColor(0.50f, 0.60f, 0.75f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky") float        AmbientIntensity = 1.0f;

    // Knobs "cartoon" del material del cielo (parámetros escalares del Sky Sphere).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky") float Bands      = 4.0f;   // franjas planas (0 = degradé suave)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky") float HorizonExp = 0.6f;   // ancho del horizonte
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky") float SunSize    = 0.995f; // tamaño del disco del sol
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky") float SunGlow    = 60.0f;  // halo del sol
};

// Geometría horneada de un prop (malla compacta que se guarda una sola vez por asset único).
struct FPTPropGeometry
{
    TArray<FVector3f> Verts;
    TArray<FVector3f> Normals;
    TArray<FColor>    Colors;
    TArray<int32>     Tris;
    bool IsValid() const { return Verts.Num() > 0 && Tris.Num() >= 3; }
};

UCLASS()
class MYPARTYGAME_API APTMapEnvironment : public AActor
{
    GENERATED_BODY()
public:
    APTMapEnvironment();

    /** Material de los props (vertex color tipo arcilla). Asignar en BP; si es null, usa el default. */
    UPROPERTY(EditAnywhere, Category="MapEnv") UMaterialInterface* PropMaterial = nullptr;

    // ── Ambiente del mapa (sol / cielo / niebla) ──
    /** Ajustes de ambiente actuales (se guardan/cargan con el mapa). */
    UPROPERTY(EditAnywhere, Category="MapEnv") FPTSkySettings SkySettings;
    /** Setea nuevos ajustes y los aplica en vivo (lo llama el panel del editor de mapas). */
    UFUNCTION(BlueprintCallable, Category="MapEnv") void SetSkySettings(const FPTSkySettings& In);
    const FPTSkySettings& GetSkySettings() const { return SkySettings; }
    /** Aplica SkySettings al mundo: sol (DirectionalLight), niebla (ExponentialHeightFog), luz ambiental
     *  (SkyLight) y el material del Sky Sphere (actor con tag "MapSky"). */
    UFUNCTION(BlueprintCallable, Category="MapEnv") void ApplySkySettings();

    /** Hornea toda la escultura del box (base + capas + SVO) a un asset y lo agrega a la paleta.
     *  Devuelve el índice del asset, o INDEX_NONE si el box está vacío. Por defecto centra la geometría en
     *  su bbox; si bUsePivot, usa PivotWorld como ORIGEN del asset (el jugador lo ubica en el editor). */
    int32 BakeAssetFromVolume(APTSculptVolume* Volume, bool bUsePivot = false, const FVector& PivotWorld = FVector::ZeroVector);

    /** Agrega un asset desde geometría ya horneada (para cargar un mapa guardado / replicación). */
    int32 AddAsset(const FPTPropGeometry& Geo);

    int32 GetNumAssets() const { return Assets.Num(); }
    /** Malla de un asset (para el preview que sigue al cursor). */
    UStaticMesh* GetAssetMesh(int32 AssetIdx) const;
    const FPTPropGeometry* GetAssetGeometry(int32 AssetIdx) const;

    /** Miniatura del asset (render de la malla a un RenderTarget) para el radial de assets. Se genera
     *  la primera vez y se cachea. Devuelve null si el índice no existe. */
    UTextureRenderTarget2D* GetAssetThumbnail(int32 AssetIdx, int32 Size = 128);

    /** Coloca una instancia del asset en ese transform (mundo). */
    void PlaceInstance(int32 AssetIdx, const FTransform& WorldXf);
    /** Borra la instancia más cercana a WorldPos dentro de Radius (cualquier asset). true si borró. */
    bool RemoveInstanceNear(const FVector& WorldPos, float Radius);
    /** Deshace la última instancia colocada (undo de props). true si sacó algo. */
    bool RemoveLastInstance();
    /** Instancia más cercana a WorldPos dentro de Radius (para el preview de "qué se va a borrar").
     *  Devuelve su asset + transform. false si no hay ninguna cerca. */
    bool GetNearestInstance(const FVector& WorldPos, float Radius, int32& OutAsset, FTransform& OutXf) const;

    /** Serializa el mapa (geometría de cada asset único + transforms de todas las instancias) a un blob.
     *  Es el contenido del mapa que se guarda/publica/replica. */
    void SerializeEnvironment(TArray<uint8>& Out);
    /** Reconstruye el entorno desde el blob (reconstruye StaticMesh + HISM y recoloca las instancias). */
    void DeserializeEnvironment(const TArray<uint8>& In);
    /** Vacía todo (assets + instancias). */
    void ClearAll();

private:
    struct FPTPropAsset
    {
        FPTPropGeometry Geo;
        UStaticMesh*    Mesh = nullptr;
        UHierarchicalInstancedStaticMeshComponent* HISM = nullptr;
    };
    TArray<FPTPropAsset> Assets;
    // Orden global de colocación (índice de asset por cada instancia colocada) → para el undo LIFO de props.
    TArray<int32> PlaceOrder;
    // Miniaturas cacheadas (alineadas con Assets); UPROPERTY para que no las junte el GC.
    UPROPERTY(Transient) TArray<UTextureRenderTarget2D*> Thumbnails;

    UStaticMesh* BuildStaticMesh(const FPTPropGeometry& Geo) const; // bake runtime (MeshDescription)
};
