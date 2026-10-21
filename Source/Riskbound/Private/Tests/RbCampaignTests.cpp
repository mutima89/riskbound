// Automation tests for campaign missions through the UE subsystem (Riskbound.Campaign.*).
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "RbCampaignSubsystem.h"
#include "RbConvert.h"
#include "RbGameSubsystem.h"

#include <algorithm>

namespace
{
constexpr EAutomationTestFlags CampaignFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

rb::MissionDef FixtureMission(const char* Id, std::vector<std::string> Requires)
{
	using rb::ECompetency;
	rb::MissionDef M;
	M.Id = Id;
	M.Title = std::string("Fixture ") + Id;
	M.Department = "Operations";
	M.Briefing = "Fixture briefing";
	M.MinLevel = 2;
	M.RequiresPassed = std::move(Requires);
	M.Evidence = {{"memo", "Memo", "Fixture memo", true}};
	int Weight = 25;
	for (ECompetency C : {ECompetency::RiskIdentification, ECompetency::RiskAssessment, ECompetency::ControlEvaluation, ECompetency::Communication})
	{
		rb::MissionQuestionDef Q;
		Q.Id = std::string("q") + std::to_string(static_cast<int>(C));
		Q.Prompt = "Which?";
		Q.Competency = C;
		Q.Weight = Weight;
		rb::MissionOptionDef Good;
		Good.Id = "good";
		Good.Label = "Good";
		Good.Score = 90;
		rb::MissionOptionDef Weak;
		Weak.Id = "weak";
		Weak.Label = "Weak";
		Weak.Score = 20;
		Q.Options = {Good, Weak};
		M.Questions.push_back(Q);
	}
	return M;
}

TArray<FRbMissionAnswer> Answers(const FRbMissionView& V, const TCHAR* Pick)
{
	TArray<FRbMissionAnswer> Out;
	for (const FRbMissionQuestionView& Q : V.Questions)
	{
		FRbMissionAnswer A;
		A.QuestionId = Q.Id;
		A.OptionIds = {Pick};
		Out.Add(A);
	}
	return Out;
}

struct FCampaignSession
{
	UGameInstance* Instance = nullptr;
	URbGameSubsystem* Game = nullptr;
	URbCampaignSubsystem* Campaign = nullptr;

	FCampaignSession()
	{
		Instance = NewObject<UGameInstance>(GEngine);
		Instance->AddToRoot();
		Instance->InitializeStandalone();
		Game = Instance->GetSubsystem<URbGameSubsystem>();
		Campaign = Instance->GetSubsystem<URbCampaignSubsystem>();
		Game->NewGame(ERbDifficulty::Professional);
	}

	~FCampaignSession()
	{
		UWorld* World = Instance->GetWorld();
		Instance->Shutdown();
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		Instance->RemoveFromRoot();
	}
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRbCampaignProgressTest, "Riskbound.Campaign.ProgressPromotionAndHiddenRubric", CampaignFlags)
bool FRbCampaignProgressTest::RunTest(const FString& Parameters)
{
	FCampaignSession S;
	if (!TestNotNull(TEXT("campaign subsystem"), S.Campaign)) return false;
	rb::MissionCatalogue Bad;
	Bad.Missions = {FixtureMission("M002", {"M404"})};
	TestFalse(TEXT("invalid catalogue rejected"), S.Campaign->SetCatalogue(MoveTemp(Bad)).IsEmpty());
	rb::MissionCatalogue Cat;
	Cat.Missions = {FixtureMission("M002", {"M001"}), FixtureMission("M003", {"M002"})};
	TestTrue(TEXT("catalogue accepted"), S.Campaign->SetCatalogue(MoveTemp(Cat)).IsEmpty());

	TArray<FRbMissionSummary> List = S.Campaign->GetMissions();
	TestTrue(TEXT("two missions, both closed before M001"), List.Num() == 2 && !List[0].bAvailable && List[0].Reasons.Contains(TEXT("Pass M001 first")));
	TestTrue(TEXT("start refused"), S.Campaign->StartMission(TEXT("M002")) == ERbMissionResult::Unavailable);

	// Mission 001 passed with solid evidence (as the vertical slice would leave it).
	rb::GameState& State = S.Game->EditState();
	for (rb::ECompetency C : {rb::ECompetency::RiskIdentification, rb::ECompetency::RiskAssessment, rb::ECompetency::ControlEvaluation, rb::ECompetency::Communication})
	{
		State.Career.ApplyAssessment({C, 80});
	}
	State.Career.RecordMission("M001", true, 100);
	State.Mission001.bSubmitted = true;
	State.Checkpoint = "M001:debriefed";
	S.Game->StateEdited();

	TestTrue(TEXT("rubric hidden before submission"), !S.Campaign->GetDebrief(TEXT("M002")).bAvailable);
	TestTrue(TEXT("start"), S.Campaign->StartMission(TEXT("M002")) == ERbMissionResult::Ok);
	const FRbMissionView V = S.Campaign->GetMission(TEXT("M002"));
	TestTrue(TEXT("view has questions and options"), V.bActive && V.Questions.Num() == 4 && V.Questions[0].Options.Num() == 2);
	TestTrue(TEXT("evidence first"), S.Campaign->SubmitMission(TEXT("M002"), Answers(V, TEXT("good"))) == ERbMissionResult::RequiredEvidenceNotViewed);
	TestTrue(TEXT("view evidence"), S.Campaign->ViewEvidence(TEXT("M002"), TEXT("memo")) == ERbMissionResult::Ok);
	TestTrue(TEXT("submit"), S.Campaign->SubmitMission(TEXT("M002"), Answers(V, TEXT("good"))) == ERbMissionResult::Ok);
	FRbMissionDebriefView D = S.Campaign->GetDebrief(TEXT("M002"));
	TestTrue(TEXT("debrief after submit"), D.bAvailable && D.bPassed && D.Score == 90 && !D.bPromoted);
	TestTrue(TEXT("gap names M003"), D.PromotionGaps.Contains(TEXT("Mission M003 not passed")));
	// Regression: the campaign moves the checkpoint on; Mission 001 must still count as debriefed.
	TestEqual(TEXT("checkpoint moved"), S.Game->GetCheckpoint(), FString(TEXT("M002:debriefed")));
	TestTrue(TEXT("M001 still debriefed"), S.Game->IsDebriefed());
	TestTrue(TEXT("objective points to case files"), S.Game->GetCurrentObjectiveText().Contains(TEXT("Case files")));

	TestTrue(TEXT("start M003"), S.Campaign->StartMission(TEXT("M003")) == ERbMissionResult::Ok);
	S.Campaign->ViewEvidence(TEXT("M003"), TEXT("memo"));
	TestTrue(TEXT("submit M003"), S.Campaign->SubmitMission(TEXT("M003"), Answers(S.Campaign->GetMission(TEXT("M003")), TEXT("good"))) == ERbMissionResult::Ok);
	D = S.Campaign->GetDebrief(TEXT("M003"));
	TestTrue(TEXT("promoted to Risk Analyst"), D.bPromoted && D.Level == 3 && D.LevelTitle == TEXT("Risk Analyst"));
	TestEqual(TEXT("career view agrees"), S.Game->GetCareer().Level, 3);
	List = S.Campaign->GetMissions();
	TestTrue(TEXT("both passed, closed"), List[0].bPassed && List[1].bPassed && !List[1].bAvailable && List[1].Attempts == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRbCampaignDataTest, "Riskbound.Campaign.ShippedMissionDataLoads", CampaignFlags)
bool FRbCampaignDataTest::RunTest(const FString& Parameters)
{
	FCampaignSession S;
	TestTrue(TEXT("no load errors"), S.Campaign->GetLoadErrors().IsEmpty());
	for (const FString& E : S.Campaign->GetLoadErrors()) AddError(E);
	const TArray<FRbMissionSummary> List = S.Campaign->GetMissions(true);
	TestTrue(TEXT("52 data missions (29 missions, 6 case studies, 12 crises, capstone, 4 story chapters)"), List.Num() == 52);
	const TArray<FRbMissionSummary> Visible = S.Campaign->GetMissions();
	TestTrue(TEXT("incident crises hidden until triggered"), Visible.Num() == 39 && !Visible.ContainsByPredicate([](const FRbMissionSummary& M) { return M.Id == TEXT("C004"); }));
	TestTrue(TEXT("lowest level first while all are locked"), Visible.Num() > 0 && Visible[0].Id == TEXT("M002"));
	TestEqual(TEXT("50 achievements"), static_cast<int32>(S.Campaign->GetAchievementDefs().size()), 50);
	TestEqual(TEXT("achievement total"), S.Game->GetAchievementTotal(), 50);
	for (const TCHAR* Id : {TEXT("M002"), TEXT("M003"), TEXT("M004"), TEXT("M005")})
	{
		TestTrue(FString::Printf(TEXT("%s present"), Id), S.Campaign->GetMission(Id).bFound);
	}
	const FRbMissionView M2 = S.Campaign->GetMission(TEXT("M002"));
	TestTrue(TEXT("M002 has evidence and questions"), M2.Evidence.Num() >= 3 && M2.Questions.Num() >= 4);
	TestTrue(TEXT("closed before M001"), !List[0].bAvailable);

	// Option display order: never the content order wholesale, and stable between calls.
	int32 Questions = 0, BestFirst = 0;
	for (const rb::MissionDef& M : S.Campaign->GetCatalogue().Missions)
	{
		const FRbMissionView View = S.Campaign->GetMission(RbConvert::ToFString(M.Id));
		const FRbMissionView Again = S.Campaign->GetMission(RbConvert::ToFString(M.Id));
		for (int32 q = 0; q < View.Questions.Num(); ++q)
		{
			const rb::MissionQuestionDef& Q = M.Questions[static_cast<size_t>(q)];
			if (Q.Kind != rb::EQuestionKind::Single) continue;
			const auto Best = std::max_element(Q.Options.begin(), Q.Options.end(), [](const rb::MissionOptionDef& L, const rb::MissionOptionDef& R) { return L.Score < R.Score; });
			++Questions;
			BestFirst += View.Questions[q].Options[0].Id == RbConvert::ToFString(Best->Id) ? 1 : 0;
			TestTrue(TEXT("stable order"), View.Questions[q].Options[0].Id == Again.Questions[q].Options[0].Id);
		}
	}
	TestTrue(FString::Printf(TEXT("best answer first in %d of %d questions"), BestFirst, Questions), Questions > 100 && BestFirst < Questions / 2);
	const TArray<FRbOptionView> Treatments = S.Game->GetTreatmentOptions();
	TestEqual(TEXT("M001 treatments are all listed"), Treatments.Num(), 4);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
