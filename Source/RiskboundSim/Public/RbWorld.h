// The living world around the campaign: relationships, the player's stress, the story
// investigation, the inbox (events that interrupt, some with reply deadlines), fallout from weak
// decisions, crisis escalation, news headlines and the share index.
// Content is data: Data/People.json, Data/Story.json, Data/Events.json (Docs/WorldData.md).
// Rules: Docs/GameDesign.md "Living world". Deterministic: every draw uses the organisation's
// seeded event stream, so a reload continues the same sequence.
#pragma once

#include "RbApi.h"

#include "RbCampaign.h"
#include "RbOrganization.h"
#include "RbPeople.h"

#include <string>
#include <utility>
#include <vector>

namespace rb
{
struct GameState;

// ---------------------------------------------------------------- content

struct ClueDef
{
	std::string Id;
	std::string Title;
	std::string Summary; // what the clue tells the player, shown on the case board
	int Chapter = 1;     // story chapter the clue belongs to (1..4)
};

struct StoryCatalogue
{
	std::string Title;   // "The Corvane Ledger"
	std::vector<ClueDef> Clues;
	RB_API const ClueDef* Find(const std::string& Id) const;
};

// A condition shared by events. Every listed part must hold.
struct WorldCondition
{
	struct MetricTest
	{
		EOrgMetric Metric = EOrgMetric::Reputation;
		bool bBelow = true;
		double Threshold = 0.0;
	};
	struct RelationTest
	{
		std::string CharacterId;
		bool bBelow = true;
		int Threshold = 0;
	};
	std::vector<std::string> Flags;
	std::vector<std::string> NotFlags;
	std::vector<std::string> Passed;     // missions passed
	int MinWeek = 0;
	int MinLevel = 0;
	int MaxLevel = 0;                    // 0: no upper limit
	int MinClues = 0;
	int MinStress = 0;                   // 0: no condition
	int MinIncidents = 0;                // consequences (any source) that occurred
	std::vector<MetricTest> Metrics;
	std::vector<RelationTest> Relations;
};

enum class EMessageCategory
{
	Story,
	Incident,
	Regulator,
	Personal,
	Politics,
	Team,
	News,
	Count
};

RB_API const char* ToString(EMessageCategory C);

struct EventChoiceDef
{
	std::string Id;
	std::string Label;
	std::string Outcome;                 // shown after the reply
	std::vector<MetricEffect> Effects;
	std::vector<RelationEffect> Relations;
	int Stress = 0;
	std::vector<std::string> SetsFlags;
	std::string Clue;
	std::vector<ConsequenceDef> Schedules;
	std::string RequiresSkill;           // offered only with this skill (RbGrowth.h)
};

struct EventDef
{
	std::string Id;
	EMessageCategory Category = EMessageCategory::Team;
	std::string From;                    // character id (Data/People.json)
	std::string Subject;
	std::string Body;
	WorldCondition When;
	double Probability = 1.0;            // chance per elapsed week once the condition holds
	int DeadlineWeeks = 0;               // 0: no reply needed; else weeks to reply before DefaultChoice applies
	std::string DefaultChoice;           // applied when the deadline passes (usually the worst outcome)
	std::vector<EventChoiceDef> Choices;
	std::string Clue;                    // gained on delivery (letters that are themselves evidence)
	std::string Headline;                // news headline when delivered, may be empty
	int HeadlineTone = 0;                // -1 bad, 0 neutral, +1 good
};

struct EventCatalogue
{
	std::vector<EventDef> Events;
	RB_API const EventDef* Find(const std::string& Id) const;
};

// Generic incident text for latent risk left by weak answers, by mission department.
struct FalloutTemplate
{
	std::string Department;              // matches MissionDef::Department; "default" is the fallback
	std::string Description;             // "{mission}" and "{choice}" are replaced
	std::string Headline;
	std::vector<MetricEffect> Effects;   // per career level of the mission (Cash scales with level)
};

struct WorldContent
{
	PeopleCatalogue People;
	StoryCatalogue Story;
	EventCatalogue Events;
	std::vector<FalloutTemplate> Fallout;
	RB_API const FalloutTemplate* FalloutFor(const std::string& Department) const;
};

// ---------------------------------------------------------------- player state (save v6)

struct InboxMessage
{
	std::string Id;                      // "MSG-<n>"
	std::string EventId;                 // empty for system messages (incidents, escalations, burnout)
	EMessageCategory Category = EMessageCategory::Team;
	std::string From;
	std::string Subject;
	std::string Body;
	int Week = 0;
	int DeadlineWeek = -1;               // -1: no reply needed
	bool bRead = false;
	std::string ChoiceId;                // reply chosen (or applied by default)
	std::string Outcome;
	bool bAutoResolved = false;          // the deadline passed and the default applied
	bool NeedsReply() const { return DeadlineWeek >= 0 && ChoiceId.empty(); }
};

struct NewsItem
{
	int Week = 0;
	std::string Text;
	int Tone = 0;
};

struct SharePoint
{
	int Week = 0;
	double Value = 100.0;
};

struct WorldState
{
	std::vector<Relation> Relations;
	int Stress = 20;                     // 0..100
	int Burnouts = 0;
	std::vector<std::string> Clues;
	std::vector<InboxMessage> Inbox;     // oldest first
	std::vector<std::string> FiredEvents;
	std::vector<NewsItem> News;          // oldest first, capped at NewsCap
	std::vector<SharePoint> Shares;      // one point per world update, capped at ShareCap
	std::vector<std::pair<std::string, int>> CrisisOpened; // crisis id -> week it became available
	int NextMessage = 1;
	// Phase 3 (save v7): who the player is and what they have learned (RbGrowth.h).
	std::string Background;
	std::vector<std::string> Skills;
	std::vector<std::string> ConversationsHad;
};

// ---------------------------------------------------------------- tuning (Docs/GameDesign.md "Living world")

constexpr int StressMax = 100;
constexpr int StressStart = 20;
constexpr int StressPerMission = 4;
constexpr int StressPerCrisis = 12;
constexpr int StressPerCapstone = 8;
constexpr int StressRecoveryPerWeek = 3;
constexpr int StressBurnout = 90;     // at or above: forced leave
constexpr int BurnoutLeaveWeeks = 2;
constexpr int StressAfterLeave = 40;
constexpr size_t NewsCap = 40;
constexpr size_t ShareCap = 104;

// ---------------------------------------------------------------- rules

// Relationship start values for every known character (new game, or a migrated save).
RB_API void InitWorld(GameState& State, const WorldContent& Content);
RB_API int GetRelation(const GameState& State, const std::string& CharacterId);
RB_API void ChangeRelation(GameState& State, const std::string& CharacterId, int Delta);
RB_API void ChangeStress(GameState& State, int Delta);
RB_API bool HasClue(const GameState& State, const std::string& ClueId);
RB_API bool AddClue(GameState& State, const std::string& ClueId); // false if already held or empty
RB_API int IncidentsOccurred(const GameState& State);
RB_API bool ConditionHolds(const GameState& State, const WorldCondition& C);

RB_API std::string PostMessage(GameState& State, InboxMessage M); // assigns the id; returns it
RB_API void AddNews(GameState& State, const std::string& Text, int Tone);
RB_API int UnreadCount(const GameState& State);
RB_API int AwaitingReplyCount(const GameState& State);
RB_API const InboxMessage* FindMessage(const GameState& State, const std::string& Id);
RB_API bool MarkMessageRead(GameState& State, const std::string& Id);

enum class EReplyError
{
	None,
	UnknownMessage,
	NoReplyNeeded,
	AlreadyAnswered,
	UnknownChoice,
	NeedsSkill
};

RB_API const char* ToString(EReplyError E);
// Applies the choice's effects, relations, stress, flags, clue and scheduled consequences.
RB_API EReplyError ReplyToMessage(GameState& State, const WorldContent& Content, const std::string& MessageId, const std::string& ChoiceId);

// Share index for the organisation now (RbQuant ShareIndex against the profile's start metrics).
RB_API double CurrentShareIndex(const GameState& State);

struct WorldUpdate
{
	std::vector<std::string> NewMessages;  // ids, in delivery order
	std::vector<std::string> AutoResolved; // message ids whose deadline passed
	std::vector<std::string> Escalated;    // crisis ids
	bool bBurnout = false;
};

// Runs after organisation time has advanced by WeeksElapsed (Resolved: what came due):
// incident and escalation messages and news, overdue replies, crisis deadlines, event delivery,
// stress recovery and burnout (which itself passes BurnoutLeaveWeeks), and a share index point.
RB_API WorldUpdate UpdateWorld(GameState& State, const MissionCatalogue* Missions, const WorldContent& Content,
	const std::vector<ResolvedConsequence>& Resolved, int WeeksElapsed);

// Content validation: every problem as "<file>: <message>". Empty means valid. Checks ids,
// references (characters, clues, choices, default choices, metrics) and value ranges.
RB_API std::vector<std::string> ValidateWorld(const WorldContent& Content, const MissionCatalogue* Missions);

// Content files (strict like mission files). Each returns true only if there are no errors.
RB_API bool PeopleFromJson(const std::string& Text, const std::string& SourceName, PeopleCatalogue& Out, std::vector<std::string>& OutErrors);
RB_API bool StoryFromJson(const std::string& Text, const std::string& SourceName, StoryCatalogue& Out, std::vector<std::string>& OutErrors);
RB_API bool EventsFromJson(const std::string& Text, const std::string& SourceName, EventCatalogue& OutEvents,
	std::vector<FalloutTemplate>& OutFallout, std::vector<std::string>& OutErrors);
} // namespace rb
