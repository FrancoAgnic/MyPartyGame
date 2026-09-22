// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTGameSettingsWidget.h"
#include "../PTGameInstance.h"
#include "../PTTextTable.h"
#include "../Multiplayer/MultiplayerSessionsSubsystem.h"
#include "PTLobbyGameMode.h"
#include "PTGameState.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "../Mods/PTWordPackSubsystem.h" // FPTWordPack (miniatura del banco)
#include "../Mods/PTMapModSubsystem.h"   // FPTMapMod (miniatura del mapa)
#include "ImageUtils.h"                   // ImportFileAsTexture2D / ImportBufferAsTexture2D
#include "Misc/Paths.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Engine/Texture2D.h"

bool UPTGameSettingsWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (TurnTimeMinus) TurnTimeMinus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnTurnTimeMinus);
    if (TurnTimePlus)  TurnTimePlus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnTurnTimePlus);
    if (RoundsMinus)   RoundsMinus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnRoundsMinus);
    if (RoundsPlus)    RoundsPlus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnRoundsPlus);
    if (RevealMinus)   RevealMinus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnRevealMinus);
    if (RevealPlus)    RevealPlus->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnRevealPlus);
    if (FriendsOnlyCheckbox) FriendsOnlyCheckbox->OnCheckStateChanged.AddDynamic(this, &UPTGameSettingsWidget::OnFriendsOnlyChanged);
    if (LibraryButton) LibraryButton->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnLibraryClicked);
    if (CloseButton)         CloseButton->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnCloseClicked);
    if (Btn_Back)            Btn_Back->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnCloseClicked);
    if (CloseSettingsButton) CloseSettingsButton->OnClicked.AddDynamic(this, &UPTGameSettingsWidget::OnCloseClicked);

    // Refrescar los textos "Word:/Map:" cuando el host cambia de banco o de mapa desde la Biblioteca.
    if (UPTGameInstance* GI = GetGI())
    {
        GI->OnSelectedWordPackChanged.AddUObject(this, &UPTGameSettingsWidget::RefreshPackTexts);
        GI->OnSelectedMapChanged.AddUObject(this, &UPTGameSettingsWidget::RefreshPackTexts);
    }
    return true;
}

UPTGameInstance* UPTGameSettingsWidget::GetGI() const
{
    return GetWorld() ? GetWorld()->GetGameInstance<UPTGameInstance>() : nullptr;
}

void UPTGameSettingsWidget::ShowPanel()
{
    SetVisibility(ESlateVisibility::Visible);
    // Inicializar el toggle de visibilidad con el estado actual de la sesión.
    if (FriendsOnlyCheckbox)
        if (UMultiplayerSessionsSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>() : nullptr)
            FriendsOnlyCheckbox->SetIsChecked(S->IsSessionFriendsOnly());
    RefreshUI();
    PlayPopIn();

    // Avisar (host) que el panel quedó abierto → los clientes muestran su vista read-only en vivo.
    if (UWorld* W = GetWorld())
        if (APTLobbyGameMode* GM = W->GetAuthGameMode<APTLobbyGameMode>())
            GM->SetHostSettingsPanelOpen(true);
}

void UPTGameSettingsWidget::OnCloseClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);

    // Panel cerrado → ocultar la vista de los clientes.
    if (UWorld* W = GetWorld())
        if (APTLobbyGameMode* GM = W->GetAuthGameMode<APTLobbyGameMode>())
            GM->SetHostSettingsPanelOpen(false);
}

void UPTGameSettingsWidget::PushSettingsToState()
{
    // Solo el host tiene GameMode con autoridad; en clientes GetAuthGameMode == null (no-op).
    if (UWorld* W = GetWorld())
        if (APTLobbyGameMode* GM = W->GetAuthGameMode<APTLobbyGameMode>())
            GM->SyncMatchSettingsToState();
}

void UPTGameSettingsWidget::RefreshPackTexts()
{
    const FText Default = PTText::Get(TEXT("GS_DEFAULT"));
    if (WordBankText)
    {
        const FString Title = GetGI() ? GetGI()->SelectedWordPackTitle : FString();
        const FText Word = Title.IsEmpty() ? Default : FText::FromString(Title);
        FFormatOrderedArguments Args; Args.Add(Word);
        WordBankText->SetText(PTText::Format(TEXT("GS_WORD"), Args));
    }
    if (MapText)
    {
        // Mapa custom elegido (vacío = mapa oficial).
        const FString MapTitle = GetGI() ? GetGI()->SelectedMapTitle : FString();
        const FText Map = MapTitle.IsEmpty() ? Default : FText::FromString(MapTitle);
        FFormatOrderedArguments Args; Args.Add(Map);
        MapText->SetText(PTText::Format(TEXT("GS_MAP"), Args));
    }

    // Replicar a los clientes (banco de palabras + valores numéricos, ya que RefreshUI pasa por acá).
    PushSettingsToState();

    RefreshThumbnails();
}

void UPTGameSettingsWidget::RefreshThumbnails()
{
    // Lee del GameState replicado (host lo pushea en PushSettingsToState) → funciona igual en host y
    // clientes (vista read-only). Si no hay preview, cae a la textura default asignada en el WBP.
    const APTGameState* PTGS = GetWorld() ? GetWorld()->GetGameState<APTGameState>() : nullptr;
    UGameInstance* GI = GetGameInstance();

    // ── MAPA ──
    if (MapThumbnail)
    {
        const FString Id = PTGS ? PTGS->MatchMapModId : FString();
        if (CachedMapThumbKey != Id)
        {
            UTexture2D* Tex = nullptr;
            if (!Id.IsEmpty())
            {
                FString Path;
                if (UPTMapModSubsystem* MM = GI ? GI->GetSubsystem<UPTMapModSubsystem>() : nullptr)
                {
                    if (const FPTMapMod* M = MM->FindMod(Id)) Path = M->PreviewPath;
                    if (Path.IsEmpty()) { MM->RescanMods(); if (const FPTMapMod* M2 = MM->FindMod(Id)) Path = M2->PreviewPath; }
                }
                if (!Path.IsEmpty() && FPaths::FileExists(Path))
                    Tex = FImageUtils::ImportFileAsTexture2D(Path);
            }
            if (!Tex) Tex = DefaultMapThumbnail; // mapa oficial o sin preview → default
            if (Tex) { MapThumbnail->SetBrushFromTexture(Tex, false); MapThumbnail->SetVisibility(ESlateVisibility::HitTestInvisible); }
            else       MapThumbnail->SetVisibility(ESlateVisibility::Collapsed);
            CachedMapThumbKey = Id;
        }
    }

    // ── BANCO DE PALABRAS ──
    if (WordPackThumbnail)
    {
        const FString Id  = PTGS ? PTGS->MatchWordPackId : FString();
        const FString URL = PTGS ? PTGS->MatchWordPackPreviewURL : FString();
        const FString Key = Id + TEXT("|") + URL;
        if (CachedPackThumbKey != Key)
        {
            FString Path;
            if (!Id.IsEmpty())
                if (UPTWordPackSubsystem* WP = GI ? GI->GetSubsystem<UPTWordPackSubsystem>() : nullptr)
                    if (const FPTWordPack* P = WP->FindPack(Id)) Path = P->PreviewPath;

            if (!Path.IsEmpty() && FPaths::FileExists(Path))
            {
                if (UTexture2D* Tex = FImageUtils::ImportFileAsTexture2D(Path))
                { WordPackThumbnail->SetBrushFromTexture(Tex, false); WordPackThumbnail->SetVisibility(ESlateVisibility::HitTestInvisible); }
                CachedPackThumbKey = Key;
            }
            else if (!URL.IsEmpty())
            {
                DownloadThumbnailTo(WordPackThumbnail, URL); // async
                CachedPackThumbKey = Key;
            }
            else // banco oficial o sin preview → default
            {
                if (DefaultWordPackThumbnail)
                { WordPackThumbnail->SetBrushFromTexture(DefaultWordPackThumbnail, false); WordPackThumbnail->SetVisibility(ESlateVisibility::HitTestInvisible); }
                else WordPackThumbnail->SetVisibility(ESlateVisibility::Collapsed);
                CachedPackThumbKey = Key;
            }
        }
    }
}

void UPTGameSettingsWidget::DownloadThumbnailTo(UImage* Target, const FString& URL)
{
    if (!Target || URL.IsEmpty()) return;
    TWeakObjectPtr<UImage> W = Target;
    UTexture2D* Fallback = DefaultWordPackThumbnail;
    TWeakObjectPtr<UTexture2D> WFallback = Fallback;
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(URL);
    Req->SetVerb(TEXT("GET"));
    Req->OnProcessRequestComplete().BindLambda(
        [W, WFallback](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
        {
            UImage* Img = W.Get();
            if (!Img) return;
            UTexture2D* Tex = (bOk && Resp) ? FImageUtils::ImportBufferAsTexture2D(Resp->GetContent()) : nullptr;
            if (!Tex) Tex = WFallback.Get(); // si falla la descarga, mostrar la default
            if (Tex) { Img->SetBrushFromTexture(Tex, false); Img->SetVisibility(ESlateVisibility::HitTestInvisible); }
        });
    Req->ProcessRequest();
}

void UPTGameSettingsWidget::OnFriendsOnlyChanged(bool bIsChecked)
{
    if (UMultiplayerSessionsSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>() : nullptr)
        S->SetSessionFriendsOnly(bIsChecked);
    PushSettingsToState(); // que los clientes vean el cambio de "Sala privada"
}

void UPTGameSettingsWidget::OnLibraryClicked()
{
    // El WBP_WordPack vive en el HUD; le pedimos que lo abra.
    OnRequestLibrary.ExecuteIfBound();
}

void UPTGameSettingsWidget::RefreshUI()
{
    UPTGameInstance* GI = GetGI();
    if (!GI) return;
    const FPTMatchSettings& S = GI->PendingMatchSettings;

    if (TurnTimeText) TurnTimeText->SetText(FText::FromString(FString::Printf(TEXT("%d s"), FMath::RoundToInt(S.TurnDuration))));
    if (RoundsText)   RoundsText->SetText(FText::FromString(FString::Printf(TEXT("%d"), S.NumRounds)));
    if (RevealText)   RevealText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(S.RevealFraction * 100.f))));

    RefreshPackTexts();
}

void UPTGameSettingsWidget::OnTurnTimeMinus() { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.TurnDuration = FMath::Clamp(GI->PendingMatchSettings.TurnDuration - 15.f, 15.f, 300.f); RefreshUI(); } }
void UPTGameSettingsWidget::OnTurnTimePlus()  { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.TurnDuration = FMath::Clamp(GI->PendingMatchSettings.TurnDuration + 15.f, 15.f, 300.f); RefreshUI(); } }
void UPTGameSettingsWidget::OnRoundsMinus()   { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.NumRounds = FMath::Clamp(GI->PendingMatchSettings.NumRounds - 1, 1, 10); RefreshUI(); } }
void UPTGameSettingsWidget::OnRoundsPlus()    { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.NumRounds = FMath::Clamp(GI->PendingMatchSettings.NumRounds + 1, 1, 10); RefreshUI(); } }
void UPTGameSettingsWidget::OnRevealMinus()   { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.RevealFraction = FMath::Clamp(GI->PendingMatchSettings.RevealFraction - 0.1f, 0.f, 0.9f); RefreshUI(); } }
void UPTGameSettingsWidget::OnRevealPlus()    { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.RevealFraction = FMath::Clamp(GI->PendingMatchSettings.RevealFraction + 0.1f, 0.f, 0.9f); RefreshUI(); } }
