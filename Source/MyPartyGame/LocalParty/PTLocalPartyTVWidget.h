// Overlay de la TV en el modo local. Se arma 100% en C++ (no necesita WBP), pero se puede reemplazar
// por un BP hijo desde APTSculptPlayerController::LocalPartyTVClass.
// No roba el mouse: todo es HitTestInvisible salvo el botón "Copiar link para el chat" debajo del QR
// (y sus contenedores, SelfHitTestInvisible), que se clickea con el mouse o se alcanza con el joystick.
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
class UButton;
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
    // Ocultar la IP escrita (para streams/capturas): se ve "192.168.•••.•••:8787" y la completa solo
    // mientras se mantiene View (joystick) o la tecla I. El QR siempre lleva la dirección real.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LocalParty") bool bMaskJoinAddress = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LocalParty") FLinearColor PanelColor  = FLinearColor(0.012f, 0.008f, 0.03f, 0.88f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LocalParty") FLinearColor AccentColor = FLinearColor(1.f, 0.48f, 0.08f, 1.f);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void BuildTree();
    void Refresh();
    bool IsRevealHeld() const;
    FString DisplayAddress(const FString& Url, bool bReveal) const;
    UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold, bool bWrap = false);
    void MakeOnlyCopyButtonHittable();
    UFUNCTION() void OnCopyJoinLink();
    UBorder* MakePanel(const FLinearColor& Color, float Radius, const FMargin& InPadding);

    UPROPERTY() UBorder*      LobbyPanel = nullptr;
    UPROPERTY() UImage*       QrImage    = nullptr;
    UPROPERTY() UTextBlock*   UrlText    = nullptr;
    UPROPERTY() UTextBlock*   Step1Text  = nullptr;
    UPROPERTY() UTextBlock*   Step2Text  = nullptr;
    UPROPERTY() UWidget*      CodeRow    = nullptr; // online: "Código de sala: KQZT"
    UPROPERTY() UTextBlock*   ChatJoinText = nullptr; // audiencia leyendo el chat: "o escribe !unirse en el chat"
    UPROPERTY() UTextBlock*   CodeText   = nullptr;
    UPROPERTY() UTextBlock*   RevealHint = nullptr;
    UPROPERTY() UTextBlock*   PlayersTitle = nullptr;
    UPROPERTY() UVerticalBox* PlayersBox = nullptr;
    UPROPERTY() UTextBlock*   StatusText = nullptr;
    UPROPERTY() UBorder*      TurnBanner = nullptr;
    UPROPERTY() UTextBlock*   BannerTitle = nullptr;
    UPROPERTY() UTextBlock*   BannerSub  = nullptr;
    UPROPERTY() UBorder*      CornerPanel = nullptr;
    UPROPERTY() UTextBlock*   CornerText = nullptr;
    UPROPERTY() UTexture2D*   QrTexture  = nullptr;
    // Debajo del QR PÚBLICO: copia GetJoinUrl() (nunca el link privado del streamer).
    UPROPERTY() UButton*      CopyLinkButton = nullptr;
    UPROPERTY() UTextBlock*   CopyLinkText = nullptr;

    FString QrForUrl = TEXT("?"); // distinto de cualquier URL → el primer Refresh arma el QR
    FString PlayersSig;
    float   RefreshAccum = 1.f;
    double  CopiedUntil = 0.0;
    int32   ShownCopyState = -1; // el texto del botón se reescribe solo si cambia (ver PTLocalizationSubsystem)
};
