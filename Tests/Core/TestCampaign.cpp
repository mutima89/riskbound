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

// Found in the walkthrough: a player who passed everything at their level with weak evidence had no
// case left to play. Passed cases at the current level reopen as reviews: evidence only.
TEST(Campaign_StuckPlayerCanReviewPassedCasesWithoutConsequences)
{
	const MissionCatalogue C = TestCatalogue();
	GameState G = AfterM001();
	for (const char* Id : {"M002", "M003"})
	{
		auto A = AllAnswers(*C.Find(Id), "good");
		A[3].OptionIds = {"weak"}; // communication drops to 42 < 45
		CHECK(Play(G, *C.Find(Id), A) == EMissionError::None);
	}
	CHECK_EQ(G.Career.Level, 2);
	CHECK(IsCampaignStuck(G, C));
	const auto Open = AvailableMissions(G, C);
	REQUIRE(Open.size() == size_t(2)); // M002 and M003 as reviews
	CHECK(CheckAvailability(G, *C.Find("M003"), &C).bReview);
	CHECK(!CheckAvailability(G, *C.Find("M003")).bAvailable); // without the catalogue: closed as before
	CHECK(StartMission(G, *C.Find("M003")) == EMissionError::Unavailable);

	const size_t Decisions = G.Org.Decisions.size();
	const int Week = G.Org.Week;
	const std::string Flags = std::to_string(G.Org.Flags.size());
	CHECK(StartMission(G, *C.Find("M003"), &C) == EMissionError::None);
	for (const MissionEvidenceDef& E : C.Find("M003")->Evidence) ViewMissionEvidence(G, *C.Find("M003"), E.Id);
	MissionDebrief D;
	CHECK(SubmitMission(G, *C.Find("M003"), AllAnswers(*C.Find("M003"), "good"), &D) == EMissionError::None);
	CHECK(D.bReview);
	CHECK_EQ(D.XpAwarded, 0);
	CHECK_EQ(G.Org.Decisions.size(), Decisions); // no new organisational decision
	CHECK_EQ(G.Org.Week, Week);                  // no time passes
	CHECK(std::to_string(G.Org.Flags.size()) == Flags);
	CHECK(D.bPromoted);                          // communication 42 -> 57
	CHECK_EQ(G.Career.Level, 3);
	CHECK(!IsCampaignStuck(G, C));               // M004 is open now
	CHECK(!CheckAvailability(G, *C.Find("M002"), &C).bAvailable);
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

TEST(Campaign_NumericAndRatingQuestionsScoreByCloseness)
{
	MissionDef M = BasicMission("M060", {"M001"});
	MissionQuestionDef& Num = M.Questions[0];
	Num.Kind = EQuestionKind::Numeric;
	Num.Options.clear();
	Num.Answer = 20800;
	Num.Tolerance = 0.02;
	Num.Unit = "a year";
	Num.Explanation = "4% x 120,000 + 1% x 1,600,000.";
	Num.Mistakes = {{1600000, "That is the worst case, not the expected loss."}};
	MissionQuestionDef& Rate = M.Questions[1];
	Rate.Kind = EQuestionKind::Rating;
	Rate.Options.clear();
	Rate.TargetLikelihood = 4;
	Rate.TargetImpact = 4;
	Rate.Explanation = "Likely and major.";
	CHECK(ValidateMission(M).empty());

	auto ScoreWith = [&](const std::string& Figure, const std::string& L, const std::string& I)
	{
		std::vector<MissionAnswer> A = AllAnswers(M, "good");
		A[0].OptionIds = {Figure};
		A[1].OptionIds = {L, I};
		return ScoreMission(M, A, 2);
	};
	MissionDebrief D = ScoreWith("20,800", "4", "4");
	CHECK_EQ(D.Criteria[0].Score, NumericScoreExact);
	CHECK_EQ(D.Criteria[1].Score, 100);
	CHECK_EQ(ScoreWith("21500", "4", "4").Criteria[0].Score, NumericScoreClose);  // 3.4% off
	CHECK_EQ(ScoreWith("24000", "4", "4").Criteria[0].Score, NumericScoreNear);   // 15% off
	CHECK_EQ(ScoreWith("52000", "4", "4").Criteria[0].Score, 0);
	D = ScoreWith("1600000", "3", "5");
	CHECK_EQ(D.Criteria[0].Score, 0);
	CHECK(HasText(D.Criteria[0].Feedback, "worst case"));
	CHECK(HasText(D.Criteria[0].Feedback, "Answer: 20,800 a year"));
	CHECK_EQ(D.Criteria[1].Score, 100 - 2 * RatingStepPenalty);
	CHECK_EQ(ScoreWith("20800", "1", "1").Criteria[1].Score, 0);

	// Answers must be well formed before submission.
	GameState G = AfterM001();
	REQUIRE(StartMission(G, M) == EMissionError::None);
	ViewMissionEvidence(G, M, "ev1");
	std::vector<MissionAnswer> Bad = AllAnswers(M, "good");
	Bad[0].OptionIds = {"about twenty"};
	Bad[1].OptionIds = {"4", "4"};
	CHECK(ValidateAnswers(G, M, Bad) == EMissionError::UnknownOption);
	Bad[0].OptionIds = {"20800"};
	Bad[1].OptionIds = {"4", "6"};
	CHECK(ValidateAnswers(G, M, Bad) == EMissionError::UnknownOption);
	std::vector<MissionAnswer> Best;
	for (const MissionQuestionDef& Q : M.Questions) Best.push_back(BestAnswer(Q));
	CHECK(ValidateAnswers(G, M, Best) == EMissionError::None);
	MissionDebrief Final;
	CHECK(SubmitMission(G, M, Best, &Final) == EMissionError::None);
	CHECK_EQ(Final.Score, 100 * 25 / 100 + 100 * 25 / 100 + 80 * 25 / 100 + 80 * 25 / 100);

	// Broken numeric content is caught.
	MissionDef Broken = M;
	Broken.Questions[0].Tolerance = 0.0;
	Broken.Questions[0].Mistakes = {{20900, "Too close to be a mistake."}};
	Broken.Questions[1].TargetImpact = 7;
	const auto E = ValidateMission(Broken);
	CHECK(HasText(E, "tolerance"));
	CHECK(HasText(E, "rating targets"));
}

TEST(Campaign_LocatedEvidenceAndPresentingInPerson)
{
	MissionDef M = BasicMission("M061", {"M001"});
	M.Evidence[0].At = "monitoring";
	M.Evidence[0].Person = "amara";
	M.PresentAt = "boardroom";
	CHECK(ValidateMission(M).empty());
	CHECK(LocationName("monitoring") == "Risk monitoring room");
	GameState G = AfterM001();
	REQUIRE(StartMission(G, M) == EMissionError::None);
	CHECK(ViewMissionEvidence(G, M, "ev1") == EMissionError::EvidenceElsewhere);
	CHECK(ViewMissionEvidence(G, M, "ev1", "meeting") == EMissionError::EvidenceElsewhere);
	CHECK(ViewMissionEvidence(G, M, "ev1", "monitoring") == EMissionError::None);
	CHECK(ViewMissionEvidence(G, M, "ev2") == EMissionError::None); // unlocated evidence: anywhere
	const auto A = AllAnswers(M, "good");
	CHECK(SubmitMission(G, M, A) == EMissionError::PresentInPerson);
	CHECK(G.Campaign.Records.empty());
	CHECK(SubmitMission(G, M, A, nullptr, nullptr, nullptr, "boardroom") == EMissionError::None);

	MissionDef Bad = M;
	Bad.Evidence[1].At = "basement";
	Bad.Evidence[1].Person = "nobody";
	Bad.PresentAt = "roof";
	const auto E = ValidateMission(Bad);
	CHECK(HasText(E, "unknown location 'basement'"));
	CHECK(HasText(E, "presentAt is an unknown location"));
}
