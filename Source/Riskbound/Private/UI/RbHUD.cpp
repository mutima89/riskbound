#include "RbHUD.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "RbCharacter.h"
#include "RbGameSubsystem.h"
#include "RbInteractionComponent.h"
#include "RbSaveSubsystem.h"
#include "RbScreens.h"
#include "RbSelfTest.h"
#include "SRbAssessmentForm.h"
#include "SRbHudOverlay.h"
#include "SRbScreen.h"
#include "SRbToolHub.h"
#include "SRbCaseFiles.h"
#include "SRbInbox.h"
#include "RbAudioSubsystem.h"
#include "RbConvert.h"
#include "HAL/PlatformTime.h"
#include "RbCampaignSubsystem.h"
#include "Widgets/Layout/SBox.h"
#include "RbToolsSubsystem.h"
#include "RbPerson.h"
#include "RbValtrenHQ.h"
#include "EngineUtils.h"

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
	SetTickableWhenPaused(true); // the menu camera keeps drifting while the menu pauses the world
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
			if (!WeakThis.IsValid()) return FText::GetEmpty();
			const FString Clock = WeakThis->GetCrisisClockText();
			const FString Objective = WeakThis->Game()->GetCurrentObjectiveText();
			return FText::FromString(Clock.IsEmpty() ? Objective : Clock + TEXT("   |   ") + Objective);
		})
		.Prompt_Lambda([WeakThis]()
		{
			const APlayerController* PC = WeakThis.IsValid() ? WeakThis->PlayerOwner.Get() : nullptr;
			const ARbCharacter* Pawn = PC ? Cast<ARbCharacter>(PC->GetPawn()) : nullptr;
			return Pawn ? Pawn->GetInteraction()->GetPromptText() : FText::GetEmpty();
		})
		.Status_Lambda([WeakThis]()
		{
			const URbCampaignSubsystem* Campaign = WeakThis.IsValid() ? WeakThis->GetGameInstance()->GetSubsystem<URbCampaignSubsystem>() : nullptr;
			const FRbWorldView W = Campaign ? Campaign->GetWorldView() : FRbWorldView();
			if (!W.bValid) return FText::GetEmpty();
			FNumberFormattingOptions One;
			One.MinimumFractionalDigits = 1;
			One.MaximumFractionalDigits = 1;
			const FString Arrow = W.ShareChange > 0.05 ? TEXT("\u25B2") : (W.ShareChange < -0.05 ? TEXT("\u25BC") : TEXT("\u25C6"));
			FString Mail = FString::Printf(TEXT("[I]  Inbox %d"), W.Unread);
			if (W.AwaitingReply > 0) Mail += FString::Printf(TEXT(" (%d to reply)"), W.AwaitingReply);
			return FText::Format(LOCTEXT("HudStatus", "{0}      Stress {1}      VLT {2} {3}      Week {4}"), FText::FromString(Mail), W.Stress,
				FText::AsNumber(W.ShareIndex, &One), FText::FromString(Arrow), W.Week);
		})
		.StressLevel_Lambda([WeakThis]()
		{
			const URbCampaignSubsystem* Campaign = WeakThis.IsValid() ? WeakThis->GetGameInstance()->GetSubsystem<URbCampaignSubsystem>() : nullptr;
			const FRbWorldView W = Campaign ? Campaign->GetWorldView() : FRbWorldView();
			return W.bValid ? W.Stress / 100.0f : 0.0f;
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
	ReleaseSpeaker();
	if (bMainMenuOpen) StopMenuCamera();
	bMainMenuOpen = false;
	SetGameInput();
}

void ARbHUD::StartMenuCamera()
{
	if (!MenuCamera)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MenuCamera = GetWorld()->SpawnActor<ACameraActor>(FVector(1200.0f, -60.0f, 165.0f), FRotator(-4.0f, 18.0f, 0.0f), Params);
		if (MenuCamera)
		{
			MenuCamera->GetCameraComponent()->SetFieldOfView(70.0f);
			MenuCamera->GetCameraComponent()->bConstrainAspectRatio = false;
		}
	}
	if (APlayerController* PC = PlayerOwner.Get())
	{
		if (MenuCamera) PC->SetViewTarget(MenuCamera);
	}
}

void ARbHUD::StopMenuCamera()
{
	if (APlayerController* PC = PlayerOwner.Get())
	{
		if (APawn* Pawn = PC->GetPawn()) PC->SetViewTarget(Pawn);
	}
}

void ARbHUD::Tick(float DeltaSeconds)
{
	if (CeremonyRoot.IsValid() && FPlatformTime::Seconds() > CeremonyEnds)
	{
		EndCeremony();
		if (bEndingPending)
		{
			// The career ends with its epilogue, the review and the certificate.
			bEndingPending = false;
			OpenReview(true, [this]() { CloseScreen(); });
		}
	}
	if (!IsScreenOpen() && Game() && Game()->HasGame())
	{
		if (URbCampaignSubsystem* C = GetGameInstance()->GetSubsystem<URbCampaignSubsystem>())
		{
			if (C->TakeChallengeOver()) ShowChallengeResults();
		}
	}
	if (!CeremonyRoot.IsValid() && !IsScreenOpen() && Game() && Game()->HasGame())
	{
		if (URbCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<URbCampaignSubsystem>())
		{
			const FString Ceremony = Campaign->TakePendingCeremony();
			if (Ceremony == TEXT("CRO"))
			{
				PlayCeremony(TEXT("Chief Risk Officer"), TEXT("From Northgate to the boardroom. Valtren's risk is yours to lead."));
				bEndingPending = true;
				const rb::EndingDef* E = Campaign->GetEnding();
				Campaign->RecordOnLeaderboard(TEXT("career"), E ? RbConvert::ToFString(E->Title) : FString(), Campaign->GetReview().AverageScore);
			}
			else if (!Ceremony.IsEmpty()) PlayCeremony(Ceremony, TEXT("Promoted on the evidence of your work"));
		}
	}
	if (!bMainMenuOpen && Game() && Game()->HasGame() && !RbSelfTest::IsRequested())
	{
		TickCrisisClock();
		TickApproach();
	}
	Super::Tick(DeltaSeconds);
	if (!bMainMenuOpen || !MenuCamera || RbUI::IsReducedMotion()) return;
	// A slow dolly along the glass corridor, easing at both ends: 70 s per pass.
	MenuCameraTime += FApp::GetDeltaTime();
	const double Phase = FMath::Fmod(MenuCameraTime / 70.0, 2.0);
	const double T = Phase < 1.0 ? Phase : 2.0 - Phase;
	const double Eased = 0.5 - 0.5 * FMath::Cos(T * PI);
	const float X = FMath::Lerp(500.0f, 3500.0f, static_cast<float>(Eased));
	const float Yaw = FMath::Lerp(24.0f, 8.0f, static_cast<float>(Eased));
	MenuCamera->SetActorLocationAndRotation(FVector(X, -60.0f, 165.0f), FRotator(-4.0f, Yaw, 0.0f));
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

void ARbHUD::ClearNotices()
{
	if (Overlay.IsValid())
	{
		Overlay->ClearNotices();
	}
}

void ARbHUD::OnSettingsChanged()
{
	BuildOverlay();
}

// ---------------- Menus ----------------

void ARbHUD::ShowMainMenu()
{
	// The world keeps running behind the main menu so the backdrop camera can drift.
	const FRbScreenParts Parts = RbScreens::MainMenu(this);
	ShowScreen(Parts.Widget, Parts.Focus, nullptr, false);
	bMainMenuOpen = true;
	StartMenuCamera();
}

void ARbHUD::ShowDifficultyPicker()
{
	const FRbScreenParts Parts = RbScreens::DifficultyPicker(this);
	ShowScreen(Parts.Widget, Parts.Focus, [this]() { ShowMainMenu(); }, false);
	bMainMenuOpen = true;
}

void ARbHUD::ShowBackgroundPicker(ERbDifficulty Difficulty)
{
	const FRbScreenParts Parts = RbScreens::BackgroundPicker(this, Difficulty);
	ShowScreen(Parts.Widget, Parts.Focus, [this]() { ShowDifficultyPicker(); }, true);
}

void ARbHUD::ShowChallenges()
{
	const FRbScreenParts Parts = RbScreens::Challenges(this);
	ShowScreen(Parts.Widget, Parts.Focus, [this]() { ShowMainMenu(); }, true);
}

void ARbHUD::StartChallenge(int32 Mode)
{
	if (URbCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<URbCampaignSubsystem>())
	{
		Campaign->StartChallengeGame(Mode == 1 ? rb::EGameMode::Survival : rb::EGameMode::Daily);
	}
	Draft = MakeShared<FRbAssessmentDraft>();
	bMainMenuOpen = false;
	StopMenuCamera();
	CloseScreen();
	OpenCaseFiles();
	Notice(Mode == 1 ? TEXT("Survival: one crisis after another. Three failures and it is over.") : TEXT("Today's daily cases: the same three for everyone today."));
}

void ARbHUD::ShowChallengeResults()
{
	const FRbScreenParts Parts = RbScreens::ChallengeResults(this);
	ShowScreen(Parts.Widget, Parts.Focus, [this]() { CloseScreen(); ShowMainMenu(); }, true);
}

void ARbHUD::OpenAiChat(const FString& PersonId, const FString& Status)
{
	const FRbScreenParts Parts = RbScreens::AiChat(this, PersonId, Status);
	ShowScreen(Parts.Widget, Parts.Focus, [this]() { CloseScreen(); OpenJournal(); }, false);
}

void ARbHUD::OpenGlossary(TFunction<void()> OnBack)
{
	const FRbScreenParts Parts = RbScreens::Glossary(this, OnBack);
	ShowScreen(Parts.Widget, Parts.Focus, OnBack, false);
}

void ARbHUD::OpenReview(bool bEnding, TFunction<void()> OnBack)
{
	const FRbScreenParts Parts = RbScreens::Review(this, bEnding, OnBack);
	ShowScreen(Parts.Widget, Parts.Focus, OnBack, false);
}

void ARbHUD::TogglePhotoMode()
{
	if (IsScreenOpen() && !bPhotoMode) return;
	bPhotoMode = !bPhotoMode;
	if (OverlayRoot.IsValid()) OverlayRoot->SetVisibility(bPhotoMode ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
	if (!bPhotoMode) Notice(TEXT("Photo mode off."));
	else if (OverlayRoot.IsValid()) UE_LOG(LogTemp, Display, TEXT("Riskbound: photo mode on"));
}

void ARbHUD::OpenSkills(TFunction<void()> OnBack)
{
	const FRbScreenParts Parts = RbScreens::Skills(this, OnBack);
	ShowScreen(Parts.Widget, Parts.Focus, OnBack, false);
}

void ARbHUD::PlayConversation(const FRbConversationView& View, TFunction<void()> OnEnd)
{
	if (View.bFound && !View.PersonId.IsEmpty())
	{
		// The person says the line and shows how they took the player's last answer.
		Speak(View.PersonId, View.bEnded ? FString() : View.Line, View.Reaction);
	}
	if (!View.bFound || View.bEnded)
	{
		if (OnEnd) OnEnd();
		else CloseScreen();
		return;
	}
	const FRbScreenParts Parts = RbScreens::Conversation(this, View, OnEnd);
	ShowScreen(Parts.Widget, Parts.Focus, [this, OnEnd]() { if (OnEnd) OnEnd(); else CloseScreen(); }, false);
}

void ARbHUD::PlayCeremony(const FString& Title, const FString& Subtitle)
{
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!Viewport || CeremonyRoot.IsValid()) return;
	CeremonyRoot = RbScreens::CeremonyOverlay(Title, Subtitle);
	Viewport->AddViewportWidgetContent(CeremonyRoot.ToSharedRef(), 25);
	CeremonyEnds = FPlatformTime::Seconds() + 6.5;
	URbAudioSubsystem::Cue(this, ERbCue::Promotion);
	// The camera eases to the head of the boardroom table. With reduced motion it cuts there instead:
	// every promotion keeps its boardroom shot, only the camera movement goes.
	if (APlayerController* PC = PlayerOwner.Get())
	{
		if (!CeremonyCamera)
		{
			CeremonyCamera = GetWorld()->SpawnActor<ACameraActor>(FVector(3820.0f, -700.0f, 165.0f), FRotator(-8.0f, 180.0f, 0.0f));
			if (CeremonyCamera) CeremonyCamera->GetCameraComponent()->SetFieldOfView(62.0f);
		}
		if (CeremonyCamera)
		{
			if (RbUI::IsReducedMotion()) PC->SetViewTarget(CeremonyCamera);
			else PC->SetViewTargetWithBlend(CeremonyCamera, 1.6f, VTBlend_EaseInOut, 2.0f);
		}
	}
}

void ARbHUD::EndCeremony()
{
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		if (CeremonyRoot.IsValid()) Viewport->RemoveViewportWidgetContent(CeremonyRoot.ToSharedRef());
	}
	CeremonyRoot.Reset();
	if (APlayerController* PC = PlayerOwner.Get())
	{
		if (PC->GetPawn() && PC->GetViewTarget() != PC->GetPawn())
		{
			if (RbUI::IsReducedMotion()) PC->SetViewTarget(PC->GetPawn());
			else PC->SetViewTargetWithBlend(PC->GetPawn(), 1.2f, VTBlend_EaseInOut, 2.0f);
		}
	}
}

void ARbHUD::StartNewGame(ERbDifficulty Difficulty, const FString& Background)
{
	Game()->NewGame(Difficulty, TEXT("Analyst"), Background);
	Draft = MakeShared<FRbAssessmentDraft>();
	ResetApproach();
	CloseScreen();
	Notice(TEXT("Day one at Valtren Coldline Logistics. Priya Okafor is expecting you in the manager's office."));
}

ERbLoadOutcome ARbHUD::ContinueGame(FString& OutMessage)
{
	const ERbLoadOutcome Outcome = Game()->Continue(OutMessage);
	if (Outcome == ERbLoadOutcome::Ok || Outcome == ERbLoadOutcome::RecoveredFromBackup)
	{
		Draft = MakeShared<FRbAssessmentDraft>();
		ResetApproach();
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

void ARbHUD::OpenCredits(TFunction<void()> OnBack)
{
	const bool bFromMenu = bMainMenuOpen;
	const FRbScreenParts Parts = RbScreens::Credits(this, OnBack);
	ShowScreen(Parts.Widget, Parts.Focus, OnBack, !bFromMenu);
	bMainMenuOpen = bFromMenu; // keep the menu backdrop camera when opened from the main menu
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
	// After Mission 001, Priya sometimes has something to talk about (Data/Dialogue.json).
	if (Game()->IsDebriefed())
	{
		if (URbCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<URbCampaignSubsystem>())
		{
			const FRbConversationView Talk = Campaign->StartConversation(TEXT("priya"), TEXT("talk"));
			if (Talk.bFound)
			{
				PlayConversation(Talk, nullptr);
				return;
			}
		}
	}
	switch (Game()->TalkToManager())
	{
	case ERbManagerTalk::Briefing:
	{
		Speak(TEXT("priya"), RbScreens::BriefingText(Game()->GetSettings().bHints).ToString());
		const FRbScreenParts Parts = RbScreens::Dialogue(LOCTEXT("Priya", "Priya Okafor, Risk Manager"),
			RbScreens::BriefingText(Game()->GetSettings().bHints), [this]() { CloseScreen(); });
		ShowScreen(Parts.Widget, Parts.Focus, [this]() { CloseScreen(); }, false);
		break;
	}
	case ERbManagerTalk::Reminder:
	{
		const FText Text = FText::Format(LOCTEXT("Reminder", "Take the time you need. Where you are: {0}.\n\nWhen the form is done, submit it from your workstation and come back to me."),
			FText::FromString(Game()->GetCurrentObjectiveText()));
		Speak(TEXT("priya"), Text.ToString());
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

void ARbHUD::OpenToolHub(int32 Tab)
{
	TSharedRef<SRbToolHub> Hub = SNew(SRbToolHub)
		.Tools(GetGameInstance()->GetSubsystem<URbToolsSubsystem>())
		.OnClose([this]() { OpenWorkstation(); });
	Hub->ShowTab(static_cast<SRbToolHub::ETab>(FMath::Clamp(Tab, 0, static_cast<int32>(SRbToolHub::ETab::Count) - 1)));
	ShowScreen(RbUI::MakeModalFrame(SNew(SBox).MinDesiredWidth(1360.0f).MinDesiredHeight(780.0f)[Hub], 1500.0f), Hub->GetInitialFocus(), [this]() { OpenWorkstation(); }, false);
}

TSharedPtr<SRbCaseFiles> ARbHUD::OpenCaseFiles(const FString& MissionId, bool bDebrief, const FString& Where)
{
	TSharedRef<SRbCaseFiles> Files = SNew(SRbCaseFiles)
		.Campaign(GetGameInstance()->GetSubsystem<URbCampaignSubsystem>())
		.Game(Game())
		.Where(Where)
		.OnMissionStarted([this]() { ClearNotices(); }) // earlier mission toasts no longer apply
		.OnClose([this]() { OpenWorkstation(); });
	ClearNotices();
	if (!MissionId.IsEmpty())
	{
		if (bDebrief) Files->ShowDebrief(MissionId);
		else Files->ShowMission(MissionId);
	}
	ShowScreen(RbUI::MakeModalFrame(SNew(SBox).MinDesiredWidth(1100.0f).MinDesiredHeight(780.0f)[Files], 1200.0f), Files->GetInitialFocus(), [this]() { OpenWorkstation(); }, false);
	return Files;
}

TSharedPtr<SRbInbox> ARbHUD::OpenInbox(const FString& MessageId)
{
	if (!Game()->HasGame()) return nullptr;
	const bool bFromGameplay = !IsScreenOpen();
	auto Back = [this, bFromGameplay]()
	{
		if (bFromGameplay) CloseScreen();
		else OpenWorkstation();
	};
	TSharedRef<SRbInbox> Inbox = SNew(SRbInbox)
		.Campaign(GetGameInstance()->GetSubsystem<URbCampaignSubsystem>())
		.OpenMessageId(MessageId)
		.OnClose(Back);
	ClearNotices();
	ShowScreen(RbUI::MakeModalFrame(SNew(SBox).MinDesiredWidth(1100.0f).MinDesiredHeight(760.0f)[Inbox], 1200.0f), Inbox->GetInitialFocus(), Back, false);
	return Inbox;
}

void ARbHUD::ExamineInPerson(const FString& MissionId, const FString& EvidenceId, const FString& Where)
{
	URbCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<URbCampaignSubsystem>();
	if (!Campaign) return;
	const ERbMissionResult R = Campaign->ViewEvidence(MissionId, EvidenceId, Where);
	if (R != ERbMissionResult::Ok)
	{
		Notice(URbCampaignSubsystem::ResultText(R));
		return;
	}
	const FRbMissionView M = Campaign->GetMission(MissionId);
	const FRbMissionEvidenceView* E = M.Evidence.FindByPredicate([&](const FRbMissionEvidenceView& V) { return V.Id == EvidenceId; });
	if (!E) return;
	auto Back = [this]() { CloseScreen(); };
	if (!E->PersonName.IsEmpty())
	{
		const FText SpeakerName = FText::FromString(E->PersonName);
		const FText Text = FText::FromString(E->Title + TEXT("\n\n") + E->Body);
		const FString PersonId = E->PersonId, Said = E->Body;
		auto ShowNotes = [this, SpeakerName, Text, Back, PersonId, Said]()
		{
			Speak(PersonId, Said);
			const FRbScreenParts Parts = RbScreens::Dialogue(SpeakerName, Text, Back);
			ShowScreen(Parts.Widget, Parts.Focus, Back, false);
		};
		// An interview starts as a conversation when there is one written for it, then the notes.
		const rb::MissionDef* Def = Campaign->GetCatalogue().Find(RbConvert::ToStd(MissionId));
		const rb::MissionEvidenceDef* Ev = nullptr;
		for (const rb::MissionEvidenceDef& X : Def ? Def->Evidence : std::vector<rb::MissionEvidenceDef>())
		{
			if (RbConvert::ToFString(X.Id) == EvidenceId) Ev = &X;
		}
		const FRbConversationView Talk = Ev ? Campaign->StartConversation(RbConvert::ToFString(Ev->Person), FString::Printf(TEXT("evidence:%s/%s"), *MissionId, *EvidenceId)) : FRbConversationView();
		if (Talk.bFound) PlayConversation(Talk, ShowNotes);
		else ShowNotes();
	}
	else
	{
		const FRbScreenParts Parts = RbScreens::Document(FText::FromString(E->Title), FText::FromString(FString::Printf(TEXT("Case %s  |  %s"), *M.Id, *E->AtName)),
			FText::FromString(E->Body), Back);
		ShowScreen(Parts.Widget, Parts.Focus, Back, false);
	}
	Notice(TEXT("Added to the case file."));
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

// ---------------- People, colleagues who come to you, crisis clock ----------------

ARbValtrenHQ* ARbHUD::Office() const
{
	for (TActorIterator<ARbValtrenHQ> It(GetWorld()); It; ++It) return *It;
	return nullptr;
}

void ARbHUD::Speak(const FString& PersonId, const FString& Line, int32 Reaction)
{
	URbPersonComponent* P = PersonId.IsEmpty() ? nullptr : URbPersonComponent::Find(GetWorld(), PersonId);
	if (Speaker.IsValid() && Speaker.Get() != P) ReleaseSpeaker();
	if (!P) return;
	Speaker = P;
	P->SetEngaged(true);
	if (!Line.IsEmpty()) P->Say(Line);
	if (Reaction != 0) P->React(Reaction);
}

void ARbHUD::ReleaseSpeaker()
{
	if (URbPersonComponent* P = Speaker.Get())
	{
		P->StopTalking();
		P->SetEngaged(false);
	}
	Speaker.Reset();
}

void ARbHUD::ResetApproach()
{
	KnownMessages.Reset();
	bKnownMessages = false;
	bApproaching = false;
	ClockMission.Reset();
	ClockEnds = 0.0;
	bClockExpired = bClockWarned = bOpenCallPending = false;
	if (ARbValtrenHQ* HQ = Office()) HQ->DismissColleague(true);
}

void ARbHUD::TickApproach()
{
	const double Now = FPlatformTime::Seconds();
	if (bApproaching)
	{
		// Someone who cannot reach the player (blocked, or the player keeps walking off) gives up.
		if (Now > ApproachTimeout)
		{
			bApproaching = false;
			if (ARbValtrenHQ* HQ = Office()) HQ->DismissColleague(false);
		}
		return;
	}
	if (Now < NextApproachCheck || IsScreenOpen() || CeremonyRoot.IsValid()) return;
	NextApproachCheck = Now + 1.0;
	URbCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<URbCampaignSubsystem>();
	if (!Campaign) return;
	const TArray<FRbInboxMessageView> Inbox = Campaign->GetInbox();
	if (!bKnownMessages)
	{
		// Only messages that arrive from now on are delivered in person.
		for (const FRbInboxMessageView& M : Inbox) KnownMessages.Add(M.Id);
		bKnownMessages = true;
		return;
	}
	const FRbInboxMessageView* Pick = nullptr;
	for (const FRbInboxMessageView& M : Inbox)
	{
		if (KnownMessages.Contains(M.Id)) continue;
		KnownMessages.Add(M.Id);
		// Office colleagues walk over; the regulator, the press and home write.
		const bool bOffice = (M.Category == TEXT("Team") || M.Category == TEXT("Politics") || M.Category == TEXT("Story"))
			&& M.FromId != TEXT("jonah") && M.FromId != TEXT("imogen") && M.FromId != TEXT("jordan") && !M.FromId.IsEmpty();
		if (!Pick && bOffice && !M.bRead) Pick = &M;
	}
	ARbValtrenHQ* HQ = Pick ? Office() : nullptr;
	if (!HQ) return;
	const FRbInboxMessageView Msg = *Pick;
	TWeakObjectPtr<ARbHUD> WeakThis(this);
	auto Arrive = [WeakThis, Msg](URbPersonComponent* Person)
	{
		ARbHUD* Hud = WeakThis.Get();
		if (!Hud) return;
		Hud->bApproaching = false;
		if (Hud->IsScreenOpen() || Hud->CeremonyRoot.IsValid())
		{
			// The player is busy: a word and they leave it in the inbox.
			Person->Say(TEXT("You're busy. It's in your inbox."));
			FTimerHandle H;
			Hud->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(Hud, [Hud]() { if (ARbValtrenHQ* O = Hud->Office()) O->DismissColleague(false); }), 2.5f, false);
			return;
		}
		FString First = Msg.Body;
		int32 Cut = INDEX_NONE;
		if (First.FindChar(TEXT('\n'), Cut)) First = First.Left(Cut);
		if (First.Len() > 420) First = First.Left(417) + TEXT("...");
		// Scripts/BuildVoices.py (approach_line) records this exact sentence for each colleague's message.
		const bool bEndsSentence = Msg.Subject.EndsWith(TEXT(".")) || Msg.Subject.EndsWith(TEXT("?")) || Msg.Subject.EndsWith(TEXT("!"));
		const FString Spoken = FString::Printf(TEXT("Have you got a minute? %s%s"), *Msg.Subject, bEndsSentence ? TEXT("") : TEXT("."));
		Hud->Speak(Msg.FromId, Spoken);
		const FText Who = FText::FromString(Msg.FromRole.IsEmpty() ? Msg.FromName : Msg.FromName + TEXT(", ") + Msg.FromRole);
		const FText Text = FText::FromString(TEXT("\u201C") + Spoken + TEXT("\u201D\n\n") + First);
		const FString Id = Msg.Id;
		auto Later = [WeakThis]()
		{
			if (!WeakThis.IsValid()) return;
			WeakThis->CloseScreen();
			if (ARbValtrenHQ* O = WeakThis->Office()) O->DismissColleague(false);
		};
		auto Read = [WeakThis, Id]()
		{
			if (!WeakThis.IsValid()) return;
			WeakThis->CloseScreen();
			if (ARbValtrenHQ* O = WeakThis->Office()) O->DismissColleague(false);
			WeakThis->OpenInbox(Id);
		};
		const FRbScreenParts Parts = RbScreens::Approach(Who, Text, Read, Later);
		Hud->ShowScreen(Parts.Widget, Parts.Focus, Later, false);
	};
	if (HQ->SendColleague(Msg.FromId, Arrive))
	{
		bApproaching = true;
		ApproachTimeout = Now + 40.0;
	}
}

FString ARbHUD::GetCrisisClockText() const
{
	if (ClockEnds <= 0.0 || bClockExpired || !GetWorld()) return FString();
	const int32 Left = FMath::Max(0, FMath::CeilToInt(ClockEnds - GetWorld()->GetTimeSeconds()));
	return FString::Printf(TEXT("Board call in %d:%02d"), Left / 60, Left % 60);
}

void ARbHUD::TickCrisisClock()
{
	URbCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<URbCampaignSubsystem>();
	if (!Campaign) return;
	const FString Active = Campaign->GetActiveMissionId();
	const double Now = GetWorld()->GetTimeSeconds();
	if (Active != ClockMission)
	{
		ClockMission = Active;
		ClockEnds = 0.0;
		bClockWarned = bClockExpired = bOpenCallPending = false;
		if (Active.IsEmpty() || Campaign->GetMission(Active).Kind != TEXT("Crisis")) return;
		if (ARbValtrenHQ* HQ = Office()) HQ->StartAlarm(12.0f);
		URbAudioSubsystem::Cue(this, ERbCue::Incident);
		// Analyst difficulty keeps the alarm but not the clock.
		const ERbDifficulty D = Game()->GetDifficulty();
		const double Minutes = D == ERbDifficulty::Executive ? 6.0 : (D == ERbDifficulty::Professional ? 10.0 : 0.0);
		if (Minutes <= 0.0 || Campaign->IsBoardCallStarted(Active)) return;
		ClockEnds = Now + Minutes * 60.0;
		Notice(FString::Printf(TEXT("Crisis. The board call is in %d minutes; evidence you have not seen by then is out of reach."), FMath::RoundToInt(Minutes)));
		return;
	}
	if (bOpenCallPending && !IsScreenOpen() && !CeremonyRoot.IsValid())
	{
		bOpenCallPending = false;
		OpenCaseFiles(ClockMission);
	}
	if (ClockEnds <= 0.0 || bClockExpired) return;
	const double Left = ClockEnds - Now;
	if (Left < 60.0 && !bClockWarned)
	{
		bClockWarned = true;
		Notice(TEXT("One minute to the board call."));
		URbAudioSubsystem::Cue(this, ERbCue::Notice);
	}
	if (Left <= 0.0)
	{
		bClockExpired = true;
		Campaign->MarkBoardCallStarted(ClockMission);
		Notice(TEXT("The board call has started. Decide with what you have."));
		URbAudioSubsystem::Cue(this, ERbCue::Incident);
		bOpenCallPending = true;
	}
}

#undef LOCTEXT_NAMESPACE
