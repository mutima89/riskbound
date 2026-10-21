// Content-level tests over the shipped Data/ files: scale, a full best-play career, achievements,
// flag reachability and the figures stated in mission options.
#include "RbTest.h"

#include "RbCampaign.h"
#include "RbGameState.h"
#include "RbMissionData.h"
#include "RbQuant.h"
#include "RbSave.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

using namespace rb;

#ifndef RB_DATA_DIR
#error "RB_DATA_DIR must point at the repository's Data folder"
#endif

namespace
{
std::string Slurp(const std::filesystem::path& P)
{
	std::ifstream In(P, std::ios::binary);
	std::stringstream Ss;
	Ss << In.rdbuf();
	return Ss.str();
}

bool Contains(const std::vector<std::string>& V, const std::string& Needle)
{
	return std::any_of(V.begin(), V.end(), [&](const std::string& S) { return S.find(Needle) != std::string::npos; });
}

MissionCatalogue ShippedCatalogue()
{
	std::vector<std::pair<std::string, std::string>> Files;
	for (const auto& Entry : std::filesystem::directory_iterator(std::filesystem::path(RB_DATA_DIR) / "Missions"))
	{
		if (Entry.path().extension() == ".json") Files.push_back({Entry.path().filename().string(), Slurp(Entry.path())});
	}
	std::sort(Files.begin(), Files.end());
	MissionCatalogue C;
	std::vector<std::string> Errors;
	LoadMissionCatalogue(Files, C, Errors);
	return C;
}

std::vector<AchievementDef> ShippedAchievements(std::vector<std::string>& Errors)
{
	std::vector<AchievementDef> Defs;
	AchievementsFromJson(Slurp(std::filesystem::path(RB_DATA_DIR) / "Achievements.json"), "Achievements.json", Defs, Errors);
	return Defs;
}

std::vector<MissionAnswer> Best(const MissionDef& M)
{
	std::vector<MissionAnswer> Out;
	for (const MissionQuestionDef& Q : M.Questions)
	{
		MissionAnswer A{Q.Id, {}};
		if (Q.Kind == EQuestionKind::Multi)
		{
			for (const MissionOptionDef& O : Q.Options)
			{
				if (O.bCorrect) A.OptionIds.push_back(O.Id);
			}
		}
		else
		{
			A.OptionIds.push_back(std::max_element(Q.Options.begin(), Q.Options.end(),
				[](const MissionOptionDef& L, const MissionOptionDef& R) { return L.Score < R.Score; })->Id);
		}
		Out.push_back(A);
	}
	return Out;
}

EMissionError PlayBestAnswers(GameState& G, const MissionDef& M, MissionDebrief* D)
{
	const EMissionError E = StartMission(G, M);
	if (E != EMissionError::None) return E;
	for (const MissionEvidenceDef& Ev : M.Evidence) ViewMissionEvidence(G, M, Ev.Id);
	return SubmitMission(G, M, Best(M), D);
}

void PassMission001(GameState& G)
{
	G.Mission001.MeetManager();
	for (const m001::EvidenceItem& E : m001::Evidence()) G.Mission001.ViewEvidence(E.Id);
	m001::Submission S;
	S.MissingInfo = {"compressor_failure_history", "after_hours_escalation", "alarm_threshold_config"};
	S.InherentLikelihood = 4;
	S.InherentImpact = 4;
	S.ControlRatings = {EControlEffectiveness::Ineffective, EControlEffectiveness::PartiallyEffective, EControlEffectiveness::Ineffective};
	S.ResidualLikelihood = 4;
	S.ResidualImpact = 3;
	S.TreatmentId = "repair_and_escalation";
	S.ActionOwnerId = "ops_site_manager";
	S.RationaleTagIds = {"alarm_ignored", "service_overdue", "penalty_exposure", "outside_appetite"};
	SubmitMission001(G, S);
}
} // namespace

TEST(Data_ShippedContentScale)
{
	const MissionCatalogue C = ShippedCatalogue();
	int Missions = 1, Crises = 0, Capstones = 0; // Mission 001 is hand-built
	for (const MissionDef& M : C.Missions)
	{
		Missions += M.Kind == EMissionKind::Mission ? 1 : 0;
		Crises += M.Kind == EMissionKind::Crisis ? 1 : 0;
		Capstones += M.Kind == EMissionKind::Capstone ? 1 : 0;
	}
	CHECK_EQ(Missions, 30);
	CHECK_EQ(Crises, 12);
	CHECK_EQ(Capstones, 1);
	for (const CareerLevelDef& L : CareerLadder())
	{
		for (const std::string& Id : L.RequiredMissions)
		{
			const bool bOk = Id == "M000" || Id == "M001" || C.Find(Id) != nullptr; // M000: optional level-1 tutorial
			if (!bOk) std::printf("    missing ladder mission %s\n", Id.c_str());
			CHECK(bOk);
		}
	}
	// Crises the ladder requires must never depend on a random incident.
	for (const char* Id : {"C001", "C002", "C003"})
	{
		REQUIRE(C.Find(Id));
		CHECK(C.Find(Id)->RequiresFlags.empty());
	}
}

TEST(Data_FullCampaignBestPlayReachesChiefRiskOfficer)
{
	const MissionCatalogue C = ShippedCatalogue();
	REQUIRE(!C.Missions.empty());
	GameState G = NewGame("Career", EDifficulty::Professional, 11);
	PassMission001(G);
	REQUIRE(G.Career.HasPassed("M001"));
	for (int Guard = 0; Guard < 200; ++Guard)
	{
		const auto Open = AvailableMissions(G, C);
		if (Open.empty()) break;
		MissionDebrief D;
		REQUIRE(PlayBestAnswers(G, *Open.front(), &D) == EMissionError::None);
		if (!D.bPassed) std::printf("    best play failed %s (%d)\n", Open.front()->Id.c_str(), D.Score);
		CHECK(D.bPassed);
	}
	for (const std::string& Gap : G.Career.CheckPromotion().UnmetGates) std::printf("    gap at level %d: %s\n", G.Career.Level, Gap.c_str());
	CHECK_EQ(G.Career.Level, 10);
	CHECK(G.Career.HasPassed("CRO"));
	for (const CareerLevelDef& L : CareerLadder())
	{
		for (const std::string& Id : L.RequiredMissions)
		{
			if (Id != "M000") CHECK(G.Career.HasPassed(Id));
		}
	}
	for (int i = 0; i < CompetencyCount; ++i) CHECK(G.Career.Competencies[static_cast<size_t>(i)].Assessments >= 2);

	std::vector<std::string> Errors;
	const auto Defs = ShippedAchievements(Errors);
	EvaluateAchievements(G, Defs, C);
	for (const char* Id : {"level_10", "the_whole_picture", "seasoned", "crisis_veteran", "consistently_good", "trust_but_verify", "no_regrets", "well_rounded", "built_to_last"})
	{
		const bool bHas = std::find(G.Achievements.begin(), G.Achievements.end(), Id) != G.Achievements.end();
		if (!bHas) std::printf("    not unlocked: %s\n", Id);
		CHECK(bHas);
	}
	GameState L;
	LoadResult Info;
	CHECK(DeserializeGame(SerializeGame(G), L, Info));
	CHECK(SerializeGame(L) == SerializeGame(G));
}

TEST(Data_AchievementsParseValidateAndNumberFifty)
{
	std::vector<std::string> Errors;
	const auto Defs = ShippedAchievements(Errors);
	for (const std::string& E : Errors) std::printf("    achievement error: %s\n", E.c_str());
	CHECK(Errors.empty());
	CHECK_EQ(Defs.size(), size_t(50));
	const auto Problems = ValidateAchievements(Defs, ShippedCatalogue());
	for (const std::string& E : Problems) std::printf("    achievement problem: %s\n", E.c_str());
	CHECK(Problems.empty());

	std::vector<AchievementDef> Out;
	Errors.clear();
	CHECK(!AchievementsFromJson("{\"schema\": 1, \"achievements\": [{\"id\": \"x\", \"title\": \"X\", \"description\": \"D\", \"when\": {\"reachLvl\": 3}}]}", "a.json", Out, Errors));
	CHECK(Contains(Errors, "unknown key \"reachLvl\""));
	Errors.clear();
	CHECK(!AchievementsFromJson("{\"schema\": 1, \"achievements\": [{\"id\": \"x\", \"title\": \"X\", \"description\": \"D\", \"when\": {\"reachLevel\": 3, \"flag\": \"f\"}}]}", "a.json", Out, Errors));
	CHECK(Contains(Errors, "exactly one condition"));
	AchievementDef UnknownMission{"x", "X", "D", EAchievementCondition::PassMission, "M999"};
	AchievementDef NotBuiltIn{"not_builtin", "X", "D", EAchievementCondition::Code};
	CHECK(!ValidateAchievements({UnknownMission}, ShippedCatalogue()).empty());
	CHECK(!ValidateAchievements({NotBuiltIn}, ShippedCatalogue()).empty());
}

TEST(Achievements_ConditionsEvaluateFromState)
{
	const MissionCatalogue C = ShippedCatalogue();
	GameState G = NewGame("A", EDifficulty::Professional, 4);
	const AchievementDef Level3{"l3", "L3", "D", EAchievementCondition::ReachLevel, "", 3};
	const AchievementDef Flag{"f", "F", "D", EAchievementCondition::Flag, "contractor_visits_verified"};
	const AchievementDef Rep{"r", "R", "D", EAchievementCondition::MetricAtLeast, "Reputation", 70};
	const AchievementDef Retry{"retry", "R", "D", EAchievementCondition::RetryPassed, "", 1};
	const AchievementDef Avoided{"avoid", "A", "D", EAchievementCondition::IncidentsAvoided, "", 1};
	CHECK(!IsAchievementMet(G, Level3, C));
	G.Career.Level = 3;
	CHECK(IsAchievementMet(G, Level3, C));
	CHECK(!IsAchievementMet(G, Flag, C));
	G.Org.SetFlag("contractor_visits_verified");
	CHECK(IsAchievementMet(G, Flag, C));
	CHECK(!IsAchievementMet(G, Rep, C)); // Valtren starts at 62
	G.Org.Metrics[static_cast<int>(EOrgMetric::Reputation)] = 70;
	CHECK(IsAchievementMet(G, Rep, C));
	MissionRecord Failed;
	Failed.MissionId = "M002";
	Failed.Level = 2;
	Failed.Score = 40;
	MissionRecord Passed = Failed;
	Passed.Score = 90;
	Passed.bPassed = true;
	CHECK(!IsAchievementMet(G, Retry, C));
	G.Campaign.Records = {Failed, Passed};
	CHECK(IsAchievementMet(G, Retry, C));
	CHECK(!IsAchievementMet(G, Avoided, C));
	ResolvedConsequence R;
	R.Id = "x";
	R.bOccurred = false;
	G.Org.History.push_back(R);
	CHECK(IsAchievementMet(G, Avoided, C));
	const auto New = EvaluateAchievements(G, {Level3, Flag, Rep}, C);
	CHECK_EQ(New.size(), size_t(3));
	CHECK(EvaluateAchievements(G, {Level3, Flag, Rep}, C).empty()); // unlocks once
}

// Every flag the content reads must be something the game can set: a choice, an incident,
// a mission result (<id>_passed / <id>_failed) or Mission 001.
TEST(Data_EveryReferencedFlagCanBeSet)
{
	const MissionCatalogue C = ShippedCatalogue();
	std::set<std::string> Settable = {"northgate_controls_fixed", "m001_warning_ignored"};
	for (const MissionDef& M : C.Missions)
	{
		Settable.insert(M.Id + "_passed");
		Settable.insert(M.Id + "_failed");
		for (const MissionQuestionDef& Q : M.Questions)
		{
			for (const MissionOptionDef& O : Q.Options)
			{
				for (const std::string& F : O.SetsFlags) Settable.insert(F);
				for (const ConsequenceDef& Cq : O.Schedules)
				{
					if (!Cq.FlagIfOccurs.empty()) Settable.insert(Cq.FlagIfOccurs);
				}
			}
		}
	}
	auto Check = [&](const std::string& Flag, const std::string& Where)
	{
		if (Flag.empty()) return;
		const bool bOk = Settable.count(Flag) == 1;
		if (!bOk) std::printf("    unreachable flag '%s' in %s\n", Flag.c_str(), Where.c_str());
		CHECK(bOk);
	};
	for (const MissionDef& M : C.Missions)
	{
		for (const std::string& F : M.RequiresFlags) Check(F, M.Id + " requiresFlags");
		for (const std::string& F : M.ExcludedByFlags) Check(F, M.Id + " excludedByFlags");
		for (const ContextNoteDef& N : M.Context) Check(N.Flag, M.Id + " context");
		for (const MissionQuestionDef& Q : M.Questions)
		{
			for (const MissionOptionDef& O : Q.Options)
			{
				for (const ConsequenceDef& Cq : O.Schedules) Check(Cq.MitigatingFlag, M.Id + " mitigatedBy");
			}
		}
	}
	std::vector<std::string> Errors;
	for (const AchievementDef& A : ShippedAchievements(Errors))
	{
		if (A.Condition == EAchievementCondition::Flag) Check(A.Target, "achievement " + A.Id);
	}
}

// Figures stated in later missions' options, checked against the engine or plain arithmetic.
TEST(Data_LaterMissionFiguresAreRight)
{
	// M012: route-planning outage 450,000; probability 25% -> 5%.
	CHECK_NEAR(ExpectedLoss({{"o", 0.25, 450000}}) - ExpectedLoss({{"o", 0.05, 450000}}), 90000.0, 1e-6);
	// M014: fuel 12% of 47.2m +35%; client loss 4.59m revenue with 60% of 4.25m costs cut; cash 3.0 + margin - 2.2.
	const double Fuel = 0.12 * 47.2 * 0.35;
	CHECK_NEAR(Fuel, 1.98, 0.01);
	const double Margin = 3.8 - (4.59 - 0.6 * 4.25) - Fuel;
	CHECK_NEAR(Margin, -0.22, 0.01);
	CHECK_NEAR(3.0 + Margin - 2.2, 0.58, 0.01);
	CHECK_NEAR(0.5 * Fuel * 0.75, 0.74, 0.01);
	// M016: 41% of 14m at a 9% margin.
	CHECK_NEAR(0.41 * 14e6 * 0.09, 517000.0, 1000.0);
	// M017: break-even probability for dual sourcing.
	CHECK_NEAR(90000.0 / 1.9e6, 0.047, 0.001);
	// M024: 0.2 - 14% of 4.6 - 0.15.
	CHECK_NEAR(0.2 - 0.14 * 4.6 - 0.15, -0.594, 0.001);
	// M027 refrigerant saving, M028 contingency, M030 bank-detail fraud ALE.
	CHECK_NEAR(310.0 / 2.0 * 120.0, 18600.0, 1e-9);
	CHECK_NEAR(14e6 * 0.12, 1.68e6, 1.0);
	CHECK_NEAR(AnnualisedLossExpectancy(600.0 / 400.0, 45000.0), 67500.0, 1e-6);
}
