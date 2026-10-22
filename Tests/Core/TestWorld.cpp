// Living world: relationships, stress, story clues, inbox events with deadlines, fallout from weak
// answers, crisis causes and escalation, burnout, news, the share index and save v6.
#include "RbTest.h"

#include "CampaignFixture.h"

#include "RbMissionData.h"
#include "RbQuant.h"
#include "RbSave.h"
#include "RbWorld.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace rb;
using namespace rbtest;

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

MissionCatalogue ShippedMissions()
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
	for (const std::string& E : Errors) std::printf("    %s\n", E.c_str());
	return C;
}

WorldContent ShippedWorld(std::vector<std::string>& Errors)
{
	const std::filesystem::path D(RB_DATA_DIR);
	WorldContent W;
	PeopleFromJson(Slurp(D / "People.json"), "People.json", W.People, Errors);
	StoryFromJson(Slurp(D / "Story.json"), "Story.json", W.Story, Errors);
	EventsFromJson(Slurp(D / "Events.json"), "Events.json", W.Events, W.Fallout, Errors);
	return W;
}

// A tiny world for rule tests.
WorldContent TestWorld()
{
	WorldContent W;
	CharacterDef Priya;
	Priya.Id = "priya";
	Priya.Name = "Priya Okafor";
	Priya.Role = "Risk Manager";
	Priya.StartRelation = 20;
	CharacterDef Sam;
	Sam.Id = "sam";
	Sam.Name = "Sam";
	Sam.Role = "Partner";
	Sam.bPersonal = true;
	Sam.StartRelation = 40;
	W.People.People = {Priya, Sam};
	W.Story.Title = "Test story";
	W.Story.Clues = {{"clue_a", "A", "Clue A", 1}, {"clue_b", "B", "Clue B", 1}};

	EventDef Dinner;
	Dinner.Id = "dinner";
	Dinner.Category = EMessageCategory::Personal;
	Dinner.From = "sam";
	Dinner.Subject = "Dinner?";
	Dinner.Body = "Thursday?";
	Dinner.When.Passed = {"M002"};
	Dinner.DeadlineWeeks = 2;
	Dinner.DefaultChoice = "forget";
	EventChoiceDef Go;
	Go.Id = "go";
	Go.Label = "Go";
	Go.Outcome = "Lovely evening.";
	Go.Relations = {{"sam", 10}};
	Go.Stress = -10;
	Go.Clue = "clue_b";
	EventChoiceDef Forget;
	Forget.Id = "forget";
	Forget.Label = "(No reply)";
	Forget.Outcome = "You forgot.";
	Forget.Relations = {{"sam", -15}};
	Dinner.Choices = {Go, Forget};
	EventDef News;
	News.Id = "news";
	News.Category = EMessageCategory::News;
	News.Subject = "Award";
	News.Body = "Valtren wins.";
	News.When.Metrics = {{EOrgMetric::Reputation, false, 70.0}};
	News.Headline = "Valtren wins award";
	News.HeadlineTone = 1;
	W.Events.Events = {Dinner, News};
	FalloutTemplate T;
	T.Department = "default";
	T.Description = "Fallout from \"{mission}\" after \"{choice}\".";
	T.Effects = {{EOrgMetric::Cash, -1000}, {EOrgMetric::Reputation, -2}};
	W.Fallout = {T};
	return W;
}

EMissionError Play(GameState& G, const MissionDef& M, const std::vector<MissionAnswer>& A, MissionDebrief* D, const MissionCatalogue* C = nullptr,
	const WorldContent* W = nullptr)
{
	const EMissionError E = StartMission(G, M, C);
	if (E != EMissionError::None) return E;
	for (const MissionEvidenceDef& Ev : M.Evidence) ViewMissionEvidence(G, M, Ev.Id, Ev.At);
	return SubmitMission(G, M, A, D, C, W, M.PresentAt);
}

std::vector<MissionAnswer> Best(const MissionDef& M)
{
	std::vector<MissionAnswer> Out;
	for (const MissionQuestionDef& Q : M.Questions) Out.push_back(BestAnswer(Q));
	return Out;
}

std::string Resign(std::string Body)
{
	Body = Body.substr(0, Body.rfind("checksum\t"));
	uint64_t H = 0xCBF29CE484222325ULL;
	for (unsigned char C : Body)
	{
		H ^= C;
		H *= 0x100000001B3ULL;
	}
	char Sum[32];
	std::snprintf(Sum, sizeof(Sum), "%016" PRIx64, H);
	return Body + "checksum\t" + Sum + "\n";
}
} // namespace

TEST(World_ShippedContentParsesAndValidates)
{
	std::vector<std::string> Errors;
	const WorldContent W = ShippedWorld(Errors);
	const MissionCatalogue C = ShippedMissions();
	for (const std::string& E : ValidateWorld(W, &C)) Errors.push_back(E);
	for (const std::string& E : Errors) std::printf("    %s\n", E.c_str());
	CHECK(Errors.empty());
	CHECK(W.People.People.size() >= 12);
	CHECK(W.Story.Clues.size() >= 8);
	CHECK(W.Events.Events.size() >= 30);
	REQUIRE(W.FalloutFor("No such department"));
	CHECK(W.FalloutFor("No such department")->Department == std::string("default"));
	for (const char* Id : {"S001", "S002", "S003", "S004"}) CHECK(C.Find(Id) != nullptr);
}

TEST(World_ValidatorRejectsBrokenReferences)
{
	WorldContent W = TestWorld();
	CHECK(ValidateWorld(W, nullptr).empty());
	W.Events.Events[0].DefaultChoice = "nope";
	W.Events.Events[0].Choices[0].Relations = {{"ghost", 5}};
	W.Events.Events[0].Choices[0].Clue = "unknown_clue";
	W.Events.Events[1].From = "nobody";
	W.Fallout[0].Department = "Finance";
	const auto E = ValidateWorld(W, nullptr);
	auto Has = [&](const char* S) { return std::any_of(E.begin(), E.end(), [&](const std::string& X) { return X.find(S) != std::string::npos; }); };
	CHECK(Has("defaultChoice"));
	CHECK(Has("unknown character 'ghost'"));
	CHECK(Has("unknown clue"));
	CHECK(Has("unknown sender"));
	CHECK(Has("\"default\" template"));
}

TEST(World_RelationsStressAndCluesFromChoices)
{
	const WorldContent W = TestWorld();
	MissionCatalogue C = TestCatalogue();
	MissionDef& M2 = C.Missions[0];
	M2.Questions[0].Options[0].Relations = {{"priya", 12}};
	M2.Questions[0].Options[0].Stress = 5;
	M2.Evidence[1].Clue = "clue_a";
	MissionDef& M3 = C.Missions[1];
	ContextNoteDef Note;
	Note.Text = "Priya trusts you now.";
	Note.RelationId = "priya";
	Note.RelationThreshold = 30;
	M3.Context.push_back(Note);
	ContextNoteDef ClueNote;
	ClueNote.Text = "Your file grows.";
	ClueNote.MinClues = 1;
	M3.Context.push_back(ClueNote);

	GameState G = AfterM001();
	InitWorld(G, W);
	CHECK_EQ(GetRelation(G, "priya"), 20);
	CHECK_EQ(G.World.Stress, StressStart);
	CHECK(BriefingFor(G, M3).size() == size_t(1));
	MissionDebrief D;
	REQUIRE(Play(G, M2, AllAnswers(M2, "good"), &D, &C, &W) == EMissionError::None);
	CHECK_EQ(GetRelation(G, "priya"), 32);
	CHECK(HasClue(G, "clue_a"));
	// +5 option, +4 mission, -3 x 2 weeks of recovery.
	CHECK_EQ(G.World.Stress, StressStart + 5 + StressPerMission - 2 * StressRecoveryPerWeek);
	const auto Brief = BriefingFor(G, M3);
	CHECK(std::find(Brief.begin(), Brief.end(), "Priya trusts you now.") != Brief.end());
	CHECK(std::find(Brief.begin(), Brief.end(), "Your file grows.") != Brief.end());
	CHECK(std::string(RelationLabel(GetRelation(G, "priya"))) == "Friendly");
	ChangeRelation(G, "priya", 500);
	CHECK_EQ(GetRelation(G, "priya"), RelationMax);
}

TEST(World_InboxDeadlinesApplyTheDefault)
{
	const WorldContent W = TestWorld();
	const MissionCatalogue C = TestCatalogue();
	GameState G = AfterM001();
	InitWorld(G, W);
	MissionDebrief D;
	REQUIRE(Play(G, *C.Find("M002"), AllAnswers(*C.Find("M002"), "good"), &D, &C, &W) == EMissionError::None);
	REQUIRE(D.NewMessages.size() == size_t(1));
	const InboxMessage* Msg = FindMessage(G, D.NewMessages[0]);
	REQUIRE(Msg);
	CHECK(Msg->NeedsReply());
	CHECK_EQ(Msg->DeadlineWeek, G.Org.Week + 2);
	CHECK_EQ(AwaitingReplyCount(G), 1);
	CHECK(ReplyToMessage(G, W, Msg->Id, "nope") == EReplyError::UnknownChoice);

	// Copy the state: one player replies, the other lets the deadline pass.
	GameState Ignored = G;
	CHECK(ReplyToMessage(G, W, D.NewMessages[0], "go") == EReplyError::None);
	CHECK(ReplyToMessage(G, W, D.NewMessages[0], "go") == EReplyError::AlreadyAnswered);
	CHECK_EQ(GetRelation(G, "sam"), 50);
	CHECK(HasClue(G, "clue_b"));

	REQUIRE(Play(Ignored, *C.Find("M003"), AllAnswers(*C.Find("M003"), "good"), &D, &C, &W) == EMissionError::None);
	UpdateWorld(Ignored, &C, W, AdvanceGameWeeks(Ignored, 1), 1); // now past the deadline week
	const InboxMessage* Late = FindMessage(Ignored, "MSG-1");
	REQUIRE(Late);
	CHECK(Late->bAutoResolved);
	CHECK(Late->ChoiceId == std::string("forget"));
	CHECK_EQ(GetRelation(Ignored, "sam"), 25);
	// Event fires once only.
	CHECK(std::count(Ignored.World.FiredEvents.begin(), Ignored.World.FiredEvents.end(), std::string("dinner")) == 1);
}

TEST(World_WeakAnswersLeaveFalloutTracedToTheDecision)
{
	const WorldContent W = TestWorld();
	const MissionCatalogue C = TestCatalogue();
	GameState G = AfterM001();
	G.Difficulty = EDifficulty::Executive;
	InitWorld(G, W);
	auto A = AllAnswers(*C.Find("M002"), "good");
	A[3].OptionIds = {"weak"}; // score 20: fallout probability 0.6 x 30/50 = 0.36, x1.2 Executive
	MissionDebrief D;
	REQUIRE(Play(G, *C.Find("M002"), A, &D, &C, &W) == EMissionError::None);
	REQUIRE(D.Fallout.size() == size_t(1));
	CHECK(D.Fallout[0].find("Title M002") != std::string::npos);
	CHECK(D.Fallout[0].find("Option weak") != std::string::npos);
	const auto It = std::find_if(G.Org.Pending.begin(), G.Org.Pending.end(), [](const PendingConsequence& P) { return P.Id.rfind("FALLOUT-", 0) == 0; });
	REQUIRE(It != G.Org.Pending.end());
	CHECK_NEAR(It->Probability, 0.36 * 1.2, 1e-9);
	CHECK(It->TriggerWeek >= G.Org.Week && It->TriggerWeek <= G.Org.Week + 6);

	// Good answers leave nothing behind.
	GameState Good = AfterM001();
	InitWorld(Good, W);
	REQUIRE(Play(Good, *C.Find("M002"), AllAnswers(*C.Find("M002"), "good"), &D, &C, &W) == EMissionError::None);
	CHECK(D.Fallout.empty());

	// Force the incident and check the inbox explains where it came from.
	It->Probability = 1.0;
	const auto Resolved = AdvanceGameWeeks(G, 8);
	const WorldUpdate U = UpdateWorld(G, &C, W, Resolved, 8);
	bool bFound = false;
	for (const std::string& Id : U.NewMessages)
	{
		const InboxMessage* M = FindMessage(G, Id);
		if (M && M->Category == EMessageCategory::Incident && M->Body.find("Traced to your decision in \"Title M002\"") != std::string::npos) bFound = true;
	}
	CHECK(bFound);
	CHECK(!G.World.News.empty());
}

TEST(World_CrisisBriefingNamesItsCauseAndEscalatesWhenIgnored)
{
	const WorldContent W = TestWorld();
	MissionCatalogue C = TestCatalogue();
	MissionDef* Crisis = nullptr;
	for (MissionDef& M : C.Missions)
	{
		if (M.Id == "C901") Crisis = &M;
	}
	REQUIRE(Crisis);
	Crisis->DeadlineWeeks = 3;
	Crisis->EscalationText = "The fire spread.";
	Crisis->EscalationEffects = {{EOrgMetric::Reputation, -5}};
	CHECK(ValidateCatalogue(C).empty());

	GameState G = AfterM001();
	InitWorld(G, W);
	auto A2 = AllAnswers(*C.Find("M002"), "good");
	A2[2].OptionIds = {"defer"}; // depot fire, certain, 3 weeks out
	MissionDebrief D;
	REQUIRE(Play(G, *C.Find("M002"), A2, &D, &C, &W) == EMissionError::None);
	REQUIRE(Play(G, *C.Find("M003"), AllAnswers(*C.Find("M003"), "good"), &D, &C, &W) == EMissionError::None);
	REQUIRE(G.Org.HasFlag("depot_fire"));
	REQUIRE(CheckAvailability(G, *Crisis).bAvailable);
	const auto Brief = BriefingFor(G, *Crisis);
	CHECK(Brief.back().find("traces back to your decision in \"Title M002\"") != std::string::npos);
	CHECK_EQ(WeeksUntilEscalation(G, *Crisis), 3);

	// Time passes without touching the crisis: it escalates and closes.
	const double Rep = G.Org.Get(EOrgMetric::Reputation);
	const WorldUpdate U = UpdateWorld(G, &C, W, AdvanceGameWeeks(G, 4), 4);
	REQUIRE(U.Escalated.size() == size_t(1));
	CHECK(G.Org.HasFlag("C901_escalated"));
	CHECK(G.Org.Get(EOrgMetric::Reputation) <= Rep - 5 + 1e-9);
	CHECK(!CheckAvailability(G, *Crisis).bAvailable);

	// A crisis the ladder needs can never be given a deadline.
	MissionCatalogue Bad = C;
	Bad.Missions.push_back(BasicMission("M090", {"C901"}));
	bool bRejected = false;
	for (const std::string& E : ValidateCatalogue(Bad)) bRejected = bRejected || E.find("can escalate") != std::string::npos;
	CHECK(bRejected);
}

TEST(World_BurnoutForcesLeaveAndTimePasses)
{
	const WorldContent W = TestWorld();
	const MissionCatalogue C = TestCatalogue();
	GameState G = AfterM001();
	InitWorld(G, W);
	G.World.Stress = 95;
	const int Week = G.Org.Week;
	const WorldUpdate U = UpdateWorld(G, &C, W, {}, 0);
	CHECK(U.bBurnout);
	CHECK_EQ(G.World.Burnouts, 1);
	CHECK_EQ(G.World.Stress, StressAfterLeave);
	CHECK_EQ(G.Org.Week, Week + BurnoutLeaveWeeks);
	CHECK(G.Org.HasFlag("burnout"));
}

TEST(World_ShareIndexAndNews)
{
	const WorldContent W = TestWorld();
	GameState G = NewGame("Shares", EDifficulty::Professional, 3);
	InitWorld(G, W);
	CHECK_NEAR(CurrentShareIndex(G), 100.0, 1e-9);
	REQUIRE(G.World.Shares.size() == size_t(1));
	G.Org.Apply({EOrgMetric::Reputation, 10});
	CHECK_NEAR(CurrentShareIndex(G), 104.0, 1e-9);
	G.Org.Apply({EOrgMetric::Cash, -4800000}); // 10% of revenue: -5 points
	CHECK_NEAR(CurrentShareIndex(G), 99.0, 1e-9);
	double Base[OrgMetricCount] = {}, Crash[OrgMetricCount] = {};
	Crash[5] = -1000;
	CHECK_NEAR(ShareIndex(Crash, Base), 20.0, 1e-9);
	// Reputation 72 opens the award headline.
	G.Org.Apply({EOrgMetric::Reputation, 10});
	UpdateWorld(G, nullptr, W, {}, 1);
	REQUIRE(!G.World.News.empty());
	CHECK(G.World.News.back().Text == std::string("Valtren wins award"));
	CHECK_EQ(G.World.News.back().Tone, 1);
}

TEST(Save_V6RoundTripsTheWorldAndMigratesV5)
{
	const WorldContent W = TestWorld();
	const MissionCatalogue C = TestCatalogue();
	GameState G = AfterM001();
	InitWorld(G, W);
	MissionDebrief D;
	REQUIRE(Play(G, *C.Find("M002"), AllAnswers(*C.Find("M002"), "good"), &D, &C, &W) == EMissionError::None);
	AddClue(G, "clue_a");
	ReplyToMessage(G, W, "MSG-1", "go");
	AddNews(G, "Line with\ttab and\nnewline", -1);
	G.World.CrisisOpened.push_back({"C901", 3});
	const std::string Text = SerializeGame(G);
	GameState L;
	LoadResult Info;
	REQUIRE(DeserializeGame(Text, L, Info));
	CHECK(!Info.bMigrated);
	CHECK(SerializeGame(L) == Text);
	CHECK_EQ(L.World.Inbox.size(), G.World.Inbox.size());
	CHECK(L.World.News.back().Text == std::string("Line with\ttab and\nnewline"));
	CHECK_EQ(GetRelation(L, "sam"), 50);

	// A v5 save has no world keys: it loads with defaults and InitWorld fills relationships.
	std::string V5;
	std::istringstream In(Text);
	std::string Line;
	while (std::getline(In, Line))
	{
		if (Line.rfind("world.", 0) == 0 || Line.rfind("checksum", 0) == 0) continue;
		if (Line.find(".setflag\t") != std::string::npos && Line.rfind("org.hist.", 0) == 0) continue;
		if (Line.rfind("version\t", 0) == 0) Line = "version\t5";
		V5 += Line + "\n";
	}
	GameState M;
	REQUIRE(DeserializeGame(Resign(V5 + "checksum\t0\n"), M, Info));
	CHECK(Info.bMigrated);
	CHECK_EQ(Info.LoadedVersion, 5);
	CHECK_EQ(M.World.Stress, StressStart);
	CHECK(M.World.Inbox.empty());
	InitWorld(M, W);
	CHECK_EQ(GetRelation(M, "priya"), 20);

	// Hostile values are rejected.
	std::string Bad = Text;
	const size_t P = Bad.find("world.stress\t");
	REQUIRE(P != std::string::npos);
	Bad.replace(P, Bad.find('\n', P) - P, "world.stress\t500");
	CHECK(!DeserializeGame(Resign(Bad), M, Info));
}

TEST(World_FullStoryCanBeSolvedWithTheShippedContent)
{
	std::vector<std::string> Errors;
	const WorldContent W = ShippedWorld(Errors);
	const MissionCatalogue C = ShippedMissions();
	REQUIRE(Errors.empty());
	GameState G = NewGame("Story", EDifficulty::Professional, 11);
	InitWorld(G, W);
	// Mission 001, best play.
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
	UpdateWorld(G, &C, W, {}, 0);

	// A diligent player: answers every message, preferring replies that gather evidence and look after people.
	auto AnswerInbox = [&]()
	{
		for (const InboxMessage& M : std::vector<InboxMessage>(G.World.Inbox))
		{
			if (!M.NeedsReply()) continue;
			const EventDef* E = W.Events.Find(M.EventId);
			REQUIRE(E);
			const EventChoiceDef* Pick = &E->Choices.front();
			for (const EventChoiceDef& Ch : E->Choices)
			{
				if (!Ch.Clue.empty()) Pick = &Ch;
			}
			CHECK(ReplyToMessage(G, W, M.Id, Pick->Id) == EReplyError::None);
		}
	};
	for (int Guard = 0; Guard < 250; ++Guard)
	{
		AnswerInbox();
		const auto Open = AvailableMissions(G, C);
		if (Open.empty()) break;
		MissionDebrief D;
		REQUIRE(Play(G, *Open.front(), Best(*Open.front()), &D, &C, &W) == EMissionError::None);
		CHECK(D.bPassed);
		CHECK(D.Fallout.empty());
	}
	for (const char* Id : {"S001", "S002", "S003", "S004", "CRO"})
	{
		if (!G.Career.HasPassed(Id)) std::printf("    not passed: %s (clues %zu)\n", Id, G.World.Clues.size());
		CHECK(G.Career.HasPassed(Id));
	}
	CHECK(G.Org.HasFlag("story_exposed"));
	CHECK_EQ(G.Career.Level, 10);
	CHECK(G.World.Inbox.size() >= size_t(15));
	CHECK(G.World.Shares.size() >= size_t(20));
	GameState L;
	LoadResult Info;
	CHECK(DeserializeGame(SerializeGame(G), L, Info));
	CHECK(SerializeGame(L) == SerializeGame(G));
}

TEST(World_EveryCaseIsWorkedOnFoot)
{
	// Like Mission 001: no case can be finished from the desk. Every piece of evidence is somewhere in the
	// office, each case needs at least two rooms, and at least one colleague is met in person.
	const MissionCatalogue C = ShippedMissions();
	std::vector<std::string> Errors;
	const WorldContent W = ShippedWorld(Errors);
	CHECK(Errors.empty());
	CHECK(C.Missions.size() >= size_t(50));
	for (const MissionDef& M : C.Missions)
	{
		std::vector<std::string> Rooms;
		int32_t Interviews = 0;
		for (const MissionEvidenceDef& E : M.Evidence)
		{
			if (E.At.empty()) std::printf("    %s/%s: read at the desk\n", M.Id.c_str(), E.Id.c_str());
			CHECK(!E.At.empty());
			if (std::find(Rooms.begin(), Rooms.end(), E.At) == Rooms.end()) Rooms.push_back(E.At);
			if (E.Person.empty()) continue;
			++Interviews;
			const CharacterDef* P = W.People.Find(E.Person);
			CHECK(P != nullptr);
			if (P) CHECK(!P->bPersonal); // the player's partner is never met at work
		}
		if (Rooms.size() < 2 || Interviews == 0) std::printf("    %s: %zu rooms, %d interviews\n", M.Id.c_str(), Rooms.size(), Interviews);
		CHECK(Rooms.size() >= size_t(2));
		CHECK(Interviews >= 1);
	}
}
