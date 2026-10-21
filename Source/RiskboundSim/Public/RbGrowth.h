// The player as a person: background, skill tree and conversations (phase 3).
// Skills and backgrounds change the living world (stress, relationships, fallout, deadlines, which
// replies are available), never a rubric score or a promotion gate: promotion stays competence
// evidence and passed missions (Docs/CareerModel.md). Conversations: Data/Dialogue.json.
#pragma once

#include "RbApi.h"

#include "RbOrganization.h"
#include "RbPeople.h"
#include "RbWorld.h"

#include <string>
#include <vector>

namespace rb
{
struct GameState;

// ---------------------------------------------------------------- skills
struct SkillDef
{
	const char* Id;
	const char* Branch;
	const char* Name;
	const char* Description;
	const char* Requires; // skill id needed first, or ""
};

RB_API const std::vector<SkillDef>& SkillDefs();
RB_API const SkillDef* FindSkill(const std::string& Id);
RB_API bool HasSkill(const GameState& State, const std::string& Id);
// Points earned: one per promotion above Junior Risk Analyst and one per four distinct missions
// passed (M001 counts), plus the background's bonus; minus skills learned.
RB_API int SkillPointsEarned(const GameState& State);
RB_API int SkillPointsAvailable(const GameState& State);

enum class ESkillError
{
	None,
	Unknown,
	AlreadyLearned,
	NeedsPrerequisite,
	NoPoints
};

RB_API const char* ToString(ESkillError E);
RB_API ESkillError LearnSkill(GameState& State, const std::string& Id);

// Modifiers the world rules read (all 1.0 / 0 without the skill).
RB_API double StressGainFactor(const GameState& State);      // composure: 0.75
RB_API int ExtraRecoveryPerWeek(const GameState& State);     // self_care: +2
RB_API double RelationGainFactor(const GameState& State);    // networker: 1.25
RB_API double RelationLossFactor(const GameState& State);    // diplomat: 0.75
RB_API int ExtraCrisisWeeks(const GameState& State);         // crisis_commander: +2
RB_API double FalloutFactor(const GameState& State);         // controls_engineer: 0.75

// ---------------------------------------------------------------- backgrounds
struct BackgroundDef
{
	const char* Id;
	const char* Name;
	const char* Description;
	const char* StartingSkill;                // learned free at the start, or ""
	int BonusPoints;
	std::vector<RelationEffect> Relations;    // how people take to you at first
};

RB_API const std::vector<BackgroundDef>& BackgroundDefs();
RB_API const BackgroundDef* FindBackground(const std::string& Id);
// Applies a background to a new game (after InitWorld). False if unknown.
RB_API bool ApplyBackground(GameState& State, const std::string& Id);

// ---------------------------------------------------------------- conversations
enum class ETone
{
	Diplomatic,
	Direct,
	Analytical,
	Count
};

RB_API const char* ToString(ETone T);

struct DialogueResponseDef
{
	std::string Id;
	std::string Label;
	ETone Tone = ETone::Diplomatic;
	std::string Next;                      // node id, empty ends the conversation
	std::string RequiresSkill;             // shown only with this skill
	std::vector<MetricEffect> Effects;
	std::vector<RelationEffect> Relations;
	int Stress = 0;
	std::vector<std::string> SetsFlags;
	std::string Clue;
};

struct DialogueNodeDef
{
	std::string Id;
	std::string Line;                      // what the other person says
	std::vector<DialogueResponseDef> Responses;
};

struct ConversationDef
{
	std::string Id;
	std::string Person;                    // character id
	std::string Title;
	std::string Trigger;                   // "evidence:<mission>/<evidence>" (before an interview) or "talk" (when you talk to them)
	WorldCondition When;
	std::vector<DialogueNodeDef> Nodes;    // the first node starts the conversation
};

struct DialogueCatalogue
{
	std::vector<ConversationDef> Conversations;
	RB_API const ConversationDef* Find(const std::string& Id) const;
};

// The conversation to play now for a trigger, or null: not yet had, condition holds.
RB_API const ConversationDef* ConversationFor(const GameState& State, const DialogueCatalogue& C, const std::string& Person, const std::string& Trigger);
// Responses the player may choose at a node (skill-gated ones only with the skill).
RB_API std::vector<const DialogueResponseDef*> AvailableResponses(const GameState& State, const DialogueNodeDef& Node);

enum class EDialogueError
{
	None,
	UnknownConversation,
	UnknownNode,
	UnknownResponse,
	NeedsSkill
};

RB_API const char* ToString(EDialogueError E);
// Applies a response; OutNext is the next node id ("" when the conversation ends, which marks it had).
RB_API EDialogueError ChooseResponse(GameState& State, const DialogueCatalogue& C, const std::string& ConversationId, const std::string& NodeId,
	const std::string& ResponseId, std::string& OutNext);

RB_API bool DialogueFromJson(const std::string& Text, const std::string& SourceName, DialogueCatalogue& Out, std::vector<std::string>& OutErrors);
RB_API std::vector<std::string> ValidateDialogue(const DialogueCatalogue& C, const PeopleCatalogue& People, const StoryCatalogue& Story);
} // namespace rb
