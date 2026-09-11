// Copyright Epic Games, Inc. All Rights Reserved.
// Escenario de un MAPA custom: un esculpido de voxels (octree) GUARDADO que se carga como
// escenografía SÓLIDA (con colisión) alrededor del canvas central de la partida. A diferencia del
// APTSculptVolume de gameplay, este actor NO se edita en partida ni se borra por turno: se mesha una
// sola vez al cargar (colisión cocinada una vez) y queda estático.
//
// Formato del blob del mapa (mismo contenedor que el snapshot de red: reusable por el transporte
// comprimido+chunked de la Fase 4):
//   [int32 rawSize][ zlib( bytes de FPTVoxelOctree::Serialize ) ]
// La geometría + color viven en el octree serializado; la ambientación viaja aparte (no acá).

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "SVO/PTVoxelOctree.h"
#include "PTMapScenery.generated.h"

class USphereComponent;

// Aviso de que la escenografía terminó de mallearse (para el "loading gate" de la Fase 4).
DECLARE_MULTICAST_DELEGATE_OneParam(FPTOnSceneryReady, class APTMapScenery* /*Scenery*/);

UCLASS()
class MYPARTYGAME_API APTMapScenery : public AActor
{
    GENERATED_BODY()

public:
    APTMapScenery();

    // ── Carga ────────────────────────────────────────────────────────────────
    /** Carga desde el blob del mapa (contenedor [int32 raw][zlib]); descomprime y mesha. */
    bool LoadFromMapBlob(const TArray<uint8>& Blob);
    /** Carga desde los bytes CRUDOS del octree (FPTVoxelOctree::Serialize) y mesha. */
    bool LoadFromOctreeBytes(const TArray<uint8>& RawOctreeBytes);
    /** Lee un archivo .svo de disco (contenedor comprimido) y lo carga. */
    bool LoadFromFile(const FString& AbsPath);

    // Empaqueta/desempaqueta el contenedor del mapa (lo reusa el editor al guardar y la red al enviar).
    static bool EncodeMapBlob(const TArray<uint8>& RawOctreeBytes, TArray<uint8>& OutBlob);
    static bool DecodeMapBlob(const TArray<uint8>& Blob, TArray<uint8>& OutRawOctreeBytes);

    // ── Área jugable (barrera de contención) ─────────────────────────────────
    /** Radio de la barrera invisible que contiene a los pawns (aunque el domo tenga aberturas). */
    void SetPlayableRadius(float InRadius);
    float GetPlayableRadius() const { return PlayableRadius; }

    /** ¿La escenografía ya está mallada y con colisión lista? (para el loading gate). */
    bool IsReady() const { return bReady; }

    // Se dispara en el game thread cuando termina el mallado (la colisión ya quedó cocinada).
    FPTOnSceneryReady OnSceneryReady;

protected:
    virtual void BeginPlay() override;

    // Malla + material de la escenografía. Root del actor.
    UPROPERTY(VisibleAnywhere, Category="Scenery") UProceduralMeshComponent* Mesh = nullptr;
    // Barrera esférica invisible que bloquea SOLO a los pawns → contención del área jugable.
    UPROPERTY(VisibleAnywhere, Category="Scenery") USphereComponent* Barrier = nullptr;

    // Material de la arcilla/escenario. Si es nulo, se usa el color por vértice del mesh.
    UPROPERTY(EditAnywhere, Category="Scenery") UMaterialInterface* SceneryMaterial = nullptr;

    // Radio inicial del área jugable (UU). Default acorde al plan (domo ~8000 → radio ~2800).
    UPROPERTY(EditAnywhere, Category="Scenery") float PlayableRadius = 2800.f;

    // DEV: si no está vacío, se carga este archivo en BeginPlay (iteración rápida sin partida/editor).
    UPROPERTY(EditAnywhere, Category="Scenery|Dev") FString DevMapFilePath;

private:
    // Octree de la escenografía (solo geometría/color; no se edita en partida).
    FPTVoxelOctree SVOField;

    // Remalla TODO el octree en un hilo de fondo y sube la sección con colisión en el game thread.
    void RebuildMesh();

    bool   bReady    = false; // true cuando hay malla+colisión lista
    bool   bMeshing  = false; // hay un mallado async en vuelo
    uint32 MeshGen   = 0;     // descarta resultados async viejos si se recarga
};
