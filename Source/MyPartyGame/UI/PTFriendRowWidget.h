// Copyright Epic Games, Inc. All Rights Reserved.
// Fila de UN amigo de Steam dentro del panel de amigos: nombre + estado + botón "Invitar".
// La llena UPTFriendsWidget con un FPTFriendInfo. El botón invita a ese amigo a la sesión actual.
//
// En el WBP derivado (nombres EXACTOS):
//   NameText   (TextBlock)  → nombre del amigo
//   StatusText (TextBlock)  → "Jugando" / "En línea" / "Desconectado"  (opcional)
//   InviteButton (Button)   → invitar a este amigo
//   InviteButtonText (TextBlock) → texto del botón (opcional; si no, no se toca)

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "MultiplayerSessionsSubsystem.h"   // FPTFriendInfo
#include "PTFriendRowWidget.generated.h"

class UTextBlock;
class UButton;
class UMultiplayerSessionsSubsystem;

UCLASS()
class MYPARTYGAME_API UPTFriendRowWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Cargar la fila con los datos del amigo. La llama UPTFriendsWidget al reconstruir la lista. */
    void Init(const FPTFriendInfo& InFriend);

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget))         UTextBlock* NameText;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* StatusText;
    UPROPERTY(meta = (BindWidget))         UButton*    InviteButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* InviteButtonText;

    UFUNCTION() void OnInviteClicked();

private:
    FPTFriendInfo Friend;
    UPROPERTY() UMultiplayerSessionsSubsystem* Sessions = nullptr;
};
