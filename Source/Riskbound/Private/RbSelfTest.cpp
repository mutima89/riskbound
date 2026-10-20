#include "RbSelfTest.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RbGameSubsystem.h"
#include "RbHUD.h"
#include "RbInteractable.h"
#include "RbLog.h"
#include "RbSaveSubsystem.h"

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
	R.Check(TEXT("checkpoint"), Game->GetCheckpoint() == URbGameSubsystem::CheckpointDebriefed, Game->GetCheckpoint());
	R.Check(TEXT("career_level"), Career.Level == 2, FString::Printf(TEXT("%d %s"), Career.Level, *Career.Title));
	R.Check(TEXT("passed_missions"), Career.PassedMissions.Contains(TEXT("M001")), FString::Join(Career.PassedMissions, TEXT(",")));
	R.Check(TEXT("r014_treatment"), Entry.bFound && Entry.Treatment == TEXT("Reduce"),
		FString::Printf(TEXT("%s (owner %s, status %s)"), *Entry.Treatment, *Entry.ActionOwner, *Entry.Status));
	const FRbDebriefView Debrief = Game->GetDebrief();
	R.Check(TEXT("debrief"), Debrief.bAvailable && Debrief.bPassed, FString::Printf(TEXT("score %d, passed %d"), Debrief.OverallScore, Debrief.bPassed ? 1 : 0));
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
	}
}
} // namespace

void Run(UWorld* World)
{
	const FString Mode = RequestedMode();
	FReport R;
	APlayerController* PC = World->GetFirstPlayerController();
	ARbHUD* Hud = PC ? Cast<ARbHUD>(PC->GetHUD()) : nullptr;
	R.Check(TEXT("hud"), Hud != nullptr, Hud ? Hud->GetName() : FString(TEXT("missing")));
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
