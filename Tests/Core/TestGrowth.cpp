// Phase 3: backgrounds, the skill tree and conversations (RbGrowth).
#include "RbTest.h"

#include "CampaignFixture.h"

#include "RbGrowth.h"
#include "RbSave.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace rb;
using namespace rbtest;

namespace
{
std::string ReadData(const char* Name)
{
	std::ifstream In(std::filesystem::path(RB_DATA_DIR) / Name, std::ios::binary);
	std::stringstream Ss;
	Ss << In.rdbuf();
	return Ss.str();
}
} // namespace

TEST(Growth_SkillsAreEarnedLearnedInOrderAndSaved)
{
	GameState G = AfterM001(); // level 2, M001 passed
	CHECK_EQ(SkillPointsEarned(G), 0);
	CHECK(LearnSkill(G, "composure") == ESkillError::NoPoints);
	G.Career.Level = 4;
	for (const char* M : {"M002", "M003", "M004"}) G.Career.RecordMission(M, true, 0);
	CHECK_EQ(SkillPointsEarned(G), 2 + 1); // two promotions, four passed missions
	CHECK(LearnSkill(G, "self_care") == ESkillError::NeedsPrerequisite);
	CHECK(LearnSkill(G, "composure") == ESkillError::None);
	CHECK(LearnSkill(G, "composure") == ESkillError::AlreadyLearned);
	CHECK(LearnSkill(G, "self_care") == ESkillError::None);
	CHECK(LearnSkill(G, "nope") == ESkillError::Unknown);
	CHECK_EQ(SkillPointsAvailable(G), 1);

	// Composure: stress gains a quarter smaller; self-care: faster recovery.
	G.World.Stress = 20;
	ChangeStress(G, 12);
	CHECK_EQ(G.World.Stress, 29);
	CHECK_EQ(ExtraRecoveryPerWeek(G), 2);

	GameState L;
	LoadResult Info;
	REQUIRE(DeserializeGame(SerializeGame(G), L, Info));
	CHECK(HasSkill(L, "self_care"));
}

TEST(Growth_BackgroundsGiveASkillAndFirstImpressions)
{
	GameState G = AfterM001();
	WorldContent W;
	CharacterDef Ruth;
	Ruth.Id = "ruth";
	Ruth.Name = "Ruth";
	Ruth.Role = "Compliance";
	Ruth.StartRelation = 10;
	W.People.People = {Ruth};
	InitWorld(G, W);
	CHECK(!ApplyBackground(G, "astronaut"));
	CHECK(ApplyBackground(G, "auditor"));
	CHECK(HasSkill(G, "controls_engineer"));
	CHECK_EQ(GetRelation(G, "ruth"), 25);
	CHECK_EQ(SkillPointsAvailable(G), 0); // the background's skill is free, not a spend
	CHECK_NEAR(FalloutFactor(G), 0.75, 1e-12);
	GameState Grad = AfterM001();
	CHECK(ApplyBackground(Grad, "graduate"));
	CHECK_EQ(SkillPointsAvailable(Grad), 1);
	CHECK_EQ(BackgroundDefs().size(), size_t(4));
}

TEST(Growth_ShippedDialogueValidatesAndPlaysThrough)
{
	std::vector<std::string> Errors;
	WorldContent W;
	PeopleFromJson(ReadData("People.json"), "People.json", W.People, Errors);
	StoryFromJson(ReadData("Story.json"), "Story.json", W.Story, Errors);
	DialogueCatalogue D;
	DialogueFromJson(ReadData("Dialogue.json"), "Dialogue.json", D, Errors);
	for (const std::string& E : ValidateDialogue(D, W.People, W.Story)) Errors.push_back(E);
	for (const std::string& E : Errors) std::printf("    %s\n", E.c_str());
	CHECK(Errors.empty());
	CHECK(D.Conversations.size() >= size_t(8));

	GameState G = AfterM001();
	InitWorld(G, W);
	CHECK(ConversationFor(G, D, "priya", "talk") == nullptr); // needs M002
	G.Career.RecordMission("M002", true, 0);
	const ConversationDef* C = ConversationFor(G, D, "priya", "talk");
	REQUIRE(C);
	CHECK(C->Id == "priya_first_chat");
	const int Before = GetRelation(G, "priya");
	std::string Next;
	CHECK(ChooseResponse(G, D, C->Id, "start", "honest", Next) == EDialogueError::None);
	CHECK(Next == "evidence");
	CHECK(ChooseResponse(G, D, C->Id, "evidence", "push", Next) == EDialogueError::None);
	CHECK(ChooseResponse(G, D, C->Id, "people", "favours", Next) == EDialogueError::None);
	CHECK(Next.empty());
	CHECK_EQ(GetRelation(G, "priya"), Before + 4 + 2 + 3);
	CHECK(G.Org.HasFlag("priya_politics_talk"));
	CHECK(ConversationFor(G, D, "priya", "talk") == nullptr); // had it

	// Skill-gated replies are hidden and refused without the skill.
	const ConversationDef* Amara = ConversationFor(G, D, "amara", "evidence:M003/excursions");
	REQUIRE(Amara);
	const DialogueNodeDef& Data = *std::find_if(Amara->Nodes.begin(), Amara->Nodes.end(), [](const DialogueNodeDef& N) { return N.Id == "data"; });
	CHECK_EQ(AvailableResponses(G, Data).size(), size_t(1));
	CHECK(ChooseResponse(G, D, Amara->Id, "data", "deal", Next) == EDialogueError::NeedsSkill);
	G.World.Skills.push_back("negotiator");
	CHECK_EQ(AvailableResponses(G, Data).size(), size_t(2));
	CHECK(ChooseResponse(G, D, Amara->Id, "data", "deal", Next) == EDialogueError::None);
	CHECK(G.Org.HasFlag("amara_driver_case"));
}
