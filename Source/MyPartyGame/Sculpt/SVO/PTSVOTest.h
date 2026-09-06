// Copyright Epic Games, Inc. All Rights Reserved.
// Actor de PRUEBA aislado para validar el octree adaptativo (Fase 1.4). Colocalo en un nivel y dale
// Play: esculpe un demo (una esfera GRANDE = pocos triángulos + varios detalles CHICOS = mucha malla)
// y lo dibuja en un ProceduralMesh, logueando hojas/nodos/tris. No toca el gameplay ni el esculpido real.
//
// Comandos de consola:
//   PTSVO.Demo    → re-esculpe el demo
//   PTSVO.Clear   → limpia
//   PTSVO.Stats   → imprime hojas/nodos/vértices/triángulos
//   PTSVO.Add R   → agrega una esfera de radio R en el centro (ver cómo cambia la resolución por tamaño)

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PTVoxelOctree.h"
#include "PTSVOTest.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

UCLASS()
class MYPARTYGAME_API APTSVOTest : public AActor
{
    GENERATED_BODY()

public:
    APTSVOTest();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    // Lado del cubo raíz del octree (UU) y profundidad máxima (detalle más fino).
    UPROPERTY(EditAnywhere, Category="SVO") float RootSize = 1000.f;
    UPROPERTY(EditAnywhere, Category="SVO") int32 MaxDepth = 7;
    UPROPERTY(EditAnywhere, Category="SVO") UMaterialInterface* Material = nullptr;

    UPROPERTY(VisibleAnywhere) UProceduralMeshComponent* Mesh = nullptr;

    // Color de arcilla actual (lo usan AddSphere y el demo). Cambialo con PTSVO.Color R G B.
    UPROPERTY(EditAnywhere, Category="SVO") FColor PaintColor = FColor::White;

    // API usada por los comandos de consola.
    void RunDemo();
    void ClearAll();
    void AddSphere(float Radius);   // esfera en el centro (con PaintColor), deshacible
    void Undo();                    // deshace el último Add
    void Rebuild();                 // re-mallar + loguear stats

    // Instancia activa (para los comandos de consola). Weak: puede no existir.
    static TWeakObjectPtr<APTSVOTest> Instance;

private:
    FPTVoxelOctree Octree;
    void EnsureInit();
};
