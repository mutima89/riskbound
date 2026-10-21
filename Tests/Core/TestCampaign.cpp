#include "RbTest.h"

#include "CampaignFixture.h"
#include "RbSave.h"

#include <algorithm>

using namespace rb;
using namespace rbtest;

static bool HasText(const std::vector<std::string>& V, const std::string& Needle)
{
	return std::any_of(V.begin(), V.end(), [&](const std::string& S) { return S.find(Needle) != std::string::npos; });
}

TEST(Campaign_ValidatorAcceptsFixtureAndRejectsBrokenContent)
{
	const MissionCatalogue Good = TestCatalogue();
	CHECK(ValidateCatalogue(Good).empty());

	MissionDef Weights = BasicMission("M050", {});
	Weights.Questions[0].Weight = 30;
	CHECK(HasText(ValidateMission(Weights), "weights sum to 105"));

	MissionDef NoGood = BasicMission("M051", {});
	NoGood.Questions[1].Options = {Opt("a", 50), Opt("b", 40)};
	CHECK(HasText(ValidateMission(NoGood), "no option scores 80"));

	MissionDef Multi = BasicMission("M052", {});
	Multi.Questions[0].Kind = EQuestionKind::Multi;
	Multi.Questions[0].Options = {Opt("a", 0, true), Opt("b", 0, true)};
	CHECK(HasText(ValidateMission(Multi), "at least one correct and one incorrect"));

	MissionDef Prob = BasicMission("M053", {});
	ConsequenceDef Bad;
	Bad.Id = "x";
	Bad.Description = "x";
	Bad.Probability = 1.5;
	Bad.DelayWeeks = 0;
	Prob.Questions[0].Options[0].Schedules = {Bad};
	const auto ProbErrors = ValidateMission(Prob);
	CHECK(HasText(ProbErrors, "probability outside 0..1"));
	CHECK(HasText(ProbErrors, "delay must be at least one week"));

	MissionDef Effect = BasicMission("M054", {});
	Effect.Questions[0].Options[0].Effects = {{EOrgMetric::Reputation, 80}};
	CHECK(HasText(ValidateMission(Effect), "index effect above 50"));

	MissionCatalogue Dup = Good;
	Dup.Missions.push_back(Good.Missions[0]);
	Dup.Missions.push_back(BasicMission("M055", {"M999"}));
	const auto DupErrors = ValidateCatalogue(Dup);
	CHECK(HasText(DupErrors, "M002: duplicate mission id"));
	CHECK(HasText(DupErrors, "requires unknown mission 'M999'"));
}

TEST(Campaign_AvailabilityIsCheckedInTheSim)
{
	const MissionCatalogue C = TestCatalogue();
	GameState G = NewGame("Test", EDifficulty::Professional, 5);
	const MissionAvailability Before = CheckAvailability(G, *C.Find("M002"));
	CHECK(!Before.bAvailable);
	CHECK(HasText(Before.Reasons, "Pass M001 first"));
	CHECK(StartMission(G, *C.Find("M002")) == EMissionError::Unavailable);
	CHECK(AvailableMissions(G, C).empty());

	G = AfterM001();
	const auto Open = AvailableMissions(G, C);
	REQUIRE(Open.size() == size_t(1));
	CHECK(Open[0]->Id == "M002");
	CHECK(HasText(CheckAvailability(G, *C.Find("C901")).Reasons, "Not triggered yet"));
	CHECK(HasText(CheckAvailability(G, *C.Find("M004")).Reasons, "level 3"));

	CHECK(StartMission(G, *C.Find("M002")) == EMissionError::None);
	CHECK(StartMission(G, *C.Find("M002")) == EMissionError::None); // resume
	G.Career.RecordMission("M002", true, 0);
	CHECK(StartMission(G, *C.Find("M003")) == EMissionError::AnotherMissionActive);
	CHECK(AbandonMission(G) == EMissionError::None);
	CHECK(StartMission(G, *C.Find("M003")) == EMissionError::None);
}

TEST(Campaign_SubmitNeedsEvidenceAndEveryAnswer)
{
	const MissionCatalogue C = TestCatalogue();
	const MissionDef& M = *C.Find("M002");
	GameState G = AfterM001();
	const auto Good = AllAnswers(M, "good");
	CHECK(SubmitMission(G, M, Good) == EMissionError::NotActive);
	CHECK(StartMission(G, M) == EMissionError::None);
	CHECK(SubmitMission(G, M, Good) == EMissionError::RequiredEvidenceNotViewed);
	CHECK(ViewMissionEvidence(G, M, "nope") == EMissionError::UnknownEvidence);
	CHECK(ViewMissionEvidence(G, M, "ev1") == EMissionError::None); // ev2 is optional
	std::vector<MissionAnswer> Missing(Good.begin(), Good.end() - 1);
	CHECK(SubmitMission(G, M, Missing) == EMissionError::UnansweredQuestion);
	std::vector<MissionAnswer> Two = Good;
	Two[0].OptionIds = {"good", "weak"};
	CHECK(SubmitMission(G, M, Two) == EMissionError::SingleNeedsOneOption);
	std::vector<MissionAnswer> Unknown = Good;
	Unknown[1].OptionIds = {"made_up"};
	CHECK(SubmitMission(G, M, Unknown) == EMissionError::UnknownOption);
	CHECK(G.Campaign.Records.empty());
	CHECK(G.Org.Decisions.empty()); // nothing changed by the failed submissions
	CHECK(SubmitMission(G, M, Good) == EMissionError::None);
	CHECK_EQ(G.Campaign.Records.size(), size_t(1));
	CHECK(G.Campaign.ActiveMissionId.empty());
}

TEST(Campaign_ScoringSingleMultiAndAuthority)
{
	MissionDef M = BasicMission("M060", {});
	// Q0 becomes multi-select: correct set {a, b}.
	M.Questions[0].Kind = EQuestionKind::Multi;
	M.Questions[0].Options = {Opt("a", 0, true), Opt("b", 0, true), Opt("c", 0, false)};
	// Q1: the best answer costs 20,000 without escalating; an escalation alternative scores 85.
	MissionOptionDef Spend = Opt("spend", 95);
	Spend.Cost = 20000.0;
	MissionOptionDef Escalate = Opt("escalate", 85);
	Escalate.Cost = 20000.0;
	Escalate.bEscalates = true;
	M.Questions[1].Options = {Spend, Escalate, Opt("weak", 35)};
	CHECK(ValidateMission(M).empty());

	std::vector<MissionAnswer> A = {{"identify", {"a", "c"}}, {"assess", {"spend"}}, {"controls", {"good"}}, {"explain", {"good"}}};
	const MissionDebrief Junior = ScoreMission(M, A, 2); // authority 5,000
	REQUIRE(Junior.Criteria.size() == size_t(4));
	CHECK_EQ(Junior.Criteria[0].Score, 25); // 1 of 2 correct (50) minus one false pick (25)
	CHECK(HasText(Junior.Criteria[0].Feedback, "Missed: Option b"));
	CHECK_EQ(Junior.Criteria[1].Score, 40); // capped: above authority
	CHECK(Junior.Criteria[1].bAuthorityExceeded);
	CHECK(HasText(Junior.Criteria[1].Feedback, "above your approval authority of 5,000"));
	CHECK_EQ(Junior.Score, 56); // (25 + 40 + 80 + 80) / 4 = 56.25
	CHECK(!Junior.bPassed);

	const MissionDebrief Analyst = ScoreMission(M, A, 3); // authority 25,000: the spend is within it
	CHECK_EQ(Analyst.Criteria[1].Score, 95);
	CHECK(!Analyst.Criteria[1].bAuthorityExceeded);

	A[1].OptionIds = {"escalate"};
	CHECK_EQ(ScoreMission(M, A, 2).Criteria[1].Score, 85);
	// Evidence: one result per assessed competency, never XP.
	CHECK_EQ(Junior.CompetencyEvidence.size(), size_t(4));
	CHECK_EQ(Junior.XpAwarded, 120 * 56 / 100);
}

TEST(Campaign_PromotionNeedsCompetenceAndMissionsAcrossMissions)
{
	const MissionCatalogue C = TestCatalogue();
	GameState G = AfterM001();
	G.Career.Xp = 1000000; // XP is never an input
	MissionDebrief D;
	CHECK(Play(G, *C.Find("M002"), AllAnswers(*C.Find("M002"), "good"), &D) == EMissionError::None);
	CHECK(D.bPassed);
	CHECK(!D.bPromoted);
	CHECK_EQ(D.Level, 2);
	CHECK(HasText(D.PromotionGaps, "Mission M003 not passed"));
	CHECK(Play(G, *C.Find("M003"), AllAnswers(*C.Find("M003"), "good"), &D) == EMissionError::None);
	CHECK(D.bPromoted);
	CHECK_EQ(G.Career.Level, 3);
	CHECK(HasText(G.LearningHistory, "M003: Lesson M003"));
	// Level 3 opens M004.
	CHECK(CheckAvailability(G, *C.Find("M004")).bAvailable);
}

TEST(Campaign_PassingMissionsWithWeakCompetenceDoesNotPromote)
{
	const MissionCatalogue C = TestCatalogue();
	GameState G = AfterM001();
	for (const char* Id : {"M002", "M003"})
	{
		auto A = AllAnswers(*C.Find(Id), "good");
		A[3].OptionIds = {"weak"}; // communication 20 both times: 80 -> 56 -> 42
		MissionDebrief D;
		CHECK(Play(G, *C.Find(Id), A, &D) == EMissionError::None);
		CHECK(D.bPassed); // (90 + 85 + 80 + 20) / 4 = 69
		CHECK(!D.bPromoted);
	}
	CHECK_EQ(G.Career.Get(ECompetency::Communication).Score, 42);
	CHECK_EQ(G.Career.Level, 2);
	CHECK(HasText(G.Career.CheckPromotion().UnmetGates, "Communication below 45"));
}

TEST(Campaign_ConsequencesCarryIntoLaterMissions)
{
	const MissionCatalogue C = TestCatalogue();
	GameState G = AfterM001();
	auto A2 = AllAnswers(*C.Find("M002"), "good");
	A2[2].OptionIds = {"defer"}; // schedules the depot fire 3 weeks out
	MissionDebrief D;
	CHECK(Play(G, *C.Find("M002"), A2, &D) == EMissionError::None);
	CHECK_EQ(G.Org.Week, 2);
	CHECK(D.Resolved.empty()); // not due yet
	CHECK(G.Org.HasFlag("m002_deferred"));
	CHECK(G.Org.HasFlag("M002_passed"));
	CHECK(!CheckAvailability(G, *C.Find("C901")).bAvailable);
	const auto Brief = BriefingFor(G, *C.Find("M003"));
	REQUIRE(Brief.size() == size_t(2));
	CHECK(Brief[1].find("deferred") != std::string::npos);

	const double Reputation = G.Org.Get(EOrgMetric::Reputation);
	CHECK(Play(G, *C.Find("M003"), AllAnswers(*C.Find("M003"), "good"), &D) == EMissionError::None);
	REQUIRE(D.Resolved.size() == size_t(1)); // came due during M003
	CHECK(D.Resolved[0].bOccurred);
	CHECK(D.Resolved[0].SourceDecisionId == "M002-1");
	CHECK_NEAR(G.Org.Get(EOrgMetric::Reputation), Reputation - 6.0, 1e-9);
	CHECK(G.Org.HasFlag("depot_fire"));
	CHECK(CheckAvailability(G, *C.Find("C901")).bAvailable);

	// Same choices, but a mitigating flag in place: the fire never happens, the crisis never opens.
	GameState M = AfterM001();
	M.Org.SetFlag("fire_drill");
	CHECK(Play(M, *C.Find("M002"), A2) == EMissionError::None);
	CHECK(Play(M, *C.Find("M003"), AllAnswers(*C.Find("M003"), "good"), &D) == EMissionError::None);
	REQUIRE(D.Resolved.size() == size_t(1));
	CHECK(!D.Resolved[0].bOccurred);
	CHECK(D.Resolved[0].bMitigated);
	CHECK(!M.Org.HasFlag("depot_fire"));
	CHECK(!CheckAvailability(M, *C.Find("C901")).bAvailable);
}

TEST(Campaign_FailedMissionCanBeRetriedAndRecordsEachAttempt)
{
	const MissionCatalogue C = TestCatalogue();
	GameState G = AfterM001();
	MissionDebrief D;
	CHECK(Play(G, *C.Find("M002"), AllAnswers(*C.Find("M002"), "weak"), &D) == EMissionError::None);
	CHECK(!D.bPassed);
	CHECK(G.Org.HasFlag("M002_failed"));
	CHECK(CheckAvailability(G, *C.Find("M002")).bAvailable);
	CHECK(Play(G, *C.Find("M002"), AllAnswers(*C.Find("M002"), "good"), &D) == EMissionError::None);
	CHECK(D.bPassed);
	REQUIRE(G.Campaign.Records.size() == size_t(2));
	CHECK(G.Org.Decisions.back().Id == "M002-2");
	CHECK(!CheckAvailability(G, *C.Find("M002")).bAvailable); // passed: closed
}

TEST(Save_V5RoundTripsCampaignAndConsequenceFlags)
{
	const MissionCatalogue C = TestCatalogue();
	GameState G = AfterM001();
	auto A2 = AllAnswers(*C.Find("M002"), "good");
	A2[2].OptionIds = {"defer"};
	CHECK(Play(G, *C.Find("M002"), A2) == EMissionError::None);
	CHECK(StartMission(G, *C.Find("M003")) == EMissionError::None);
	CHECK(ViewMissionEvidence(G, *C.Find("M003"), "ev2") == EMissionError::None);
	const std::string Text = SerializeGame(G);
	GameState L;
	LoadResult Info;
	CHECK(DeserializeGame(Text, L, Info));
	CHECK(!Info.bMigrated);
	CHECK(SerializeGame(L) == Text);
	CHECK(L.Campaign.ActiveMissionId == "M003");
	REQUIRE(L.Campaign.ViewedEvidence.size() == size_t(1));
	REQUIRE(L.Campaign.Records.size() == size_t(1));
	CHECK(L.Campaign.Records[0].Answers[2].OptionIds[0] == "defer");
	REQUIRE(!L.Org.Pending.empty());
	CHECK(L.Org.Pending.back().FlagIfOccurs == "depot_fire");
	// The reloaded game resolves the same way.
	for (GameState* S : {&G, &L})
	{
		CHECK(ViewMissionEvidence(*S, *C.Find("M003"), "ev1") == EMissionError::None);
		CHECK(SubmitMission(*S, *C.Find("M003"), AllAnswers(*C.Find("M003"), "good")) == EMissionError::None);
	}
	CHECK(SerializeGame(G) == SerializeGame(L));
	CHECK(L.Org.HasFlag("depot_fire"));
}
