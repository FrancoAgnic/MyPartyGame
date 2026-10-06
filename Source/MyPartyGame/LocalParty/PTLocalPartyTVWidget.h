// Overlay de la TV en el modo local. Se arma 100% en C++ (no necesita WBP), pero se puede reemplazar
// por un BP hijo desde APTSculptPlayerController::LocalPartyTVClass.
//
//  · Esperando jugadores: panel central con el QR + la URL para entrar desde el celular, la lista de
//    jugadores conectados y quién tiene que tocar "Empezar".
//  · Eligiendo palabra:   cartel grande "¡Pásale el joystick a X!".
//  · Esculpiendo:         ayuda de controles del joystick + recordatorio chico de la URL (para los
//                         que llegan tarde).

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTLocalPartyTVWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UVerticalBox;
class UTexture2D;

UCLASS()
class MYPARTYGAME_API UPTLocalPartyTVWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LocalParty") float QrSize          = 300.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LocalParty") int32 TitleFontSize   = 40;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LocalParty") int32 BodyFontSize    = 22;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LocalParty") int32 UrlFontSize     = 34;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LocalParty") int32 BannerFontSize  = 46;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LocalParty") FLinearColor PanelColor  = FLinearColor(0.012f, 0.008f, 0.03f, 0.88f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LocalParty") FLinearColor AccentColor = FLinearColor(1.f, 0.48f, 0.08f, 1.f);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void BuildTree();
    void Refresh();
    UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold, bool bWrap = false);
    UBorder* MakePanel(const FLinearColor& Color, float Radius, const FMargin& InPadding);

    UPROPERTY() UBorder*      LobbyPanel = nullptr;
    UPROPERTY() UImage*       QrImage    = nullptr;
    UPROPERTY() UTextBlock*   UrlText    = nullptr;
    UPROPERTY() UTextBlock*   PlayersTitle = nullptr;
    UPROPERTY() UVerticalBox* PlayersBox = nullptr;
    UPROPERTY() UTextBlock*   StatusText = nullptr;
    UPROPERTY() UBorder*      TurnBanner = nullptr;
    UPROPERTY() UTextBlock*   BannerTitle = nullptr;
    UPROPERTY() UTextBlock*   BannerSub  = nullptr;
    UPROPERTY() UBorder*      CornerPanel = nullptr;
    UPROPERTY() UTextBlock*   CornerText = nullptr;
    UPROPERTY() UBorder*      PadPanel   = nullptr;
    UPROPERTY() UTexture2D*   QrTexture  = nullptr;

    FString QrForUrl = TEXT("?"); // distinto de cualquier URL → el primer Refresh arma el QR
    FString PlayersSig;
    float   RefreshAccum = 1.f;
};
