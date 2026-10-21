// Phase 4: glossary, endings, performance review, certificate, what-if, challenges, leaderboard.
#include "RbTest.h"

#include "CampaignFixture.h"

#include "RbLearning.h"
#include "RbMissionData.h"
#include "RbSave.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace rb;
using namespace rbtest;

namespace
{
std::string ReadData(const std::string& Name)
{
	std::ifstream In(std::filesystem::path(RB_DATA_DIR) / Name, std::ios::binary);
	std::stringstream Ss;
	Ss << In.rdbuf();
	return Ss.str();
}

MissionCatalogue Shipped()
{
	std::vector<std::pair<std::string, std::string>> Files;
	for (const auto& E : std::filesystem::directory_iterator(std::filesystem::path(RB_DATA_DIR) / "Missions"))
	{
		if (E.path().extension() == ".json") Files.push_back({E.path().filename().string(), ReadData("Missions/" + E.path().filename().string())});
	}
	std::sort(Files.begin(), Files.end());
	MissionCatalogue C;
	std::vector<std::string> Errors;
	LoadMissionCatalogue(Files, C, Errors);
	return C;
}
} // namespace

TEST(Learning_GlossaryAndEndingsValidate)
{
	const MissionCatalogue C = Shipped();
	std::vector<std::string> Errors;
	std::vector<GlossaryTerm> Terms;
	GlossaryFromJson(ReadData("Glossary.json"), "Glossary.json", Terms, Errors);
	for (const std::string& E : ValidateGlossary(Terms, &C)) Errors.push_back(E);
	std::vector<EndingDef> Endings;
	EndingsFromJson(ReadData("Endings.json"), "Endings.json", Endings, Errors);
	for (const std::string& E : ValidateEndings(Endings)) Errors.push_back(E);
	for (const std::string& E : Errors) std::printf("    %s\n", E.c_str());
	CHECK(Errors.empty());
	CHECK(Terms.size() >= size_t(40));
	CHECK(!TermsForMission(Terms, "M004").empty());

	GameState G = AfterM001();
	REQUIRE(ChooseEnding(G, Endings));
	G.Org.SetFlag("story_exposed");
	G.Org.Metrics[static_cast<int>(EOrgMetric::Reputation)] = 70;
	CHECK(ChooseEnding(G, Endings)->Id == "clean_sweep");
	G.Org.Metrics[static_cast<int>(EOrgMetric::Reputation)] = 40;
	CHECK(ChooseEnding(G, Endings)->Id == "costly_truth");
	G.Org.SetFlag("story_compromised");
	CHECK(ChooseEnding(G, Endings)->Id == "golden_cage");
}

TEST(Learning_ReviewCertificateAndWhatIf)
{
	const MissionCatalogue C = TestCatalogue();
	GameState G = AfterM001();
	auto A = AllAnswers(*C.Find("M002"), "good");
	A[2].OptionIds = {"defer"};
	REQUIRE(StartMission(G, *C.Find("M002")) == EMissionError::None);
	ViewMissionEvidence(G, *C.Find("M002"), "ev1");
	REQUIRE(SubmitMission(G, *C.Find("M002"), A) == EMissionError::None);
	PeopleCatalogue People;
	const PerformanceReview R = MakeReview(G, People);
	CHECK_EQ(R.MissionsPassed, 2);
	CHECK(!R.Strengths.empty());
	CHECK(R.Summary.find("cases") != std::string::npos);
	const std::string Cert = CertificateMarkdown(G, R, nullptr, "2026-10-21");
	CHECK(Cert.find("# Riskbound career certificate") == 0);
	CHECK(Cert.find("not a professional qualification") != std::string::npos);

	const auto W = WhatIf(*C.Find("M002"), G.Campaign.Records.back());
	REQUIRE(W.size() == size_t(4));
	const auto& Controls = W[2].Options;
	const auto Defer = std::find_if(Controls.begin(), Controls.end(), [](const WhatIfOption& O) { return O.Label == "Option defer"; });
	REQUIRE(Defer != Controls.end());
	CHECK(Defer->bChosen);
	CHECK(!Defer->Consequences.empty());
	CHECK(Defer->Consequences[0].find("Depot fire") != std::string::npos);
}

TEST(Learning_SurvivalAndDailyChallenges)
{
	const MissionCatalogue C = Shipped();
	GameState G = NewGame("Challenger", EDifficulty::Professional, 9);
	StartChallenge(G, C, EGameMode::Survival);
	CHECK_EQ(G.Challenge.Sequence.size(), size_t(12));
	const std::string First = CurrentChallengeMission(G);
	REQUIRE(!First.empty());
	CHECK(AvailableMissions(G, C).size() == size_t(1));
	CHECK(AvailableMissions(G, C).front()->Id == First);
	int Guard = 0;
	while (!G.Challenge.bOver && Guard++ < 20)
	{
		const MissionDef* M = C.Find(CurrentChallengeMission(G));
		REQUIRE(M);
		REQUIRE(StartMission(G, *M, &C) == EMissionError::None);
		for (const MissionEvidenceDef& E : M->Evidence) ViewMissionEvidence(G, *M, E.Id, E.At);
		std::vector<MissionAnswer> Worst;
		for (const MissionQuestionDef& Q : M->Questions)
		{
			MissionAnswer A = BestAnswer(Q);
			if (Q.Kind == EQuestionKind::Single)
			{
				A.OptionIds = {std::min_element(Q.Options.begin(), Q.Options.end(), [](const MissionOptionDef& L, const MissionOptionDef& R) { return L.Score < R.Score; })->Id};
			}
			Worst.push_back(A);
		}
		MissionDebrief D;
		REQUIRE(SubmitMission(G, *M, Worst, &D, &C, nullptr, M->PresentAt) == EMissionError::None);
	}
	CHECK(G.Challenge.bOver);
	CHECK(G.Challenge.Lives <= 0 || G.Challenge.Index == static_cast<int>(G.Challenge.Sequence.size()));
	GameState L;
	LoadResult Info;
	REQUIRE(DeserializeGame(SerializeGame(G), L, Info));
	CHECK(L.Challenge.Mode == EGameMode::Survival && L.Challenge.bOver);

	GameState D1 = NewGame("A", EDifficulty::Professional, 1), D2 = NewGame("B", EDifficulty::Professional, 2);
	StartChallenge(D1, C, EGameMode::Daily, "2026-10-21");
	StartChallenge(D2, C, EGameMode::Daily, "2026-10-21");
	CHECK_EQ(D1.Challenge.Sequence.size(), size_t(3));
	CHECK(D1.Challenge.Sequence == D2.Challenge.Sequence); // same day, same cases for everyone
}

TEST(Learning_LeaderboardKeepsTheBestAndSurvivesBadLines)
{
	std::vector<LeaderboardEntry> L;
	for (int i = 0; i < 25; ++i) AddToLeaderboard(L, {"survival", "P" + std::to_string(i), "", i, 0, "2026-10-21"});
	AddToLeaderboard(L, {"career", "Me\twith tab", "Clean Sweep", 88, 90, "2026-10-21"});
	CHECK_EQ(L.size(), size_t(21));
	const std::string Text = SerializeLeaderboard(L) + "garbage line\n";
	const auto Back = DeserializeLeaderboard(Text);
	CHECK_EQ(Back.size(), size_t(21));
	const auto Top = std::find_if(Back.begin(), Back.end(), [](const LeaderboardEntry& E) { return E.Mode == "survival"; });
	REQUIRE(Top != Back.end());
	CHECK_EQ(Top->Points, 24);
	CHECK(DeserializeLeaderboard("not a leaderboard").empty());
}

#include "RbAiPrompt.h"
#include "RbJson.h"

TEST(Ai_PromptRequestAndResponseParsing)
{
	GameState G = AfterM001();
	CharacterDef P;
	P.Id = "priya";
	P.Name = "Priya Okafor";
	P.Role = "Risk Manager";
	P.Bio = "Your first manager.";
	const MissionCatalogue C = TestCatalogue();
	const std::string Sys = ColleagueSystemPrompt(G, P, C.Find("M002"));
	CHECK(Sys.find("Priya Okafor") != std::string::npos);
	CHECK(Sys.find("Do not give the player the answer") != std::string::npos);
	CHECK(Sys.find("Title M002") != std::string::npos);
	std::vector<AiTurn> Turns = {{false, "Hello"}, {true, "What worries you about \"Kelvarin\"?\nBe honest."}};
	const std::string Body = ColleagueRequestJson(Sys, Turns);
	JsonValue Doc;
	std::string Err;
	REQUIRE(ParseJson(Body, Doc, Err));
	CHECK(Doc.Find("model")->String == "claude-opus-5-5");
	REQUIRE(Doc.Find("messages")->Items.size() == size_t(1)); // starts with the player
	CHECK(Doc.Find("messages")->Items[0].Find("content")->String.find("\"Kelvarin\"") != std::string::npos);

	std::string Text, Error;
	CHECK(ParseColleagueResponse(R"({"type":"message","content":[{"type":"thinking","thinking":""},{"type":"text","text":"Start with the service log."}],"stop_reason":"end_turn"})", Text, Error));
	CHECK(Text == "Start with the service log.");
	CHECK(!ParseColleagueResponse(R"({"type":"error","error":{"type":"authentication_error","message":"invalid x-api-key"}})", Text, Error));
	CHECK(Error == "invalid x-api-key");
	CHECK(!ParseColleagueResponse(R"({"content":[],"stop_reason":"refusal"})", Text, Error));
	CHECK(!ParseColleagueResponse("not json", Text, Error));
}
