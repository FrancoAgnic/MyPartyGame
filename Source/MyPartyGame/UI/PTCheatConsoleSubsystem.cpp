// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTCheatConsoleSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"
#include "HAL/PlatformTime.h"

// ── Comando MAESTRO para desbloquear. Cambiá esta palabra para rotar la "llave". Es anti-accidente /
//    anti-trampa casual, NO una barrera de seguridad: con tipearlo exacto alcanza. ──
static const TCHAR* PT_CHEAT_UNLOCK = TEXT("ninjasculpt");

// Ventana (seg) para el doble combo del gesto de apertura.
static const double PT_CHEAT_DOUBLE_WINDOW = 0.6;

// ── Input preprocessor: detecta el gesto a nivel aplicación (Ctrl+Alt+Shift+K ×2) y el Escape para cerrar ──
class FPTCheatInputProcessor : public IInputProcessor
{
public:
    explicit FPTCheatInputProcessor(UPTCheatConsoleSubsystem* InOwner) : Owner(InOwner) {}

    virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}

    virtual bool HandleKeyDownEvent(FSlateApplication& /*App*/, const FKeyEvent& Ev) override
    {
        UPTCheatConsoleSubsystem* O = Owner.Get();
        if (!O) return false;

        // Con la consola abierta, Escape la cierra (y lo consumimos para que no caiga al gameplay).
        if (O->IsOpen() && Ev.GetKey() == EKeys::Escape)
        {
            O->Close();
            return true;
        }

        // Gesto: Ctrl+Alt+Shift+K. Consumimos SIEMPRE que entra el combo (para que la K no llegue al juego).
        if (Ev.GetKey() == EKeys::K && Ev.IsControlDown() && Ev.IsAltDown() && Ev.IsShiftDown())
        {
            const double Now = FPlatformTime::Seconds();
            if (Now - LastCombo <= PT_CHEAT_DOUBLE_WINDOW)
            {
                LastCombo = 0.0;   // consumido: exige otro doble para la próxima
                O->Toggle();
            }
            else
            {
                LastCombo = Now;   // primer toque del doble
            }
            return true;
        }
        return false;
    }

    TWeakObjectPtr<UPTCheatConsoleSubsystem> Owner;
    double LastCombo = 0.0;
};

void UPTCheatConsoleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (FSlateApplication::IsInitialized())
    {
        InputProcessor = MakeShared<FPTCheatInputProcessor>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
    }
}

void UPTCheatConsoleSubsystem::Deinitialize()
{
    Close();
    if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
        FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
    InputProcessor.Reset();
    Super::Deinitialize();
}

void UPTCheatConsoleSubsystem::Toggle()
{
    if (bOpen) Close(); else Open();
}

void UPTCheatConsoleSubsystem::Open()
{
    if (bOpen || !GEngine || !GEngine->GameViewport || !FSlateApplication::IsInitialized()) return;

    const FSlateFontInfo Mono = FCoreStyle::GetDefaultFontStyle("Mono", 12);

    Root =
        SNew(SBox)
        .HAlign(HAlign_Center).VAlign(VAlign_Top)
        .Padding(FMargin(0.f, 24.f, 0.f, 0.f))
        [
            SNew(SBox).WidthOverride(640.f)
            [
                SNew(SBorder)
                .BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.85f))
                .Padding(FMargin(8.f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
                    [
                        SAssignNew(OutputText, STextBlock)
                        .Font(Mono)
                        .ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.9f, 0.7f, 1.f)))
                        .Text(FText::GetEmpty())
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SAssignNew(InputBox, SEditableTextBox)
                        .Font(Mono)
                        .HintText(FText::GetEmpty())
                        .OnTextCommitted(FOnTextCommitted::CreateUObject(this, &UPTCheatConsoleSubsystem::OnTextCommitted))
                    ]
                ]
            ]
        ];

    GEngine->GameViewport->AddViewportWidgetContent(Root.ToSharedRef(), 100000); // por encima de todo
    bOpen = true;

    // Para poder tipear hace falta input UI: GameAndUI + cursor + foco en la caja. Guardamos el estado del
    // cursor para restaurarlo al cerrar.
    if (APlayerController* PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr)
    {
        bPrevCursor = PC->bShowMouseCursor;
        PC->SetShowMouseCursor(true);
        FInputModeGameAndUI Mode;
        Mode.SetWidgetToFocus(InputBox);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(Mode);
    }
    if (InputBox.IsValid())
        FSlateApplication::Get().SetKeyboardFocus(InputBox, EFocusCause::SetDirectly);

    SetOutput(bUnlocked ? TEXT("dev: ON") : FString());
}

void UPTCheatConsoleSubsystem::Close()
{
    if (!bOpen) return;
    bOpen = false;

    if (GEngine && GEngine->GameViewport && Root.IsValid())
        GEngine->GameViewport->RemoveViewportWidgetContent(Root.ToSharedRef());
    Root.Reset(); InputBox.Reset(); OutputText.Reset();

    // Restaurar el input: cursor como estaba; si estaba visible (contexto UI) → GameAndUI, si no → GameOnly.
    if (APlayerController* PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr)
    {
        PC->SetShowMouseCursor(bPrevCursor);
        if (bPrevCursor) { FInputModeGameAndUI M; PC->SetInputMode(M); }
        else             { FInputModeGameOnly M;  PC->SetInputMode(M); }
    }
}

void UPTCheatConsoleSubsystem::OnTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    // Solo actuar con ENTER (ignorar la pérdida de foco: no queremos ejecutar al clickear afuera).
    if (CommitType != ETextCommit::OnEnter) return;

    const FString Line = Text.ToString().TrimStartAndEnd();
    if (Line.IsEmpty()) { Close(); return; } // Enter en vacío = cerrar

    RunLine(Line);

    // Limpiar la caja y volver el foco para seguir tipeando.
    if (InputBox.IsValid())
    {
        InputBox->SetText(FText::GetEmpty());
        FSlateApplication::Get().SetKeyboardFocus(InputBox, EFocusCause::SetDirectly);
    }
}

void UPTCheatConsoleSubsystem::RunLine(const FString& Line)
{
    if (!bUnlocked)
    {
        // Bloqueada: solo el comando maestro desbloquea. Cualquier otra cosa no hace NADA (sin pistas).
        if (Line.Equals(PT_CHEAT_UNLOCK, ESearchCase::IgnoreCase))
        {
            bUnlocked = true;
            SetOutput(TEXT("dev: ON"));
        }
        else
        {
            SetOutput(FString()); // silencio
        }
        return;
    }

    // Desbloqueada: reenviar al PlayerController (ahí corren los exec dev: PTSolo, PTSpectate, etc.).
    APlayerController* PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
    if (!PC) { SetOutput(TEXT("(sin PlayerController)")); return; }
    const FString Result = PC->ConsoleCommand(Line, /*bWriteToLog=*/true);
    SetOutput(Result.IsEmpty() ? FString::Printf(TEXT("> %s"), *Line) : Result);
}

void UPTCheatConsoleSubsystem::SetOutput(const FString& Msg)
{
    if (OutputText.IsValid())
        OutputText->SetText(FText::FromString(Msg));
}
