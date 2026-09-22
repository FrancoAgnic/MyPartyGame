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
#include "../Sculpt/PTSculptVolume.h" // FPTPaintAtlas (snapshot del atlas de pintura por asset)
#include "PTMapEnvironment.generated.h"

class UProceduralMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
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
    // Coordenada de LOOKUP del atlas de pintura por vértice (posición VOLUMEN-LOCAL, empaquetada:
    // UV0 = (x, y), UV1 = (z, 0)). El material del asset pintado samplea el atlas con esto → pintura nítida,
    // independiente del transform de la instancia (así el merge sigue valiendo). Vacío = sin atlas.
    TArray<FVector2f> UV0;
    TArray<FVector2f> UV1;
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

    /** Agrega un asset desde geometría ya horneada (para cargar un mapa guardado / replicación). EyesGeo =
     *  malla de ojos (sección aparte); PaintAtlas = snapshot del atlas de pintura (vacío = sin pintura). */
    int32 AddAsset(const FPTPropGeometry& Geo, const FPTPropGeometry& EyesGeo = FPTPropGeometry(),
                   const FPTPaintAtlas& PaintAtlas = FPTPaintAtlas());

    /** Material de los OJOS horneados (asignar M_CreazyEyes en BP). Los ojos van en su propia sección. */
    UPROPERTY(EditAnywhere, Category="MapEnv") UMaterialInterface* EyeMaterial = nullptr;
    UMaterialInterface* GetEyeMaterial() const { return EyeMaterial; }

    /** Material de arcilla que SAMPLEA el atlas de pintura por UV (duplicado de M_Clay con el sampler leyendo
     *  UV0/UV1). Asignar en BP. Se usa en la sección 0 de los assets CON pintura (nitidez pixel-perfect). */
    UPROPERTY(EditAnywhere, Category="MapEnv") UMaterialInterface* PaintClayMaterial = nullptr;
    /** Material a usar para el preview/miniatura del asset actual: el de pintura (con atlas) si tiene, o el normal. */
    UMaterialInterface* GetAssetPreviewMaterial(int32 AssetIdx);

    int32 GetNumAssets() const { return Assets.Num(); }
    /** Geometría horneada de un asset (para el preview que sigue al cursor). */
    const FPTPropGeometry* GetAssetGeometry(int32 AssetIdx) const;
    /** Geometría de OJOS del asset (para el preview con su material). Null/inválida si no tiene ojos. */
    const FPTPropGeometry* GetAssetEyesGeometry(int32 AssetIdx) const;
    /** Radio aproximado del asset (bbox), para alejar el preview al escalar. 0 si no existe. */
    float GetAssetRadius(int32 AssetIdx) const;
    /** Material de los props (con el MID del ambiente ya aplicado) para el preview del PlayerController. */
    UMaterialInterface* GetPropMaterialForPreview();
    /** Rellena una sección de un ProceduralMesh con la geometría del asset transformada por Xf (vertex
     *  colors incluidos → se ven en build). Lo usa el entorno (instancias) y el preview del PC. */
    static void FillProcSection(UProceduralMeshComponent* PMC, int32 Section, const FPTPropGeometry& Geo,
                                const FTransform& Xf, bool bCollision);

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
        // Los props se dibujan con un ProceduralMeshComponent (NO StaticMesh): un StaticMesh construido en
        // runtime NO renderiza sus vertex colors en build cocinada, el ProcMesh sí. Cada instancia colocada
        // es una SECCIÓN del PMC con la geometría transformada a mundo.
        // Todas las instancias de este asset se FUSIONAN en la sección 0 de un único PMC → ~1 draw call
        // por asset (como el HISM), y con vertex colors que sí se ven en build. Se reconstruye al
        // colocar/borrar (solo en autoría, nunca por frame).
        UProceduralMeshComponent* PMC = nullptr;
        FPTPropGeometry EyesGeo;        // malla de ojos (sección 1 del PMC, material de ojos); vacía = sin ojos
        TArray<FTransform> InstXf;      // transform (mundo) por instancia viva
        // Pintura NÍTIDA: snapshot del atlas del SVO (si el asset tiene Paint). El material del asset lo
        // samplea con la UV horneada → idéntico a lo pintado. Vacío = sin pintura (usa solo vertex color).
        FPTPaintAtlas PaintAtlas;
        UMaterialInstanceDynamic* PaintMID = nullptr; // MID con el atlas (sección 0); rooteado en AssetMIDs
    };
    TArray<FPTPropAsset> Assets;
    // Orden global de colocación (índice de asset por cada instancia colocada) → para el undo LIFO de props.
    TArray<int32> PlaceOrder;
    // Miniaturas cacheadas (alineadas con Assets); UPROPERTY para que no las junte el GC.
    UPROPERTY(Transient) TArray<UTextureRenderTarget2D*> Thumbnails;

    // MID del material de props con el sol del ambiente inyectado (se comparte en todas las secciones/PMCs).
    UPROPERTY(Transient) UMaterialInstanceDynamic* PropMID = nullptr;
    // Mantiene vivos (anti-GC) los MID de pintura por asset (uno por asset con Paint) + sus texturas de atlas.
    UPROPERTY(Transient) TArray<UMaterialInstanceDynamic*> AssetMIDs;
    UMaterialInstanceDynamic* GetOrCreatePropMID();
    // Recrea las texturas del atlas desde un snapshot y arma un MID (PaintClayMaterial) para la pintura nítida.
    UMaterialInstanceDynamic* MakePaintMID(const FPTPaintAtlas& Atlas);
    void RebuildAssetMesh(FPTPropAsset& A); // fusiona todas las instancias en la sección 0 (arcilla) + 1 (ojos)
    // Fusiona la geometría G de TODAS las instancias de A en una sección del PMC con el material dado.
    void BuildMergedSection(FPTPropAsset& A, const FPTPropGeometry& G, int32 Section, UMaterialInterface* Mat);
    // Inyecta la dirección/color del sol del ambiente en el MID compartido. Se llama al hornear/cargar y
    // desde ApplySkySettings.
    void ApplyAssetSunParams();
};
