#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInterface.h"
#include "PTSculptField.h"
#include "SVO/PTVoxelOctree.h"
#include "PTSculptVolume.generated.h"

class UTexture2D;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UNiagaraSystem;
class UNiagaraComponent;

UENUM(BlueprintType)
// OJO: agregar SIEMPRE al final (los valores se serializan/replican; mover rompe saves y red).
// TriPrism es en realidad un CONO (histórico). Las de abajo son primitivas nuevas para el radial.
enum class EPTStampShape : uint8 { Sphere, Cube, Cylinder, TriPrism, Pyramid, Torus, Capsule, HexPrism, Octahedron };

UENUM(BlueprintType)
enum class EPTEditMode : uint8 { Add, Erase, Paint, Smooth };

UCLASS()
class MYPARTYGAME_API APTSculptVolume : public AActor
{
    GENERATED_BODY()
public:
    APTSculptVolume();

    // Resolución del campo (tamaño de celda en UU). Menor = más geometría/detalle.
    UPROPERTY(EditAnywhere, Category="Sculpt") float VoxelSize = 5.f;

    // ── SVO experimental (esculpido adaptativo tipo SculptrVR) detrás de un flag ──
    // ON = la GEOMETRÍA (Add/Erase) y el color van por el octree adaptativo (menos tris en grande,
    // detalle en chico), con mallas por bloque y replicado por las mismas operaciones.
    // OFF (default) = campo clásico FPTSculptField. SVO soporta pintura y capas; Smooth se ignora.
    UPROPERTY(EditAnywhere, Category="Sculpt|SVO") bool bUseSVO = false;
    UPROPERTY(EditAnywhere, Category="Sculpt") UMaterialInterface* ClayMaterial = nullptr;
    // Override opcional del material de la malla de arcilla. Si se asigna, el volumen lo usa en vez del
    // ClayMID (atlas) al crear/re-crear las secciones. Lo usa la CABEZA del modo G (material de pintura
    // 2D) para no pelear con el re-mallado. Default null → gameplay usa el atlas como siempre.
    UPROPERTY() UMaterialInterface* ClayMaterialOverride = nullptr;

    // ── Partículas de feedback (Borrar / Pintar) ─────────────────────────────
    // Se emiten mientras mantenés el sello. Las ve TODO EL MUNDO (salen en el Multicast del sello).
    // El color va por el parámetro User del sistema (ver SculptFXColorParam):
    //  - Borrar: el color de la arcilla QUE SE BORRA (no el del picker).
    //  - Pintar: el color del picker.
    // Asignar los sistemas en BP_SculptVolume; si quedan vacíos, no hay partículas.
    // (Agregar YA NO usa partículas: ahora la arcilla nueva "brilla" — ver AddGlow* abajo.)
    UPROPERTY(EditAnywhere, Category="Sculpt|FX") UNiagaraSystem* FXErase = nullptr; // piedritas rotas al borrar
    UPROPERTY(EditAnywhere, Category="Sculpt|FX") UNiagaraSystem* FXPaint = nullptr; // gotitas al pintar

    // Nombre del parámetro de color (User.*) que los sistemas usan para teñir las partículas.
    UPROPERTY(EditAnywhere, Category="Sculpt|FX") FName SculptFXColorParam = TEXT("Color");

    // Nombre del parámetro float (User.*) al que se le manda el RADIO de la brocha (en UU), para que
    // el Shape Location (esfera) de las partículas de Borrar/Pintar escale con el tamaño del pincel.
    UPROPERTY(EditAnywhere, Category="Sculpt|FX") FName SculptFXRadiusParam = TEXT("Radius");

    // ── Material de los cubitos de Borrar ────────────────────────────────────
    // El emisor de Borrar usa un MESH renderer; su material tiene un parámetro "Color". Para
    // teñirlo con el color de la arcilla que se borra, se crea un material dinámico y se le pasa al
    // sistema como override (parámetro User de tipo Material). Requiere que el sistema Niagara tenga
    // un parámetro User Material y que el mesh renderer use ESE parámetro (ver instrucciones).
    // Asignar acá el MISMO material del cubito.
    UPROPERTY(EditAnywhere, Category="Sculpt|FX") UMaterialInterface* EraseParticleMaterial = nullptr;
    // Parámetro de color DENTRO de ese material.
    UPROPERTY(EditAnywhere, Category="Sculpt|FX") FName EraseMaterialColorParam = TEXT("Color");
    // Nombre del parámetro User (de tipo Material) del sistema Niagara al que se le pasa el override.
    UPROPERTY(EditAnywhere, Category="Sculpt|FX") FName EraseMaterialUserParam  = TEXT("MeshMaterial");

    // Segundos sin sellar tras los cuales la fuente de partículas se apaga (soltaste el click).
    UPROPERTY(EditAnywhere, Category="Sculpt|FX", meta=(ClampMin="0.05", ClampMax="1.0"))
    float SculptFXIdleTimeout = 0.15f;

    // Color base de la arcilla (para el color de las partículas de Borrar si esa zona no estaba
    // pintada). En la práctica Agregar siempre pinta, así que rara vez se usa.
    UPROPERTY(EditAnywhere, Category="Sculpt|FX") FLinearColor ClayBaseColor = FLinearColor(0.62f, 0.55f, 0.5f);

    // ── Brillo de la arcilla nueva (Agregar, SOBRE la propia malla) ──────────
    // NO es un mesh aparte: el material del clay hace brillar los vértices recién agregados y los
    // desvanece hasta su color normal. Solo brilla lo NUEVO (el tiempo de agregado va horneado en
    // la UV0 de cada vértice). El material del clay debe leer TexCoord[0].x como "tiempo de agregado"
    // y compararlo con el parámetro escalar "NowTime" (que este actor actualiza cada frame):
    //     fresh = saturate(1 - (NowTime - TexCoord0.x) / NewClayGlowSeconds)
    //     Emissive += BaseColor * fresh * NewClayGlowBrightness
    // Estos dos valores se le pasan al material como parámetros (NewClayGlowSeconds/Brightness).
    UPROPERTY(EditAnywhere, Category="Sculpt|FX", meta=(ClampMin="0.05")) float NewClayGlowSeconds    = 1.2f;
    UPROPERTY(EditAnywhere, Category="Sculpt|FX", meta=(ClampMin="0.0"))  float NewClayGlowBrightness = 2.5f;

    // ── Modo Smooth (tuneables) ─────────────────────────────────────────────
    // Intensidad del suavizado por aplicación (0..1). Más alto = suaviza más rápido.
    UPROPERTY(EditAnywhere, Category="Sculpt|Smooth", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SmoothStrength = 0.35f;
    // Empuje por aplicación: >0 infla (crece), <0 encoge, 0 = neutro (recomendado).
    UPROPERTY(EditAnywhere, Category="Sculpt|Smooth", meta=(ClampMin="-0.2", ClampMax="0.2"))
    float SmoothBias = 0.f;
    // Tope de cambio por celda por aplicación (evita que se dispare al mantener).
    UPROPERTY(EditAnywhere, Category="Sculpt|Smooth", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SmoothMaxDelta = 0.15f;

    // Suavizado visual de la malla al generarla (0 = off, ~0.5 = suave). No borra detalle guardado.
    UPROPERTY(EditAnywhere, Category="Sculpt", meta=(ClampMin="0.0", ClampMax="1.0"))
    float DisplaySmoothing = 0.5f;

    // Tamaño del voxel de color (UU). Menor = detalle más fino, más atlas/VRAM.
    UPROPERTY(EditAnywhere, Category="Sculpt|Paint", meta=(ClampMin="1.0", ClampMax="10.0"))
    float ColorVoxel = 3.f;

    // Capacidad máxima de bricks de color allocados (define el tamaño del atlas).
    UPROPERTY(EditAnywhere, Category="Sculpt|Paint", meta=(ClampMin="256", ClampMax="131072"))
    int32 MaxColorBricks = 32768;

    // Dureza del borde del pincel: 0 = suave (degradé), 1 = duro (borde nítido).
    UPROPERTY(EditAnywhere, Category="Sculpt|Paint", meta=(ClampMin="0.0", ClampMax="1.0"))
    float PaintHardness = 0.6f;

    // StampRot = rotación del sello (rueda del mouse + arrastrar). Solo afecta la GEOMETRÍA
    // (Add/Erase/Smooth); Paint es un splat sobre la superficie y la ignora.
    // Devuelve true si el sello CAMBIÓ algo (para Borrar: hubo malla que borrar). Sirve para no
    // lanzar partículas de borrado en el aire.
    // StampScale = escala NO uniforme del sello (en su espacio local, antes de la rotación). (1,1,1) =
    // uniforme. Ej: (1,1,2) estira la forma al doble en Z. Se aplica deformando el punto de muestreo.
    bool ApplyStamp(FVector WorldPos, EPTStampShape Shape, float Size,
                    EPTEditMode Mode, FLinearColor PaintColor,
                    FRotator StampRot = FRotator::ZeroRotator,
                    FVector StampScale = FVector::OneVector);


    // ── Ojos (tecla 4): esferas/mesh aparte, replicadas, que se apoyan sobre la escultura ──
    UPROPERTY(EditAnywhere, Category="Sculpt|Eyes") UStaticMesh*        EyeMesh     = nullptr; // necesita "Allow CPU Access"
    UPROPERTY(EditAnywhere, Category="Sculpt|Eyes") UMaterialInterface* EyeMaterial = nullptr;
    UPROPERTY(EditAnywhere, Category="Sculpt|Eyes") float               EyeBaseSize = 50.f;

    // Coloca un ojo en WorldPos (radio). Se llama SOLO en el servidor (el controller enruta su
    // propio Server RPC, porque el volumen no tiene owner por jugador y sus Server RPC se descartan).
    // Se replica a todos via la propiedad Eyes → la escultura con ojos se ve igual en todos.
    void AddEye(FVector WorldPos, float Radius);

    // Acceso al ProceduralMesh (para hornear la escultura a otro componente, ej: la cabeza custom).
    UProceduralMeshComponent* GetMeshComponent() const { return Mesh; }
    // Mallas de las CAPAS de detalle (para hornearlas junto a la base en la cabeza custom).
    const TArray<UProceduralMeshComponent*>& GetDetailMeshes() const { return DetailMeshes; }
    const TMap<int32, UProceduralMeshComponent*>& GetSVOChunkMeshes() const { return SVOChunkMeshes; }

    /** ¿Ese punto del mundo cae DENTRO del lienzo (el BoundsBox)? Para no dejar poner cosas
     *  (ej: ojos) fuera de la zona de modelado. */
    UFUNCTION(BlueprintPure, Category="Sculpt")
    bool IsInsideCanvas(FVector WorldPos) const;

    /** Clampea un punto del mundo al INTERIOR del lienzo (BoundsBox), dejando un margen = InsetRadius
     *  en cada cara. Así la BROCHA (no solo su centro) queda dentro y "choca" con las paredes/piso/
     *  techo en vez de salirse. Si el área es más chica que la brocha en un eje, cae al centro de ese eje. */
    UFUNCTION(BlueprintPure, Category="Sculpt")
    FVector ClampInsideCanvas(FVector WorldPos, float InsetRadius) const;

    float        SampleWorldDensity(FVector WorldPos) const;
    FLinearColor SampleWorldColor  (FVector WorldPos) const;
    // Color PINTADO (atlas) en esa posición del mundo. bOutPainted=false si ese punto no fue
    // pintado (busca en un vecindario chico para no fallar por redondeo del voxel).
    FLinearColor SampleWorldPaintColor(FVector WorldPos, bool& bOutPainted) const;

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ApplyStamp(FVector WorldPos, EPTStampShape Shape, float Size,
                           EPTEditMode Mode, FLinearColor PaintColor, FRotator StampRot, FVector StampScale);

    // bDetail=true → el sello cae en la CAPA de detalle activa (la última creada con Multicast_BeginDetailLayer),
    // no en la base. Solo se usa con Add (ALT). false = base (comportamiento normal).
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ApplyStamp(FVector WorldPos, EPTStampShape Shape, float Size,
                              EPTEditMode Mode, FLinearColor PaintColor, FRotator StampRot, bool bDetail, FVector StampScale);

    // Aplica el sello Y dispara el feedback (partículas de Borrar/Pintar, brillo de Agregar).
    // Lo llama el Multicast (gameplay, en todos los clientes) y también el modo G del lobby
    // (esculpido de la cabeza, local). Así el modo G tiene las mismas partículas que el gameplay.
    void ApplyStampAndFX(FVector WorldPos, EPTStampShape Shape, float Size,
                         EPTEditMode Mode, FLinearColor PaintColor, FRotator StampRot = FRotator::ZeroRotator,
                         FVector StampScale = FVector::OneVector);

    // Dispara SOLO la partícula de pintar (gotitas) en un punto del mundo, sin tocar la escultura.
    // Lo usa el pintado del CUERPO en el modo G (que pinta una textura, no el volumen).
    void PlayPaintFXAt(const FVector& WorldPos, const FLinearColor& Color, float BrushSize)
    { PlaySculptFX(FXPaint, EPTEditMode::Paint, WorldPos, Color, BrushSize); }

    // Crea un material dinámico (de ClayMaterial) para la cabeza HORNEADA que muestrea COPIAS de las
    // texturas de color 3D (PageTable + Atlas) con los mismos parámetros que el clay en vivo. Como es
    // el mismo material + mismos datos + mismo espacio (posiciones locales), la cabeza usada se ve
    // EXACTAMENTE igual a como la pintaste (sin re-muestrear ni perder resolución). Sin glow.
    // Las copias quedan referenciadas por el MID (Outer = el personaje), así sobreviven al volumen.
    UMaterialInstanceDynamic* CreateBakedColorMID(UObject* Outer);

    // Limpia toda la escultura (geometría + color) dejando el lienzo en blanco.
    // Reusa las texturas de color existentes (solo re-sube la page table vacía).
    void ClearAll();

    // ── Estado CRUDO (para re-editar la cabeza del Locker) ──────────────────
    // Serializa SOLO la geometría (campo SDF + color por celda). NO incluye ojos ni la textura de
    // pintura 2D de la cabeza (de eso se encarga el controller del modo G). LoadFieldState fuerza
    // el re-mallado; el material correcto lo pone RebuildDirty al crear cada sección.
    void SaveFieldState(TArray<uint8>& Out);
    bool LoadFieldState(const TArray<uint8>& In);

    // ── Snapshot COMPLETO (para el que se (re)conecta tarde) ────────────────
    // Geometría (base + capas) + PINTURA del atlas 3D → el late-joiner ve la escultura tal cual.
    void SaveSnapshot(TArray<uint8>& Out);
    void LoadSnapshot(const TArray<uint8>& In);
    // Serialización solo de la pintura (voxeles de color del atlas). La usa el snapshot.
    void SavePaintState(TArray<uint8>& Out) const;
    void LoadPaintState(const TArray<uint8>& In);

    // Reset sincronizado en todos: el GameMode (servidor) lo llama al empezar cada turno.
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ClearAll();

    // ── Undo (deshacer la última acción) ────────────────────────────────────
    // Un "trazo" = desde que apretás el click hasta que lo soltás (o un ojo colocado). Cada
    // cliente graba su propio respaldo, así que el undo se difunde y todos deshacen igual.
    UFUNCTION(NetMulticast, Reliable) void Multicast_BeginStroke();
    UFUNCTION(NetMulticast, Reliable) void Multicast_EndStroke();
    UFUNCTION(NetMulticast, Reliable) void Multicast_Undo();

    // ── Capas de DETALLE (ALT) ──────────────────────────────────────────────
    // Cada período de Alt = una capa aparte: su propio campo SDF + su propio ProceduralMesh. Fusiona
    // consigo misma pero NO con la base ni con otras capas. Replicada (este multicast la crea en todos).
    // Mientras la capa está activa, los sellos de detalle caen en ella (ver Multicast_ApplyStamp bDetail).
    UFUNCTION(NetMulticast, Reliable) void Multicast_BeginDetailLayer();

    bool CanUndo() const { return VolumeUndoStack.Num() > 0; }

    // Preview de la forma del sello (malla fantasma que sigue al cursor).
    static void BuildStampPreview(EPTStampShape Shape, float Size, float VoxSz,
                                  TArray<FVector>& OutVerts, TArray<int32>& OutTris,
                                  TArray<FVector>& OutNormals, FVector StampScale = FVector::OneVector);

    // SDF de cada forma, centrado en origen. HalfSize en unidades de celda.
    static float StampSDF(EPTStampShape Shape, FVector LocalPos, float HalfSize);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION() void OnRep_Eyes();
    void RebuildEyesMesh(); // reconstruye EyesMesh desde Eyes (usa EyeMesh/EyeMaterial)
    void EraseEyesNear(const FVector& WorldPos, float Radius); // saca los ojos bajo la brocha (servidor)

    // ── Fuente de partículas al esculpir (Borrar/Pintar) ─────────────────────
    // Componente único reusado: se lo reubica en cada sello, se le setea el sistema según la
    // herramienta y el color, y se apaga solo cuando pasa SculptFXIdleTimeout sin sellar.
    UPROPERTY() UNiagaraComponent* SculptFX = nullptr;
    UPROPERTY() UMaterialInstanceDynamic* EraseMID = nullptr; // material dinámico de los cubitos de Borrar
    EPTEditMode  SculptFXMode = EPTEditMode::Smooth; // último sistema puesto (para no re-asignar por frame)
    float        SculptFXLastTime = -1000.f;         // último instante en que se selló
    FLinearColor LastErasedColor  = FLinearColor::White; // color de la última arcilla sólida borrada
    // Emite/reubica la fuente en el punto del sello (llamado desde el Multicast → lo ven todos).
    void PlaySculptFX(UNiagaraSystem* Sys, EPTEditMode Mode, const FVector& WorldPos, const FLinearColor& Color, float BrushSize);


private:
    UPROPERTY(VisibleAnywhere) UProceduralMeshComponent* Mesh;
    UPROPERTY(VisibleAnywhere) UBoxComponent* BoundsBox;
    UPROPERTY() UProceduralMeshComponent* EyesMesh = nullptr; // malla de ojos (aparte de la arcilla)

    // Ojos colocados (local al volumen: XYZ=centro, W=radio). Replicado → todos ven los mismos.
    UPROPERTY(ReplicatedUsing=OnRep_Eyes) TArray<FVector4> Eyes;

    // ── Undo: lo que el campo NO cubre (pintura del atlas + ojos) ───────────
    // La geometría la respalda FPTSculptField (copy-on-write por brick). Acá va el resto, con una
    // pila 1:1 con la del campo para que Undo deshaga las dos partes a la vez.
    struct FPTVolumeUndo
    {
        TMap<int32, FColor> AtlasOld;      // texel del atlas → color previo (solo el 1er cambio)
        TSet<int32>         Slots;         // slots tocados (para re-subir esos tiles al deshacer)
        int32               EyesCount = 0; // cuántos ojos había antes del trazo
    };
    TArray<FPTVolumeUndo>  VolumeUndoStack;
    FPTVolumeUndo          CurrentVolumeUndo;
    bool                   bRecordingStroke = false;
    static constexpr int32 MaxUndoSteps = 8;
    /** Guarda el color previo de un texel del atlas (solo la 1ra vez en el trazo). */
    void BackupAtlas(int32 AIdx, int32 Slot);

    FPTSculptField Field; // campo BASE (la arcilla principal)

    // ── SVO (modo bUseSVO) ───────────────────────────────────────────────────
    FPTVoxelOctree SVOField;      // octree adaptativo BASE en espacio ACTOR-LOCAL (UU)
    bool bSVOInit  = false;       // ya inicializado sobre el lienzo actual
    bool bSVODirty = false;       // hay ediciones sin re-mallar
    // Capas de DETALLE (ALT): un octree por capa + su ProceduralMesh (reusa DetailMeshes, paralelo).
    TArray<TSharedPtr<FPTVoxelOctree>> SVODetailFields;
    TSet<int32> DirtySVODetailLayers;
    FPTVoxelOctree* ActiveSVO = &SVOField; // dónde caen los sellos (base o última capa)
    void InitSVO();               // arma el octree BASE cubriendo el BoundsBox
    void InitSVOOctree(FPTVoxelOctree& F) const; // inicializa un octree cualquiera sobre el BoundsBox
    void ApplyStampSVO(FVector WorldPos, EPTStampShape Shape, float Size, EPTEditMode Mode,
                       FLinearColor PaintColor, FRotator StampRot, FVector StampScale);
    void RebuildSVOMesh();        // remalla base (por chunks) + capas
    void RebuildSVOInto(FPTVoxelOctree& F, UProceduralMeshComponent* M); // remalla un octree entero a un mesh (capas)

    // Base: Mesh sección 0 = hojas grandes. Cada chunk fino ocupado tiene su propio componente;
    // cambiar su topología no recrea el scene proxy de los otros chunks.
    static constexpr int32 SVOChunkDim = 12;                // 12^3 chunks (grilla fina → re-mallado acotado)
    TSet<int32> DirtySVOChunks;                             // chunks finos a rehacer
    // One render proxy per occupied chunk: changing topology must not upload the whole sculpture.
    UPROPERTY(Transient) TMap<int32, UProceduralMeshComponent*> SVOChunkMeshes;
    void ClearSVOChunkMeshes();
    bool bSVOCoarseDirty = true;                            // sección gruesa a rehacer
    void MarkAllSVODirty();                                 // marca todo (tras load/clear/demo)
    void MarkSVODirtyLocalBounds(const FVector& LMin, const FVector& LMax); // marca chunks tocados por una edición
    void RebuildSVOChunk(int32 ChunkIndex);                // rehace una sección de chunk fino (sync)
    // Aplica una malla de chunk ya construida a su componente (game thread). Lo usa el mallado async.
    void ApplySVOChunkMesh(int32 ChunkIndex, const TArray<FVector>& V, const TArray<int32>& T,
                           const TArray<FVector>& N, const TArray<FColor>& C);
    bool bSVOMeshing = false;                              // hay un mallado async de chunks en vuelo
    uint32 SVOMeshGen = 0;                                 // generación: descarta resultados async viejos tras clear/load
    float SVOChunkSize() const { return SVOField.GetRootSize() / (float)SVOChunkDim; }
    // Hojas <= chunk = "finas" (van a chunks); > chunk = grandes (pocas, sección gruesa). Umbral = chunk
    // para que quepan en un chunk y el halo de 1 chunk cubra sus vecinos.
    float SVOFineThreshold() const { return SVOChunkSize(); }

    // ── Capas de detalle (ALT): campos + meshes aparte ──────────────────────
    // Cada capa fusiona consigo misma pero no con la base ni con otras. ActiveField apunta a dónde
    // caen los sellos (base por defecto; la última capa mientras se dibuja detalle).
    FPTSculptField* ActiveField = &Field;
    TArray<TSharedPtr<FPTSculptField>>            DetailFields;   // 1 campo por capa
    UPROPERTY() TArray<UProceduralMeshComponent*> DetailMeshes;   // 1 mesh por capa (paralelo a DetailFields)
    // Orden LIFO de operaciones para el undo: 0 = trazo de la BASE, 1 = capa de DETALLE.
    TArray<uint8> UndoOrder;

    bool  bRebuildInProgress = false;
    float TimeSinceRebuild   = 0.f;
    static constexpr float RebuildInterval = 0.05f;

    void RebuildDirty();
    // Crea el mesh de una capa de detalle (mismo transform/material que la base). Devuelve el componente.
    UProceduralMeshComponent* CreateDetailLayerMesh();
    void MarkStampDirty(int32 x0, int32 y0, int32 z0, int32 x1, int32 y1, int32 z1);

    // Bloqueo del ÁREA (BoundsBox): solo el escultor del turno entra al cubo; los demás rebotan. Solo
    // se activa en gameplay (si hay APTSculptGameState); en el lobby el cubo nunca bloquea.
    void  UpdateSculptBoundaryCollision();
    float BoundaryAccum = 0.f;
    bool  bBoundaryOn   = false;

    // ── Pintura por campo de color 3D DISPERSO (bricks + page table + atlas) ──
    // El color vive en un campo 3D real (sin bleed), pero solo se allocan bricks
    // donde se pinta (disperso → memoria ∝ superficie). El material hace 2 lecturas:
    // page table (brick → slot) y atlas (voxel → color).
    static constexpr int32 CB = 8; // voxels de color por brick, por eje

    UPROPERTY(Transient) UTexture2D* PageTex  = nullptr; // R16, point: brickCoord → slot+1
    UPROPERTY(Transient) UTexture2D* AtlasTex = nullptr; // RGBA8, bilinear: voxels de color
    UPROPERTY(Transient) UMaterialInstanceDynamic* ClayMID = nullptr;

    TMap<FIntVector,int32> BrickSlot;  // brick coord → slot en el atlas
    TArray<float>  PageBuf;            // mirror CPU de la page table (R32F: slot+1)
    TArray<FColor> AtlasBuf;           // mirror CPU del atlas
    TSet<int32>    DirtyTiles;         // slots con tile sucio (subida parcial)
    TArray<int32>  DirtyPageIdx;       // texels de page table nuevos (subida incremental)
    TArray<int32>  FreeSlots;          // slots liberados (brick borrado) para reusar
    TArray<int32>  SlotUsed;           // voxels pintados por slot → liberar al llegar a 0
    int32 NextSlot      = 0;
    int32 AtlasCapacity = 4096;
    bool  bPageDirty           = false;
    bool  bPaintDirty          = false;
    float TimeSincePaintUpload = 0.f;
    static constexpr float PaintUploadInterval = 0.05f;

    // Dimensiones derivadas del lienzo.
    FVector    CanvasMinLocal  = FVector::ZeroVector;
    FVector    CanvasSizeLocal = FVector(960.f);
    FIntVector ColorVoxDim     = FIntVector(320); // voxels de color por eje
    FIntVector ColorBrickDim   = FIntVector(40);  // bricks por eje (= dims page table)
    int32 AtlasTilesPerRow     = 64;
    int32 AtlasW = 512, AtlasH = 4096;

    void InitColorField();
    void SetupClayMID();
    // Devuelve true si pintó sobre superficie (para no lanzar partículas al pintar en el aire).
    bool WritePaintStamp(FVector WorldPos, EPTStampShape Shape, float Size, FLinearColor Color, bool bFull = false,
                         FVector StampScale = FVector::OneVector);
    // Escribe un voxel de color (alloca brick si hace falta). false = atlas lleno.
    bool WriteColorVoxel(int32 vx, int32 vy, int32 vz, const FColor& C);
    // Limpia un voxel de color (libera el brick si queda vacío).
    void ClearColorVoxel(int32 vx, int32 vy, int32 vz);
    // Borra la pintura dentro del volumen del sello (al usar Erase).
    void ClearPaintStamp(FVector WorldPos, EPTStampShape Shape, float Size, FVector StampScale = FVector::OneVector);
    void UploadColorField();

    // Coordenadas: mundo → celda (float) en espacio local del actor.
    FVector WorldToCell(FVector W) const;
    // Rango de celdas del lienzo (definido por el BoundsBox).
    void    CellBounds(FIntVector& OutMin, FIntVector& OutMax) const;

public:
    // ── Preview (marching cubes sobre un grid chico aislado). PÚBLICO: lo reusa el mallado del SVO. ──
    static void RunMarchingCubes(const TArray<float>& G, const TArray<FLinearColor>& CG,
                                 int32 GS, float VoxSz,
                                 int32 X0, int32 Y0, int32 Z0, int32 X1, int32 Y1, int32 Z1,
                                 TArray<FVector>& OutVerts, TArray<int32>& OutTris,
                                 TArray<FVector>& OutNormals, TArray<FColor>& OutColors);
    static FVector Interp     (FVector P1, FVector P2, float V1, float V2);
    static FColor  InterpColor(FLinearColor C1, FLinearColor C2, float V1, float V2);
protected:

    static const int32 EdgeTable[256];
    static const int32 TriTable[256][16];
};
