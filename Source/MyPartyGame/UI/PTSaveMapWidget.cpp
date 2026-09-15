// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSaveMapWidget.h"
#include "../PTGameInstance.h"
#include "../PTTextTable.h"
#include "../Sculpt/PTSculptVolume.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"

UPTGameInstance* UPTSaveMapWidget::GI() const { return Cast<UPTGameInstance>(GetGameInstance()); }

void UPTSaveMapWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (ThumbnailButton) ThumbnailButton->OnClicked.AddDynamic(this, &UPTSaveMapWidget::OnThumbnailClicked);
    if (ConfirmButton)     ConfirmButton->OnClicked.AddDynamic(this, &UPTSaveMapWidget::OnConfirmClicked);
    if (SaveAndExitButton) SaveAndExitButton->OnClicked.AddDynamic(this, &UPTSaveMapWidget::OnSaveAndExitClicked);
    if (CancelButton)      CancelButton->OnClicked.AddDynamic(this, &UPTSaveMapWidget::OnCancelClicked);
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTSaveMapWidget::ShowPanel()
{
    SetVisibility(ESlateVisibility::Visible);
    PendingThumb.Reset();
    if (StatusText) StatusText->SetVisibility(ESlateVisibility::Collapsed);

    // Autocompletar título/descripción del mapa actual (los que pusiste al crear/guardar).
    UPTGameInstance* G = GI();
    if (G)
    {
        FString T, D;
        if (G->GetAuthoredMapMeta(G->CurrentAuthoringSlug, T, D))
        {
            if (TitleBox) TitleBox->SetText(FText::FromString(T));
            if (DescBox)  DescBox->SetText(FText::FromString(D));
        }
        // Mostrar la miniatura actual del mapa si ya tiene una.
        const FString Prev = G->AuthoredMapPreviewPath(G->CurrentAuthoringSlug);
        if (ThumbnailImage)
        {
            if (FPaths::FileExists(Prev))
            {
                if (UTexture2D* Tex = FImageUtils::ImportFileAsTexture2D(Prev))
                { ThumbnailImage->SetBrushFromTexture(Tex, false); ThumbnailImage->SetVisibility(ESlateVisibility::Visible); }
            }
            else ThumbnailImage->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    PlayPopIn();
}

void UPTSaveMapWidget::OnThumbnailClicked()
{
    UPTGameInstance* G = GI();
    if (!G) return;
    FString Path;
    if (!G->PickImageFile(Path)) return;
    PendingThumb = Path;
    if (ThumbnailImage)
    {
        if (UTexture2D* Tex = FImageUtils::ImportFileAsTexture2D(Path))
        { ThumbnailImage->SetBrushFromTexture(Tex, false); ThumbnailImage->SetVisibility(ESlateVisibility::Visible); }
    }
}

bool UPTSaveMapWidget::DoSave()
{
    UWorld* W = GetWorld();
    UPTGameInstance* G = GI();
    APTSculptVolume* Vol = W ? Cast<APTSculptVolume>(
        UGameplayStatics::GetActorOfClass(W, APTSculptVolume::StaticClass())) : nullptr;
    if (!G || !Vol) return false;

    const FString Title = TitleBox ? TitleBox->GetText().ToString().TrimStartAndEnd() : FString();
    const FString Desc  = DescBox  ? DescBox->GetText().ToString().TrimStartAndEnd()  : FString();

    TArray<uint8> Blob;
    Vol->SaveSnapshot(Blob);                          // escenario (geometría + pintura)
    G->SaveAuthoredMap(Blob);                         // → sculpt.bin del mapa actual
    G->SaveAuthoredMapMeta(Title, Desc, PendingThumb);// → mod.json (título/desc) + preview.png (si elegiste)
    return true;
}

void UPTSaveMapWidget::OnConfirmClicked()
{
    // Apply: guardar y cerrar el popup (seguís en el nivel).
    if (!DoSave()) return;
    if (StatusText)
    {
        StatusText->SetText(PTText::Get(TEXT("MAP_SAVED")));
        StatusText->SetVisibility(ESlateVisibility::Visible);
    }
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTSaveMapWidget::OnSaveAndExitClicked()
{
    // Guardar y salir al menú principal.
    DoSave();
    UGameplayStatics::OpenLevel(this, FName(TEXT("MainMenu")));
}

void UPTSaveMapWidget::OnCancelClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}
