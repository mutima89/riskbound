#include "RbHUD.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "RbCharacter.h"
#include "RbGameSubsystem.h"
#include "RbInteractionComponent.h"
#include "RbSaveSubsystem.h"
#include "RbScreens.h"
#include "RbSelfTest.h"
#include "SRbAssessmentForm.h"
#include "SRbHudOverlay.h"
#include "SRbScreen.h"

#define LOCTEXT_NAMESPACE "Riskbound"

URbGameSubsystem* ARbHUD::Game() const
{
	return GetGameInstance()->GetSubsystem<URbGameSubsystem>();
}

URbSaveSubsystem* ARbHUD::Saves() const
{
	return GetGameInstance()->GetSubsystem<URbSaveSubsystem>();
}

void ARbHUD::BeginPlay()
{
	Super::BeginPlay();
	Draft = MakeShared<FRbAssessmentDraft>();
	Game()->OnNotice.AddDynamic(this, &ARbHUD::HandleNotice);
	Game()->ApplySettings(false);
	BuildOverlay();
	if (!RbSelfTest::IsRequested())
	{
		ShowMainMenu();
	}
}

void ARbHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URbGameSubsystem* G = Game())
	{
		G->OnNotice.RemoveDynamic(this, &ARbHUD::HandleNotice);
	}
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		if (Screen.IsValid()) Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
		if (OverlayRoot.IsValid()) Viewport->RemoveViewportWidgetContent(OverlayRoot.ToSharedRef());
	}
	Screen.Reset();
	OverlayRoot.Reset();
	Overlay.Reset();
	Super::EndPlay(EndPlayReason);
}

void ARbHUD::BuildOverlay()
{
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!Viewport) return;
	if (OverlayRoot.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(OverlayRoot.ToSharedRef());
	}
	TWeakObjectPtr<ARbHUD> WeakThis(this);
	TSharedRef<SRbHudOverlay> NewOverlay = SNew(SRbHudOverlay)
		.Objective_Lambda([WeakThis]()
		{
			return WeakThis.IsValid() ? FText::FromString(WeakThis->Game()->GetCurrentObjectiveText()) : FText::GetEmpty();
		})
		.Prompt_Lambda([WeakThis]()
		{
			const APlayerController* PC = WeakThis.IsValid() ? WeakThis->PlayerOwner.Get() : nullptr;
			const ARbCharacter* Pawn = PC ? Cast<ARbCharacter>(PC->GetPawn()) : nullptr;
			return Pawn ? Pawn->GetInteraction()->GetPromptText() : FText::GetEmpty();
		})
		.ShowGameplay_Lambda([WeakThis]()
		{
			return WeakThis.IsValid() && WeakThis->Game()->HasGame() && !WeakThis->IsScreenOpen();
		});
	Overlay = NewOverlay;
	OverlayRoot = NewOverlay;
	Viewport->AddViewportWidgetContent(NewOverlay, 30);
}

void ARbHUD::ShowScreen(TSharedRef<SWidget> Content, TSharedPtr<SWidget> Focus, TFunction<void()> OnBack, bool bPauseGame)
{
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (Viewport && Screen.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
	}
	TSharedRef<SRbScreen> Root = SNew(SRbScreen).OnBack(MoveTemp(OnBack))[Content];
	Screen = Root;
	if (Viewport)
	{
		Viewport->AddViewportWidgetContent(Root, 20);
	}
	if (APlayerController* PC = PlayerOwner.Get())
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Focus.IsValid() ? Focus : TSharedPtr<SWidget>(Root));
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
		PC->FlushPressedKeys();
		PC->SetPause(bPauseGame);
	}
}

void ARbHUD::CloseScreen()
{
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		if (Screen.IsValid()) Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
	}
	Screen.Reset();
	bMainMenuOpen = false;
	SetGameInput();
}

void ARbHUD::SetGameInput()
{
	if (APlayerController* PC = PlayerOwner.Get())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
		PC->FlushPressedKeys();
		PC->SetPause(false);
	}
}

void ARbHUD::HandleNotice(const FString& Message)
{
	Notice(Message);
}

void ARbHUD::Notice(const FString& Message)
{
	if (Overlay.IsValid())
	{
		Overlay->AddNotice(Message);
	}
}

void ARbHUD::OnSettingsChanged()
{
	BuildOverlay();
}

// ---------------- Menus ----------------

void ARbHUD::ShowMainMenu()
{
	const FRbScreenParts Parts = RbScreens::MainMenu(this);
	ShowScreen(Parts.Widget, Parts.Focus, nullptr, true);
	bMainMenuOpen = true;
}

void ARbHUD::ShowDifficultyPicker()
{
	const FRbScreenParts Parts = RbScreens::DifficultyPicker(this);
	ShowScreen(Parts.Widget, Parts.Focus, [this]() { ShowMainMenu(); }, true);
	bMainMenuOpen = true;
}

void ARbHUD::StartNewGame(ERbDifficulty Difficulty)
{
	Game()->NewGame(Difficulty);
	Draft = MakeShared<FRbAssessmentDraft>();
	CloseScreen();
	Notice(TEXT("Day one at Valtren Coldline Logistics. Priya Okafor is expecting you in the manager's office."));
}

ERbLoadOutcome ARbHUD::ContinueGame(FString& OutMessage)
{
	const ERbLoadOutcome Outcome = Game()->Continue(OutMessage);
	if (Outcome == ERbLoadOutcome::Ok || Outcome == ERbLoadOutcome::RecoveredFromBackup)
	{
		Draft = MakeShared<FRbAssessmentDraft>();
		CloseScreen();
		BuildOverlay(); // loaded settings may change text size
		if (Outcome == ERbLoadOutcome::Ok) Notice(TEXT("Welcome back."));
		// RecoveredFromBackup already raised its own notice through the subsystem.
		MenuMessage = FText::GetEmpty();
	}
	else
	{
		MenuMessage = FText::FromString(OutMessage);
	}
	return Outcome;
}

void ARbHUD::OpenPauseMenu()
{
	if (IsScreenOpen() || !Game()->HasGame()) return;
	const FRbScreenParts Parts = RbScreens::PauseMenu(this);
	ShowScreen(Parts.Widget, Parts.Focus, [this]() { CloseScreen(); }, true);
}

void ARbHUD::OpenSettings(TFunction<void()> OnBack)
{
	TFunction<void()> Back = [this, OnBack]()
	{
		OnSettingsChanged();
		OnBack();
	};
	const FRbScreenParts Parts = RbScreens::Settings(this, Back);
	ShowScreen(Parts.Widget, Parts.Focus, Back, true);
}

void ARbHUD::OpenJournal()
{
	if (IsScreenOpen() || !Game()->HasGame()) return;
	const FRbScreenParts Parts = RbScreens::Journal(this);
	ShowScreen(Parts.Widget, Parts.Focus, [this]() { CloseScreen(); }, false);
}

bool ARbHUD::SaveNow(FString& OutMessage)
{
	FString Error;
	const bool bOk = Saves()->Save(Error);
	OutMessage = bOk ? FString(TEXT("Game saved.")) : FString::Printf(TEXT("Save failed: %s"), *Error);
	Notice(OutMessage);
	return bOk;
}

void ARbHUD::QuitToDesktop()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), PlayerOwner.Get(), EQuitPreference::Quit, false);
}

// ---------------- Mission flow ----------------

void ARbHUD::TalkToManager()
{
	switch (Game()->TalkToManager())
	{
	case ERbManagerTalk::Briefing:
	{
		const FRbScreenParts Parts = RbScreens::Dialogue(LOCTEXT("Priya", "Priya Okafor, Risk Manager"),
			RbScreens::BriefingText(Game()->GetSettings().bHints), [this]() { CloseScreen(); });
		ShowScreen(Parts.Widget, Parts.Focus, [this]() { CloseScreen(); }, false);
		break;
	}
	case ERbManagerTalk::Reminder:
	{
		const FText Text = FText::Format(LOCTEXT("Reminder", "Take the time you need. Where you are: {0}.\n\nWhen the form is done, submit it from your workstation and come back to me."),
			FText::FromString(Game()->GetCurrentObjectiveText()));
		const FRbScreenParts Parts = RbScreens::Dialogue(LOCTEXT("Priya", "Priya Okafor, Risk Manager"), Text, [this]() { CloseScreen(); });
		ShowScreen(Parts.Widget, Parts.Focus, [this]() { CloseScreen(); }, false);
		break;
	}
	case ERbManagerTalk::Debrief:
		OpenDebrief();
		Game()->MarkDebriefReceived();
		break;
	}
}

void ARbHUD::OpenEvidence(const FString& EvidenceId, TFunction<void()> OnBack)
{
	if (!Game()->ViewEvidence(EvidenceId))
	{
		Notice(FString::Printf(TEXT("Nothing useful here (%s)."), *EvidenceId));
		return;
	}
	const FRbEvidenceView View = Game()->GetEvidenceItem(EvidenceId);
	FString Body = View.Body;
	if (EvidenceId == TEXT("risk_register_R014"))
	{
		Body += TEXT("\n\n") + RbScreens::RegisterEntryText(Game()->GetRegisterEntry(TEXT("R-014"))).ToString();
	}
	TFunction<void()> Back = OnBack ? OnBack : TFunction<void()>([this]() { CloseScreen(); });
	const FRbScreenParts Parts = RbScreens::Document(FText::FromString(View.Title), FText::FromString(View.Location), FText::FromString(Body), Back);
	ShowScreen(Parts.Widget, Parts.Focus, Back, false);
}

void ARbHUD::OpenWorkstation()
{
	const FRbScreenParts Parts = RbScreens::Workstation(this);
	ShowScreen(Parts.Widget, Parts.Focus, [this]() { CloseScreen(); }, false);
}

void ARbHUD::OpenAssessment()
{
	if (Game()->IsSubmitted())
	{
		Notice(TEXT("The assessment is submitted. Priya Okafor has your debrief."));
		return;
	}
	TSharedRef<SRbAssessmentForm> Form = SNew(SRbAssessmentForm)
		.Game(Game())
		.Draft(Draft)
		.OnSubmit([this](const FRbSubmissionInput& Input) { return SubmitAssessment(Input); })
		.OnClose([this]() { OpenWorkstation(); });
	ShowScreen(RbUI::MakeModalFrame(Form, 1100.0f), Form->GetInitialFocus(), [this]() { OpenWorkstation(); }, false);
}

ERbSubmitResult ARbHUD::SubmitAssessment(const FRbSubmissionInput& Input)
{
	const ERbSubmitResult Result = Game()->SubmitAssessment(Input);
	if (Result == ERbSubmitResult::Ok)
	{
		CloseScreen();
		Notice(TEXT("Assessment submitted. Priya Okafor is waiting in her office for your debrief."));
	}
	return Result;
}

void ARbHUD::OpenDebrief()
{
	const FRbScreenParts Parts = RbScreens::Debrief(Game()->GetDebrief(), Game()->GetCareer(), Game()->GetAchievementTitles(), [this]() { CloseScreen(); });
	ShowScreen(Parts.Widget, Parts.Focus, [this]() { CloseScreen(); }, false);
}

#undef LOCTEXT_NAMESPACE
