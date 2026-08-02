// Copyright Epic Games, Inc. All Rights Reserved.
// SaveGame de la cabeza custom esculpida (v1 local): guarda la malla horneada (secciones del
// ProceduralMesh) para restaurarla al reabrir el juego. No guarda el campo de voxels, solo la
// geometría final (más simple y suficiente para "que vuelva tu cabeza").

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PTHeadSaveGame.generated.h"

USTRUCT()
struct FPTHeadSection
{
    GENERATED_BODY()

    UPROPERTY() TArray<FVector>   Verts;
    UPROPERTY() TArray<int32>     Tris;
    UPROPERTY() TArray<FVector>   Normals;
    UPROPERTY() TArray<FVector2D> UVs;
    UPROPERTY() TArray<FColor>    Colors;
    // true = sección de OJOS (esferas): al aplicar usa el material de ojos, no el de arcilla.
    UPROPERTY() bool bEye = false;
};

UCLASS()
class MYPARTYGAME_API UPTHeadSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    // Legacy (saves viejos): solo geometría por secciones.
    UPROPERTY() TArray<FPTHeadSection> Sections;
    // Nuevo: blob completo (geometría + centro + texturas de pintura de cabeza/cuerpo en PNG). Es el
    // MISMO blob que se replica, así el guardado y la red comparten formato.
    UPROPERTY() TArray<uint8> Blob;
};
