// Copyright Epic Games, Inc. All Rights Reserved.
// Panel de amigos de Steam dentro del juego: lista tus amigos (jugando Sculpturillo / en línea /
// desconectados) con un botón "Invitar" por cada uno, más "Actualizar" y "Invitar por Steam"
// (abre el overlay). Convive con las partidas públicas y el código; no reemplaza nada.
//
// En el WBP derivado (nombres EXACTOS; varios opcionales):
//   FriendsBox   (ScrollBox o cualquier Panel)  → contenedor de filas (lo llena el código)
//   RefreshButton (Button)                       → releer la lista
//   SteamInviteButton (Button)                   → abrir overlay de Steam (opcional)
//   BackButton   (Button)                        → cerrar el panel (opcional)
//   TitleText / EmptyText (TextBlock)            → título / "no hay amigos" (opcionales)
// En Details (categoría Friends) asignar RowWidgetClass = WBP de la fila (deriva de PTFriendRowWidget).

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTFriendsWidget.generated.h"

class UPanelWidget;
class UButton;
class UTextBlock;
class UPTFriendRowWidget;
class UMultiplayerSessionsSubsystem;

UCLASS()
class MYPARTYGAME_API UPTFriendsWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Mostrar el panel y (re)leer la lista de amigos. */
    UFUNCTION(BlueprintCallable, Category = "Friends")
    void ShowPanel();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget))         UPanelWidget* FriendsBox;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      RefreshButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      SteamInviteButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      BackButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   TitleText;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   EmptyText;

    /** Clase del widget de fila. Asignar en el WBP derivado. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Friends")
    TSubclassOf<UPTFriendRowWidget> RowWidgetClass;

    UFUNCTION() void OnRefreshClicked();
    UFUNCTION() void OnSteamInviteClicked();
    UFUNCTION() void OnBackClicked();

private:
    void Rebuild();                 // repuebla FriendsBox desde Sessions->GetFriends()
    void OnFriendsListUpdated();     // callback del subsistema

    UPROPERTY() UMultiplayerSessionsSubsystem* Sessions = nullptr;
    bool bBound = false;
};
