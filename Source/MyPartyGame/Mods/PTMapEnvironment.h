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
class APTSculptVolume;

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

    /** Hornea toda la escultura del box (base + capas + SVO) a un asset y lo agrega a la paleta.
     *  Devuelve el índice del asset, o INDEX_NONE si el box está vacío. Centra la geometría en su bbox. */
    int32 BakeAssetFromVolume(APTSculptVolume* Volume);

    /** Agrega un asset desde geometría ya horneada (para cargar un mapa guardado / replicación). */
    int32 AddAsset(const FPTPropGeometry& Geo);

    int32 GetNumAssets() const { return Assets.Num(); }
    /** Malla de un asset (para el preview que sigue al cursor). */
    UStaticMesh* GetAssetMesh(int32 AssetIdx) const;
    const FPTPropGeometry* GetAssetGeometry(int32 AssetIdx) const;

    /** Coloca una instancia del asset en ese transform (mundo). */
    void PlaceInstance(int32 AssetIdx, const FTransform& WorldXf);
    /** Borra la instancia más cercana a WorldPos dentro de Radius (cualquier asset). true si borró. */
    bool RemoveInstanceNear(const FVector& WorldPos, float Radius);

private:
    struct FPTPropAsset
    {
        FPTPropGeometry Geo;
        UStaticMesh*    Mesh = nullptr;
        UHierarchicalInstancedStaticMeshComponent* HISM = nullptr;
    };
    TArray<FPTPropAsset> Assets;

    UStaticMesh* BuildStaticMesh(const FPTPropGeometry& Geo) const; // bake runtime (MeshDescription)
};
