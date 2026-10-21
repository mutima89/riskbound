#include "RbSelfTest.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RbCampaignSubsystem.h"
#include "RbAudioSubsystem.h"
#include "RbConvert.h"
#include "RbGameSubsystem.h"
#include "RbWalkthrough.h"
#include "UI/SRbCaseFiles.h"
#include "RbHUD.h"
#include "RbInteractable.h"
#include "RbLog.h"
#include "RbValtrenHQ.h"
#include "RbSaveSubsystem.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "UnrealClient.h"
#include "GameFramework/Pawn.h"

extern ENGINE_API float GAverageFPS;
extern ENGINE_API float GAverageMS;

namespace RbSelfTest
{
const TCHAR* const SlotName = TEXT("selftest");

FString RequestedMode()
{
	FString Mode;
	FParse::Value(FCommandLine::Get(), TEXT("RbSelfTest="), Mode);
	return Mode.ToLower();
}

bool IsRequested()
{
	return !RequestedMode().IsEmpty();
}

FRbSubmissionInput StrongAssessment()
{
	FRbSubmissionInput S;
	S.MissingInfo = {TEXT("compressor_failure_history"), TEXT("after_hours_escalation"), TEXT("alarm_threshold_config")};
	S.InherentLikelihood = 4;
	S.InherentImpact = 4;
	S.ControlRatings = {ERbControlRating::Ineffective, ERbControlRating::PartiallyEffective, ERbControlRating::Ineffective};
	S.ResidualLikelihood = 4;
	S.ResidualImpact = 3;
	S.TreatmentId = TEXT("repair_and_escalation");
	S.ActionOwnerId = TEXT("ops_site_manager");
	S.RationaleTagIds = {TEXT("alarm_ignored"), TEXT("service_overdue"), TEXT("penalty_exposure"), TEXT("outside_appetite")};
	S.RationaleText = TEXT("Controls exist on paper but are not operating; the alarm is silenced without action and servicing is overdue.");
	return S;
}

void PlayMission001(URbGameSubsystem* Game)
{
	Game->TalkToManager();
	for (const FRbEvidenceView& E : Game->GetEvidence())
	{
		Game->ViewEvidence(E.Id);
	}
	Game->SubmitAssessment(StrongAssessment());
	Game->TalkToManager();
	Game->MarkDebriefReceived();
}

// Best answers for a campaign mission, read from the content (the self-test may see the rubric).
TArray<FRbMissionAnswer> BestAnswers(const rb::MissionDef& M)
{
	TArray<FRbMissionAnswer> Out;
	for (const rb::MissionQuestionDef& Q : M.Questions)
	{
		const rb::MissionAnswer Best = rb::BestAnswer(Q);
		FRbMissionAnswer A;
		A.QuestionId = RbConvert::ToFString(Q.Id);
		A.OptionIds = RbConvert::ToFStrings(Best.OptionIds);
		Out.Add(A);
	}
	return Out;
}

// Plays a campaign mission through the case files screen with the best answers.
ERbMissionResult PlayCaseBest(ARbHUD* Hud, const FString& MissionId)
{
	URbCampaignSubsystem* Campaign = Hud->GetGameInstance()->GetSubsystem<URbCampaignSubsystem>();
	const rb::MissionDef* Def = Campaign->GetCatalogue().Find(RbConvert::ToStd(MissionId));
	if (!Def) return ERbMissionResult::UnknownMission;
	TSharedPtr<SRbCaseFiles> Files = Hud->OpenCaseFiles(MissionId); // starts the mission
	for (const FRbMissionEvidenceView& E : Campaign->GetMission(MissionId).Evidence) Campaign->ViewEvidence(MissionId, E.Id, E.At);
	for (const FRbMissionAnswer& A : BestAnswers(*Def))
	{
		Files->SetAnswerValues(A.QuestionId, A.OptionIds);
	}
	return Campaign->SubmitMission(MissionId, Files->CurrentAnswers(), RbConvert::ToFString(Def->PresentAt));
}

namespace
{
struct FReport
{
	TArray<FString> Lines;
	bool bPass = true;

	void Check(const FString& Name, bool bOk, const FString& Detail)
	{
		bPass &= bOk;
		Lines.Add(FString::Printf(TEXT("%s %s: %s"), bOk ? TEXT("ok  ") : TEXT("FAIL"), *Name, *Detail));
	}

	void Write(const FString& Mode, const FString& FileName) const
	{
		FString Text = FString::Printf(TEXT("%s\nmode=%s\n"), bPass ? TEXT("PASS") : TEXT("FAIL"), *Mode);
		for (const FString& L : Lines) Text += L + TEXT("\n");
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FileName);
		FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogRiskbound, Display, TEXT("RbSelfTest wrote %s:\n%s"), *Path, *Text);
	}
};

void CheckFinalState(FReport& R, URbGameSubsystem* Game)
{
	const FRbCareerView Career = Game->GetCareer();
	const FRbRegisterEntryView Entry = Game->GetRegisterEntry(TEXT("R-014"));
	R.Check(TEXT("objective"), Game->IsDebriefed(), Game->GetCurrentObjectiveText());
	R.Check(TEXT("checkpoint"), Game->IsDebriefed(), Game->GetCheckpoint());
	R.Check(TEXT("career_level"), Career.Level == 2, FString::Printf(TEXT("%d %s"), Career.Level, *Career.Title));
	R.Check(TEXT("passed_missions"), Career.PassedMissions.Contains(TEXT("M001")), FString::Join(Career.PassedMissions, TEXT(",")));
	R.Check(TEXT("r014_treatment"), Entry.bFound && Entry.Treatment == TEXT("Reduce"),
		FString::Printf(TEXT("%s (owner %s, status %s)"), *Entry.Treatment, *Entry.ActionOwner, *Entry.Status));
	const FRbDebriefView Debrief = Game->GetDebrief();
	R.Check(TEXT("debrief"), Debrief.bAvailable && Debrief.bPassed, FString::Printf(TEXT("score %d, passed %d"), Debrief.OverallScore, Debrief.bPassed ? 1 : 0));
}

void CheckCampaign(FReport& R, URbGameSubsystem* Game)
{
	URbCampaignSubsystem* Campaign = Game->GetGameInstance()->GetSubsystem<URbCampaignSubsystem>();
	const TArray<FRbMissionSummary> Missions = Campaign->GetMissions(true);
	R.Check(TEXT("mission_data"), Missions.Num() == 52 && Campaign->GetAchievementDefs().size() == 50 && Campaign->GetLoadErrors().IsEmpty(),
		FString::Printf(TEXT("%d missions, %d achievements, %d problems"), Missions.Num(), static_cast<int32>(Campaign->GetAchievementDefs().size()), Campaign->GetLoadErrors().Num()));
	const FRbMissionSummary* M2 = Missions.FindByPredicate([](const FRbMissionSummary& M) { return M.Id == TEXT("M002"); });
	const FRbMissionDebriefView D = Campaign->GetDebrief(TEXT("M002"));
	R.Check(TEXT("m002_passed"), M2 && M2->bPassed && D.bAvailable && D.bPassed, FString::Printf(TEXT("score %d, attempts %d"), D.Score, M2 ? M2->Attempts : 0));
	const FRbMissionSummary* M3 = Missions.FindByPredicate([](const FRbMissionSummary& M) { return M.Id == TEXT("M003"); });
	R.Check(TEXT("m003_open"), M3 && M3->bAvailable, M3 ? FString::Join(M3->Reasons, TEXT(",")) : FString(TEXT("missing")));
	R.Check(TEXT("objective_next"), Game->GetCurrentObjectiveText().Contains(TEXT("Case files")), Game->GetCurrentObjectiveText());
	R.Check(TEXT("campaign_checkpoint"), Game->GetCheckpoint() == TEXT("M002:debriefed"), Game->GetCheckpoint());
	// The living world: content loaded, the first inbox messages delivered and kept across save/load, sound present.
	const FRbWorldView W = Campaign->GetWorldView();
	const TArray<FRbInboxMessageView> Inbox = Campaign->GetInbox();
	R.Check(TEXT("world_content"), Campaign->GetPeople().Num() >= 12 && W.CluesTotal == 10, FString::Printf(TEXT("%d people, %d clues"), Campaign->GetPeople().Num(), W.CluesTotal));
	R.Check(TEXT("inbox"), Inbox.Num() >= 2 && Inbox.ContainsByPredicate([](const FRbInboxMessageView& M) { return M.Subject == TEXT("Welcome to the team"); }),
		FString::Printf(TEXT("%d messages, %d awaiting reply, stress %d, share %.1f"), Inbox.Num(), W.AwaitingReply, W.Stress, W.ShareIndex));
	const URbAudioSubsystem* Audio = Game->GetGameInstance()->GetSubsystem<URbAudioSubsystem>();
	R.Check(TEXT("audio"), Audio && Audio->LoadedSoundCount() >= 20, FString::Printf(TEXT("%d sounds"), Audio ? Audio->LoadedSoundCount() : 0));
}

void RunPlay(FReport& R, ARbHUD* Hud, UWorld* World)
{
	URbGameSubsystem* Game = Hud->Game();
	Hud->StartNewGame(ERbDifficulty::Professional);

	ARbNpc* Npc = nullptr;
	ARbWorkstation* Workstation = nullptr;
	TArray<ARbEvidenceProp*> Props;
	for (TActorIterator<ARbInteractable> It(World); It; ++It)
	{
		if (ARbNpc* N = Cast<ARbNpc>(*It)) Npc = N;
		else if (ARbWorkstation* W = Cast<ARbWorkstation>(*It)) Workstation = W;
		else if (ARbEvidenceProp* P = Cast<ARbEvidenceProp>(*It)) Props.Add(P);
	}
	R.Check(TEXT("level_interactables"), Npc && Workstation && Props.Num() == 5,
		FString::Printf(TEXT("npc %d, workstation %d, evidence props %d"), Npc ? 1 : 0, Workstation ? 1 : 0, Props.Num()));
	if (!Npc || !Workstation) return;

	Npc->Interact(Hud); // briefing: MeetManager + autosave
	Hud->CloseScreen();
	R.Check(TEXT("met_manager"), Game->HasMetManager(), Game->GetCheckpoint());

	Workstation->Interact(Hud);
	Hud->OpenEvidence(TEXT("incident_report")); // the workstation inbox button
	Hud->OpenEvidence(TEXT("risk_register_R014")); // the workstation register button
	for (ARbEvidenceProp* Prop : Props)
	{
		Prop->Interact(Hud);
	}
	Hud->CloseScreen();
	int32 Viewed = 0;
	for (const FRbEvidenceView& E : Game->GetEvidence()) Viewed += E.bViewed ? 1 : 0;
	R.Check(TEXT("evidence_viewed"), Viewed == 7, FString::Printf(TEXT("%d of 7"), Viewed));

	Workstation->Interact(Hud);
	Hud->OpenAssessment();
	const ERbSubmitResult Submit = Hud->SubmitAssessment(StrongAssessment()); // the form's Submit button
	R.Check(TEXT("submit"), Submit == ERbSubmitResult::Ok, URbGameSubsystem::SubmitResultText(Submit));

	Npc->Interact(Hud); // debrief
	Hud->CloseScreen();
	CheckFinalState(R, Game);

	Workstation->Interact(Hud);
	const ERbMissionResult Case = PlayCaseBest(Hud, TEXT("M002")); // the case files Submit button
	R.Check(TEXT("m002_submit"), Case == ERbMissionResult::Ok, URbCampaignSubsystem::ResultText(Case));
	Hud->CloseScreen();
	CheckCampaign(R, Game);

	FString SaveMessage;
	R.Check(TEXT("save"), Hud->SaveNow(SaveMessage), Hud->Saves()->GetSlotPath());
}

void RunResume(FReport& R, ARbHUD* Hud)
{
	FString Message;
	const ERbLoadOutcome Outcome = Hud->ContinueGame(Message); // the main menu Continue button
	R.Check(TEXT("load"), Outcome == ERbLoadOutcome::Ok, FString::Printf(TEXT("outcome %d %s %s"), static_cast<int32>(Outcome), *Message, *Hud->Saves()->GetSlotPath()));
	if (Outcome == ERbLoadOutcome::Ok || Outcome == ERbLoadOutcome::RecoveredFromBackup)
	{
		CheckFinalState(R, Hud->Game());
		CheckCampaign(R, Hud->Game());
	}
}
} // namespace

struct FTourState
{
	TArray<FString> Names;
	TArray<FTransform> Views;
	TArray<FString> Lines;
	int32 Index = 0;
	bool bCaptured = false;
	double SumMs = 0.0;
	FTimerHandle Timer;
};

static FString FileSafe(const FString& Name)
{
	FString Out;
	for (TCHAR C : Name) Out.AppendChar(FChar::IsAlnum(C) ? C : TEXT('_'));
	return Out;
}

// Teleports the player through every room; each stop settles for 2.5 s, then records timings and a screenshot.
static void RunTour(UWorld* World, ARbHUD* Hud)
{
	APlayerController* PC = World->GetFirstPlayerController();
	ARbValtrenHQ* HQ = nullptr;
	for (TActorIterator<ARbValtrenHQ> It(World); It; ++It) HQ = *It;
	if (!PC || !HQ || !PC->GetPawn())
	{
		FFileHelper::SaveStringToFile(TEXT("FAIL\nno player or HQ\n"), *(FPaths::ProjectSavedDir() / TEXT("RbTour.txt")));
		FPlatformMisc::RequestExit(false, TEXT("RbSelfTest"));
		return;
	}
	Hud->StartNewGame(ERbDifficulty::Professional);
	PC->SetIgnoreLookInput(true);
	PC->SetIgnoreMoveInput(true);
	TSharedRef<FTourState> State = MakeShared<FTourState>();
	HQ->GetTourViewpoints(State->Names, State->Views);
	if (FParse::Param(FCommandLine::Get(), TEXT("RbTourEggs")))
	{
		// Local check of the hidden credits; these close-ups are not published.
		auto View = [](const FVector& From, const FVector& To) { return FTransform((To - From).Rotation(), From); };
		State->Names = {TEXT("Egg_Certificate"), TEXT("Egg_Book"), TEXT("Egg_Plate")};
		State->Views = {View(FVector(70.0f, -1010.0f, 160.0f), FVector(8.0f, -1010.0f, 150.0f)),
			View(FVector(2085.0f, 654.0f, 165.0f), FVector(2030.0f, 654.0f, 153.0f)),
			View(FVector(3282.0f, -850.0f, 165.0f), FVector(3282.0f, -770.0f, 74.0f))};
	}
	const FIntPoint Size = World->GetGameViewport() ? World->GetGameViewport()->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
	State->Lines.Add(FString::Printf(TEXT("resolution=%dx%d"), Size.X, Size.Y));

	auto Place = [PC](const FTransform& View)
	{
		PC->GetPawn()->SetActorLocation(View.GetLocation() - FVector(0.0f, 0.0f, 70.0f), false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(View.Rotator());
		if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame(); // no motion blur from the jump
	};
	Place(State->Views[0]);
	TWeakObjectPtr<UWorld> WeakWorld(World);
	World->GetTimerManager().SetTimer(State->Timer, [StateRef = State, WeakWorld, Place]()
	{
		// Clearing this timer below destroys the lambda and its captures, so work on local copies.
		const TSharedRef<FTourState> State = StateRef;
		const TWeakObjectPtr<UWorld> TourWorld = WeakWorld;
		const auto PlaceAt = Place;
		if (!TourWorld.IsValid()) return;
		// Alternate ticks: capture the current view, then move on the next tick, so the screenshot
		// (taken at the end of the frame) always shows the view it is named after.
		if (State->bCaptured)
		{
			State->bCaptured = false;
			if (++State->Index < State->Views.Num())
			{
				PlaceAt(State->Views[State->Index]);
				return;
			}
		}
		else
		{
			State->bCaptured = true;
		}
		const int32 I = State->Index;
		if (I >= State->Views.Num())
		{
			TourWorld->GetTimerManager().ClearTimer(State->Timer);
			const double AvgMs = State->SumMs / State->Views.Num();
			FString Text = FString::Printf(TEXT("PASS\nmode=tour\naverage_frame_ms=%.2f average_fps=%.1f\n"), AvgMs, AvgMs > 0.0 ? 1000.0 / AvgMs : 0.0);
			for (const FString& L : State->Lines) Text += L + TEXT("\n");
			const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RbTour.txt"));
			FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			UE_LOG(LogRiskbound, Display, TEXT("RbSelfTest wrote %s:\n%s"), *Path, *Text);
			FTimerHandle Quit;
			TourWorld->GetTimerManager().SetTimer(Quit, []() { FPlatformMisc::RequestExit(false, TEXT("RbSelfTest")); }, 1.0f, false);
			return;
		}
		const double Gpu = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		State->Lines.Add(FString::Printf(TEXT("%s: fps=%.1f frame_ms=%.2f game_ms=%.2f render_ms=%.2f gpu_ms=%.2f"), *State->Names[I], GAverageFPS, GAverageMS,
			FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), Gpu));
		State->SumMs += GAverageMS;
		FScreenshotRequest::RequestScreenshot(TEXT("Tour_") + FileSafe(State->Names[I]), false, false);
	}, 2.0f, true);
}

// Opens every screen through the same HUD calls the UI uses and captures each with the UI visible
// (Saved/Screenshots/<platform>/Ui_<n>_<name>.png). Uses the self-test save slot.
static void RunUiTour(UWorld* World, ARbHUD* Hud)
{
	struct FStep
	{
		FString Name;
		TFunction<void(ARbHUD*)> Open;
	};
	TSharedRef<TArray<FStep>> Steps = MakeShared<TArray<FStep>>();
	Steps->Add({TEXT("MainMenu"), [](ARbHUD* H) { H->ShowMainMenu(); }});
	Steps->Add({TEXT("Difficulty"), [](ARbHUD* H) { H->ShowDifficultyPicker(); }});
	Steps->Add({TEXT("Hud"), [](ARbHUD* H) { H->StartNewGame(ERbDifficulty::Professional); }});
	Steps->Add({TEXT("Briefing"), [](ARbHUD* H) { H->TalkToManager(); }});
	Steps->Add({TEXT("Workstation"), [](ARbHUD* H) { H->OpenWorkstation(); }});
	Steps->Add({TEXT("Document"), [](ARbHUD* H) { H->OpenEvidence(TEXT("risk_register_R014")); }});
	Steps->Add({TEXT("Assessment"), [](ARbHUD* H) { H->OpenAssessment(); }});
	Steps->Add({TEXT("Journal"), [](ARbHUD* H) { H->CloseScreen(); H->OpenJournal(); }});
	Steps->Add({TEXT("Pause"), [](ARbHUD* H) { H->CloseScreen(); H->OpenPauseMenu(); }});
	Steps->Add({TEXT("Settings"), [](ARbHUD* H) { H->OpenSettings([H]() { H->CloseScreen(); }); }});
	Steps->Add({TEXT("Credits"), [](ARbHUD* H) { H->OpenCredits([H]() { H->CloseScreen(); }); }});
	const TCHAR* TabNames[] = {TEXT("Register"), TEXT("Kri"), TEXT("Loss"), TEXT("CostBenefit"), TEXT("Sensitivity"), TEXT("Stress"), TEXT("MonteCarlo"), TEXT("Actions"), TEXT("Report")};
	for (int32 Tab = 0; Tab < UE_ARRAY_COUNT(TabNames); ++Tab)
	{
		Steps->Add({FString(TEXT("Tools_")) + TabNames[Tab], [Tab](ARbHUD* H) { H->CloseScreen(); H->OpenToolHub(Tab); }});
	}
	Steps->Add({TEXT("Debrief"), [](ARbHUD* H)
	{
		H->CloseScreen();
		for (const FRbEvidenceView& E : H->Game()->GetEvidence()) H->Game()->ViewEvidence(E.Id);
		H->SubmitAssessment(StrongAssessment());
		H->TalkToManager();
	}});
	Steps->Add({TEXT("CaseFiles"), [](ARbHUD* H) { H->CloseScreen(); H->Game()->MarkDebriefReceived(); H->OpenCaseFiles(); }});
	Steps->Add({TEXT("CaseMission"), [](ARbHUD* H) { H->OpenCaseFiles(TEXT("M002")); }});
	Steps->Add({TEXT("CaseDebrief"), [](ARbHUD* H) { PlayCaseBest(H, TEXT("M002")); H->OpenCaseFiles(TEXT("M002"), true); }});
	TSharedRef<int32> Index = MakeShared<int32>(0);
	TSharedRef<bool> bOpened = MakeShared<bool>(false);
	TWeakObjectPtr<ARbHUD> WeakHud(Hud);
	// Core ticker, not a world timer: menus pause the world, and world timers stop while paused.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Steps, Index, bOpened, WeakHud](float)
	{
		if (!WeakHud.IsValid()) return false;
		if (*Index >= Steps->Num())
		{
			FPlatformMisc::RequestExit(false, TEXT("RbSelfTest"));
			return false;
		}
		if (!*bOpened)
		{
			(*Steps)[*Index].Open(WeakHud.Get());
			*bOpened = true;
			return true;
		}
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Ui_%02d_%s"), *Index, *(*Steps)[*Index].Name), true, false);
		*bOpened = false;
		++*Index;
		return true;
	}), 0.8f);
}

void Run(UWorld* World)
{
	const FString Mode = RequestedMode();
	FReport R;
	APlayerController* PC = World->GetFirstPlayerController();
	ARbHUD* Hud = PC ? Cast<ARbHUD>(PC->GetHUD()) : nullptr;
	R.Check(TEXT("hud"), Hud != nullptr, Hud ? Hud->GetName() : FString(TEXT("missing")));
	if (Hud && Mode == TEXT("ui"))
	{
		Hud->Saves()->SetSlotName(SlotName);
		RunUiTour(World, Hud);
		return;
	}
	if (Hud && (Mode == TEXT("walkthrough") || Mode == TEXT("walkthrough2")))
	{
		Hud->Saves()->SetSlotName(TEXT("walkthrough"));
		RbWalkthrough::Start(World, Hud, Mode == TEXT("walkthrough") ? 1 : 2);
		return;
	}
	if (Hud && Mode == TEXT("tour"))
	{
		RunTour(World, Hud);
		return;
	}
	if (Hud)
	{
		Hud->Saves()->SetSlotName(SlotName);
		if (Mode == TEXT("play")) RunPlay(R, Hud, World);
		else if (Mode == TEXT("resume")) RunResume(R, Hud);
		else R.Check(TEXT("mode"), false, TEXT("expected play or resume"));
	}
	R.Write(Mode, Mode == TEXT("resume") ? TEXT("RbSelfTest.txt") : TEXT("RbSelfTest_play.txt"));
	FPlatformMisc::RequestExit(false, TEXT("RbSelfTest"));
}
} // namespace RbSelfTest
