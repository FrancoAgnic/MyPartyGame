// Copyright Epic Games, Inc. All Rights Reserved.
// Sparse Voxel Octree para esculpido ADAPTATIVO (tipo SculptrVR): la resolución depende del tamaño
// de la edición → brocha grande escribe en nodos grandes (pocos vértices → pocos tris), brocha chica
// subdivide a nodos finos (detalle alto). Solo se allocan nodos que se tocan (esparso).
//
// Mallado: DUAL CONTOURING (variante Surface Nets). Cada HOJA con cambio de signo aporta UN vértice
// (promedio de los cruces cero de sus 12 aristas). La conectividad se arma recorriendo las "aristas
// mínimas" del árbol: por cada arista con cambio de signo se conecta el vértice de las (hasta 4) hojas
// que la rodean. Como los vértices se COMPARTEN entre hojas de distinto nivel, la malla es
// CRACK-FREE por construcción: no hay costuras aunque una hoja grande toque una chica.
//
// Convención SDF: POSITIVO = dentro del material, 0 = superficie (igual que FPTSculptField).
// Es un struct plano C++ (sin UObject) para poder mesharlo en el ThreadPool más adelante.

#pragma once
#include "CoreMinimal.h"

// Formas analíticas del sello para el SVO (todas soportan escala no-uniforme + rotación).
enum class EPTSVOShape : uint8 { Sphere, Box, Cylinder, Torus, Cone };

// Un nodo del octree. HOJA = una sola celda cúbica con 8 valores SDF en las esquinas.
// INTERNO = 8 hijos (octantes) y sin esquinas propias. Nunca ambos.
struct FPTOctreeNode
{
    TUniquePtr<FPTOctreeNode> Children[8]; // hijos (octantes) si es interno
    float  Corner[8];                       // SDF en las 8 esquinas si es hoja
    FColor Col[8];                          // color de arcilla en las 8 esquinas
    bool   bLeaf = true;

    FPTOctreeNode()
    {
        for (int32 i = 0; i < 8; ++i) { Corner[i] = -1.f; Col[i] = FColor::White; } // aire, blanco
    }

    bool IsLeaf() const { return bLeaf; }

    // Esquina c: x=bit0, y=bit1, z=bit2. Posición = NodeMin + (x,y,z)*NodeSize.
    static FORCEINLINE FVector CornerOffset(int32 c)
    {
        return FVector((c & 1) ? 1.f : 0.f, (c & 2) ? 1.f : 0.f, (c & 4) ? 1.f : 0.f);
    }
};

class FPTVoxelOctree
{
public:
    // Origin = esquina mínima del cubo raíz; RootSize = lado del cubo (UU local); MaxDepth = profundidad
    // máxima (celda mínima = RootSize / 2^MaxDepth). Hojas de UNA celda: el detalle lo da la profundidad.
    void Init(const FVector& InOrigin, float InRootSize, int32 InMaxDepth);
    void Reset();
    bool IsInit() const { return Root.IsValid(); }

    // Caja de recorte (LOCAL): las esquinas en el límite o fuera se fuerzan a AIRE al editar, así la
    // arcilla SIEMPRE cierra contra la pared (como el campo acotado del modo clásico). Sin caja = sin recorte.
    void SetClampBox(const FBox& InBox) { ClampBox = InBox; bClamp = true; }

    // SDF interpolado (trilineal) en una posición LOCAL. >0 dentro, <0 fuera. Fuera de lo allocado = aire.
    float Sample(const FVector& LocalPos) const;

    // Edita una esfera (CSG). bAdd=true une (agrega arcilla), false resta (borra). La resolución se elige
    // según el RADIO: radio grande → nodos grandes (pocos tris), radio chico → subdivide fino (detalle).
    // PaintColor = color de la arcilla agregada (ignorado al borrar).
    void EditSphere(const FVector& Center, float Radius, bool bAdd, const FColor& PaintColor = FColor::White);

    // Sello genérico: shape analítica con escala NO-UNIFORME (HalfExtent por eje) y ROTACIÓN (Xf).
    // Xf aporta posición + rotación (su escala se ignora; el tamaño lo da HalfExtent en UU local).
    void EditShape(const FTransform& Xf, EPTSVOShape Shape, const FVector& HalfExtent,
                   bool bAdd, const FColor& PaintColor = FColor::White);

    // PINTAR: recolorea las esquinas dentro de la shape SIN tocar el SDF ni subdividir (pinta sobre la
    // superficie existente, a la resolución que ya tenga). Devuelve true si tocó alguna esquina de superficie.
    bool PaintShape(const FTransform& Xf, EPTSVOShape Shape, const FVector& HalfExtent, const FColor& Color);

    // Balancea el árbol a 2:1 (hojas vecinas difieren máx. 1 nivel). Reduce muchísimo los artefactos
    // en transiciones con saltos grandes de nivel (brocha chica y después grande encima). Llamar
    // antes de BuildMesh.
    // Changed leaves include refinements outside the brush: callers must invalidate their meshes too.
    // bForceFull is the reference path used by regression/performance tests.
    void Balance(TArray<FBox>* OutRefinedBounds = nullptr, bool bForceFull = false);
    int32 GetLastBalanceLeafChecks() const { return LastBalanceLeafChecks; }
    const FBox& GetPendingBalanceBounds() const { return PendingBalanceBounds; }

    // Mallado por Dual Contouring (crack-free entre niveles). Verts en espacio LOCAL.
    void BuildMesh(TArray<FVector>& OutVerts, TArray<int32>& OutTris, TArray<FVector>& OutNormals,
                   TArray<FColor>& OutColors) const;

    // Mallado FILTRADO para re-mallado incremental por chunks: emite solo los triángulos cuya hoja
    // DUEÑA cae en OwnerRegion (por su esquina Min, medio-abierto) y tiene tamaño en [MinLeafSize,
    // MaxLeafSize). Malla autocontenida (vértices propios). Reusa consultas globales de vecinos.
    void BuildMeshFiltered(const FBox& OwnerRegion, float MinLeafSize, float MaxLeafSize,
                           TArray<FVector>& OutVerts, TArray<int32>& OutTris,
                           TArray<FVector>& OutNormals, TArray<FColor>& OutColors) const;

    // Clona SOLO los nodos que intersectan Region (más un poco por el árbol de acceso) en un octree
    // aparte, para mallar esa región en un HILO DE FONDO sin tocar el octree vivo (sin data races).
    // Barato: cuesta ~ tamaño de la región editada, no del modelo.
    TSharedPtr<FPTVoxelOctree> CloneRegion(const FBox& Region) const;

    // Mallado por DUAL CONTOURING RECURSIVO (Ju et al.): WATERTIGHT por construcción — cada arista mínima
    // se procesa exactamente una vez, sin importar el salto de resolución → SIN grietas nunca. Malla plana
    // (el llamador reparte por chunks). Pensado para correr sobre un CloneRegion en un hilo de fondo.
    void BuildMeshDC(TArray<FVector>& OutVerts, TArray<int32>& OutTris,
                     TArray<FVector>& OutNormals, TArray<FColor>& OutColors) const;

    // Cierra TODOS los bordes abiertos de una malla (en una malla única, todo borde abierto = hueco real).
    // Enlaza los bordes en bucles y los rellena (fan). Garantiza malla topológicamente cerrada.
    static void FillAllHoles(const TArray<FVector>& V, TArray<int32>& T);

    // Mallado por MARCHING CUBES sobre una grilla UNIFORME muestreada del octree (reusa el MC probado del
    // proyecto). Uniforme = sin T-junctions = WATERTIGHT garantizado (sin grietas, en ningún caso). Pierde
    // la ventaja adaptativa de "grande = pocos triángulos", pero cierra siempre. Verts en espacio LOCAL.
    void BuildMeshMC(TArray<FVector>& OutVerts, TArray<int32>& OutTris,
                     TArray<FVector>& OutNormals, TArray<FColor>& OutColors) const;

    // ── Baking / persistencia (serialización del octree a bytes) ─────────────
    // Snapshot compacto del árbol para guardar (SaveGame), bakear escenografía o mandar por red.
    void Serialize(TArray<uint8>& OutBytes) const;
    bool LoadFromBytes(const TArray<uint8>& InBytes);

    // ── Undo por trazo (snapshots) ──────────────────────────────────────────
    // Llamá PushUndoSnapshot() ANTES de cada trazo; Undo() vuelve al estado previo.
    void PushUndoSnapshot();
    bool Undo();
    int32 UndoDepth() const { return UndoStack.Num(); }
    int32 MaxUndo = 20;

    // ── Métricas / debug ────────────────────────────────────────────────────
    int32 CountLeaves() const;   // hojas allocadas (≈ cuánto detalle hay)
    int32 CountNodes()  const;   // nodos totales

    FVector GetOrigin()   const { return Origin; }
    float   GetRootSize() const { return RootSize; }
    int32   GetMaxDepth() const { return MaxDepth; }
    float   MinCellSize() const { return RootSize / (float)(1 << MaxDepth); }

private:
    FVector Origin = FVector::ZeroVector;
    float   RootSize = 1024.f;
    int32   MaxDepth = 8;
    TUniquePtr<FPTOctreeNode> Root;
    FBox    ClampBox = FBox(ForceInit); // caja de recorte (local)
    bool    bClamp   = false;
    TArray<TUniquePtr<FPTOctreeNode>> UndoStack; // snapshots (clones) para deshacer
    FBox PendingBalanceBounds = FBox(ForceInit);
    int32 LastBalanceLeafChecks = 0;

    static TUniquePtr<FPTOctreeNode> CloneNode(const FPTOctreeNode* N);
    static TUniquePtr<FPTOctreeNode> CloneNodeRegion(const FPTOctreeNode* N, const FVector& NodeMin, float NodeSize, const FBox& Region);
    static void SerializeNode(FArchive& Ar, FPTOctreeNode& N);
    static TUniquePtr<FPTOctreeNode> DeserializeNode(FArchive& Ar);

    // Profundidad objetivo para un radio dado (adaptativo). Celda ≈ Radius/CellsPerRadius.
    int32 DepthForRadius(float Radius) const;

    // SDF genérico (POSITIVO = dentro, en UU). Descenso recursivo de edición.
    using FSDFFunc = TFunctionRef<float(const FVector&)>;
    void EditField(const FBox& WorldBounds, int32 TargetDepth, bool bAdd, const FColor& PaintColor, FSDFFunc SDF);
    void EditFieldNode(FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize, int32 Depth,
                       const FBox& WorldBounds, int32 TargetDepth, bool bAdd, const FColor& PaintColor, FSDFFunc SDF);
    void WriteCorners(FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize,
                      bool bAdd, const FColor& PaintColor, FSDFFunc SDF) const;
    // Recolorea sin refinar ni tocar SDF. Devuelve true si pintó alguna esquina de superficie.
    bool PaintNode(FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize,
                   const FBox& WorldBounds, const FColor& Color, FSDFFunc SDF) const;

    // Refina una HOJA en 8 hijos-hoja, resampleando (trilineal) sus esquinas a cada hijo.
    static void RefineLeaf(FPTOctreeNode& Node);
    static float TrilinearCorners(const float C[8], float fx, float fy, float fz);
    // Colapsa un nodo interno a HOJA downsampleando (la brocha grande borra el detalle fino que tapa).
    static void CollapseToLeaf(FPTOctreeNode& Node);
    // Rellena huecos CHICOS (bucles de borde cortos) de una malla; no toca bordes grandes (chunk).
    static void FillSmallHoles(TArray<int32>& T);
    static void GetCornerDeep(const FPTOctreeNode& N, int32 k, float& OutV, FColor& OutC);

    // ── Consulta espacial ────────────────────────────────────────────────────
    struct FLeafInfo
    {
        const FPTOctreeNode* Node = nullptr; // null si la región es aire (no allocada)
        FVector Min = FVector::ZeroVector;
        float   Size = 0.f;
    };
    FLeafInfo FindLeaf(const FVector& P) const;

    // ── Mallado ───────────────────────────────────────────────────────────────
    struct FLeafRef { const FPTOctreeNode* Node; FVector Min; float Size; };
    void CollectLeaves(const FPTOctreeNode* N, const FVector& NodeMin, float NodeSize, TArray<FLeafRef>& Out) const;
    void CollectBalanceCandidates(const FPTOctreeNode* N, const FVector& NodeMin, float NodeSize,
                                  const FBox& ChangedBounds, TArray<FLeafRef>& Out) const;
    // Recolecta hojas cuyo box intersecta Region, podando subtrees con NodeSize < MinSize (para no
    // recorrer lo fino en el pase grueso). Para el mallado por chunks.
    void CollectLeavesFiltered(const FPTOctreeNode* N, const FVector& NodeMin, float NodeSize,
                               const FBox& Region, float MinSize, TArray<FLeafRef>& Out) const;
    // Vértice Surface Nets de una hoja (promedio de cruces de sus 12 aristas). Devuelve false si no hay cruce.
    // OutColor = color interpolado en los cruces.
    static bool LeafVertex(const FPTOctreeNode& Leaf, const FVector& Min, float Size, FVector& OutLocal, FColor& OutColor);
    FVector FieldNormal(const FVector& P) const; // normal = -grad(SDF)
    FLinearColor SampleColorLinear(const FVector& P) const; // color interpolado del campo (para MC)

    static int32 CountLeavesRec(const FPTOctreeNode* N);
    static int32 CountNodesRec(const FPTOctreeNode* N);
};
