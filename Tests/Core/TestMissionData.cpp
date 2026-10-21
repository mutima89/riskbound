#include "RbTest.h"

#include "RbCampaign.h"
#include "RbGameState.h"
#include "RbJson.h"
#include "RbMissionData.h"
#include "RbQuant.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace rb;

#ifndef RB_DATA_DIR
#error "RB_DATA_DIR must point at the repository's Data folder"
#endif

static bool HasText(const std::vector<std::string>& V, const std::string& Needle)
{
	return std::any_of(V.begin(), V.end(), [&](const std::string& S) { return S.find(Needle) != std::string::npos; });
}

static std::vector<std::pair<std::string, std::string>> ShippedMissionFiles()
{
	std::vector<std::pair<std::string, std::string>> Files;
	const std::filesystem::path Dir = std::filesystem::path(RB_DATA_DIR) / "Missions";
	for (const auto& Entry : std::filesystem::directory_iterator(Dir))
	{
		if (Entry.path().extension() != ".json") continue;
		std::ifstream In(Entry.path(), std::ios::binary);
		std::stringstream Ss;
		Ss << In.rdbuf();
		Files.push_back({Entry.path().filename().string(), Ss.str()});
	}
	std::sort(Files.begin(), Files.end());
	return Files;
}

// The best answer to every question: highest score for single choice, the correct set for multi.
static std::vector<MissionAnswer> BestAnswers(const MissionDef& M)
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
			const auto Best = std::max_element(Q.Options.begin(), Q.Options.end(), [](const MissionOptionDef& L, const MissionOptionDef& R) { return L.Score < R.Score; });
			A.OptionIds.push_back(Best->Id);
		}
		Out.push_back(A);
	}
	return Out;
}

static EMissionError PlayBest(GameState& G, const MissionDef& M, MissionDebrief* D = nullptr)
{
	const EMissionError E = StartMission(G, M);
	if (E != EMissionError::None) return E;
	for (const MissionEvidenceDef& Ev : M.Evidence) ViewMissionEvidence(G, M, Ev.Id);
	return SubmitMission(G, M, BestAnswers(M), D);
}

TEST(Json_ParsesValuesEscapesAndReportsErrorsWithLines)
{
	JsonValue V;
	std::string Err;
	CHECK(ParseJson("\xEF\xBB\xBF{\"a\": [1, -2.5e1, true, null], \"s\": \"x\\n\\u00e9\\ud83d\\ude00\"}", V, Err));
	REQUIRE(V.IsObject() && V.Find("a") && V.Find("a")->Items.size() == size_t(4));
	CHECK_NEAR(V.Find("a")->Items[1].Number, -25.0, 1e-12);
	CHECK(V.Find("s")->String == "x\n\xC3\xA9\xF0\x9F\x98\x80");
	CHECK(!ParseJson("{\n  \"a\": 1,\n  \"a\": 2\n}", V, Err));
	CHECK(Err.find("line 3") != std::string::npos && Err.find("duplicate key") != std::string::npos);
	CHECK(!ParseJson("{\"a\": 1,}", V, Err));
	CHECK(Err.find("expected a quoted key") != std::string::npos);
	CHECK(!ParseJson("[1, 2", V, Err));
	CHECK(!ParseJson("{\"a\": \"unterminated}", V, Err));
	CHECK(Err.find("unterminated string") != std::string::npos);
	CHECK(!ParseJson("{} extra", V, Err));
	CHECK(!ParseJson("{\"a\": 01x}", V, Err));
}

TEST(MissionData_StrictKeysTypesAndNames)
{
	const std::string Good = R"({
  "schema": 1, "id": "M090", "title": "T", "department": "Ops", "minLevel": 2, "briefing": ["One", "Two"],
  "evidence": [{"id": "e", "title": "E", "body": "B"}],
  "questions": [{"id": "q", "prompt": "P", "competency": "Judgement", "weight": 100,
    "options": [{"id": "a", "label": "A", "score": 90, "effects": {"Reputation": 2}}, {"id": "b", "label": "B", "score": 10}]}]
})";
	MissionDef M;
	std::vector<std::string> Errors;
	CHECK(MissionFromJson(Good, "M090.json", M, Errors));
	CHECK(Errors.empty());
	CHECK(M.Briefing == "One\n\nTwo");
	REQUIRE(M.Questions.size() == size_t(1) && M.Questions[0].Options[0].Effects.size() == size_t(1));
	CHECK(M.Questions[0].Competency == ECompetency::Judgement);
	CHECK(M.Questions[0].Options[0].Effects[0].Metric == EOrgMetric::Reputation);

	std::string Typo = Good;
	Typo.replace(Typo.find("\"weight\""), 8, "\"wieght\"");
	Errors.clear();
	CHECK(!MissionFromJson(Typo, "M090.json", M, Errors));
	CHECK(HasText(Errors, "unknown key \"wieght\""));
	CHECK(HasText(Errors, "missing \"weight\""));
	CHECK(HasText(Errors, "M090.json question (line 4)"));

	std::string BadNames = Good;
	BadNames.replace(BadNames.find("Judgement"), 9, "Wisdom");
	BadNames.replace(BadNames.find("Reputation"), 10, "Karma");
	Errors.clear();
	CHECK(!MissionFromJson(BadNames, "M090.json", M, Errors));
	CHECK(HasText(Errors, "unknown competency \"Wisdom\""));
	CHECK(HasText(Errors, "unknown metric \"Karma\""));

	std::string BadType = Good;
	BadType.replace(BadType.find("\"minLevel\": 2"), 13, "\"minLevel\": \"2\"");
	Errors.clear();
	CHECK(!MissionFromJson(BadType, "M090.json", M, Errors));
	CHECK(HasText(Errors, "\"minLevel\" must be a whole number"));

	MissionCatalogue C;
	Errors.clear();
	CHECK(!LoadMissionCatalogue({{"M091.json", Good}}, C, Errors));
	CHECK(HasText(Errors, "file name must match the mission id \"M090\""));
	Errors.clear();
	CHECK(!LoadMissionCatalogue({{"M090.json", "{ not json"}}, C, Errors));
	CHECK(HasText(Errors, "M090.json: line 1"));
}

TEST(Data_ShippedMissionsParseAndValidate)
{
	const auto Files = ShippedMissionFiles();
	MissionCatalogue C;
	std::vector<std::string> Errors;
	const bool bOk = LoadMissionCatalogue(Files, C, Errors);
	for (const std::string& E : Errors) std::printf("    data error: %s\n", E.c_str());
	CHECK(bOk);
	for (const char* Id : {"M002", "M003", "M004", "M005"}) CHECK(C.Find(Id) != nullptr);
	// Every question has a defensible best answer and the best play passes every mission.
	for (const MissionDef& M : C.Missions)
	{
		const MissionDebrief D = ScoreMission(M, BestAnswers(M), M.MinLevel);
		CHECK(D.bPassed);
		CHECK(D.Score >= 85);
	}
}

TEST(Data_BestPlayPromotesThroughTheShippedMissions)
{
	MissionCatalogue C;
	std::vector<std::string> Errors;
	REQUIRE(LoadMissionCatalogue(ShippedMissionFiles(), C, Errors));
	GameState G = NewGame("Data", EDifficulty::Professional, 3);
	CHECK(AvailableMissions(G, C).empty()); // nothing before Mission 001
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
	REQUIRE(SubmitMission001(G, S) == m001::ESubmitError::None);
	REQUIRE(G.Career.HasPassed("M001"));

	const auto Open = AvailableMissions(G, C);
	REQUIRE(Open.size() == size_t(1));
	CHECK(Open[0]->Id == "M002");
	// M001's fix is mentioned in M002's briefing.
	CHECK(HasText(BriefingFor(G, *C.Find("M002")), "Northgate repair you recommended"));
	MissionDebrief D;
	CHECK(PlayBest(G, *C.Find("M002"), &D) == EMissionError::None);
	CHECK(D.bPassed && !D.bPromoted);
	CHECK(G.Org.HasFlag("contractor_visits_verified"));
	CHECK(HasText(BriefingFor(G, *C.Find("M003")), "Site sign-off for contractor visits"));
	CHECK(StartMission(G, *C.Find("M004")) == EMissionError::Unavailable); // level 3 only
	CHECK(PlayBest(G, *C.Find("M003"), &D) == EMissionError::None);
	CHECK(D.bPassed);
	CHECK(D.bPromoted);
	CHECK_EQ(G.Career.Level, 3);
	CHECK(CheckAvailability(G, *C.Find("M004")).bAvailable);
	CHECK(CheckAvailability(G, *C.Find("M005")).bAvailable);
	CHECK(PlayBest(G, *C.Find("M004"), &D) == EMissionError::None);
	CHECK(D.bPassed);
	CHECK(PlayBest(G, *C.Find("M005"), &D) == EMissionError::None);
	CHECK(D.bPassed);
	CHECK_EQ(G.Career.Level, 3); // level 4 needs M006 and stronger quantitative evidence
	CHECK(HasText(G.Career.CheckPromotion().UnmetGates, "Mission M006 not passed"));
}

// The figures M004 states in its options must be what the quant engine computes.
TEST(Data_M004FiguresMatchTheQuantEngine)
{
	const std::vector<LossScenario> Gross = {{"minor", 0.04, 120000}, {"major", 0.01, 1600000}};
	CHECK_NEAR(ExpectedLoss(Gross), 20800.0, 1e-6);
	// Retained after insurance: excess 250,000, limit 1,000,000 per event.
	auto Retained = [](double Loss) { return Loss - std::min(std::max(Loss - 250000.0, 0.0), 1000000.0); };
	const std::vector<LossScenario> Net = {{"minor", 0.04, Retained(120000)}, {"major", 0.01, Retained(1600000)}};
	CHECK_NEAR(ExpectedLoss(Net), 10800.0, 1e-6);
	const std::vector<LossScenario> NetBarriers = {{"minor", 0.04, Retained(20000)}, {"major", 0.01, Retained(600000)}};
	CHECK_NEAR(ExpectedLoss(NetBarriers), 3300.0, 1e-6);
	TreatmentAppraisalInput In;
	In.ExpectedLossBefore = 10800;
	In.ExpectedLossAfter = 3300;
	In.OneOffCost = 45000;
	In.AnnualCost = 3000;
	In.Years = 5;
	In.DiscountRate = 0.08;
	const TreatmentAppraisal A = AppraiseTreatment(In);
	CHECK_NEAR(A.Npv, -27000.0, 500.0); // "about -27,000"
	CHECK_NEAR(A.PvBenefit, 29900.0, 100.0); // "about 29,900"
	CHECK_NEAR(A.PvCost, 57000.0, 100.0); // "about 57,000"
	In.ExpectedLossBefore = 20800;
	In.ExpectedLossAfter = 6800;
	CHECK_NEAR(AppraiseTreatment(In).Npv, -1100.0, 100.0); // "about -1,100 on gross losses"
}
