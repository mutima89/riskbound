// Campaign: generic decision missions after Mission 001, with career progression and
// organisational consequences that carry from one mission to the next.
// Mission content is data (MissionDef); rules live here. Canonical rules: Docs/CareerModel.md
// ("Campaign progression"). Rubric scores stay hidden until SubmitMission().
#pragma once

#include "RbApi.h"

#include "RbCareer.h"
#include "RbOrganization.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rb
{
struct GameState;

enum class EMissionKind
{
	Mission,
	Crisis,
	Capstone
};

RB_API const char* ToString(EMissionKind K);

enum class EQuestionKind
{
	Single, // pick one option; the option's Score is the result
	Multi   // pick all that apply; scored against the options marked bCorrect
};

// A consequence a choice schedules for later (resolved by OrganizationState::AdvanceWeeks).
struct ConsequenceDef
{
	std::string Id;
	std::string Description;
	int DelayWeeks = 4;                 // weeks after the decision
	double Probability = 0.5;           // before the difficulty multiplier
	std::vector<MetricEffect> Effects;  // if it occurs
	std::string FlagIfOccurs;           // set on the organisation when it occurs (opens follow-up missions)
	std::string MitigatingFlag;
	double MitigatedProbabilityFactor = 1.0;
	double MitigatedSeverityFactor = 1.0;
};

struct MissionOptionDef
{
	std::string Id;
	std::string Label;
	std::string Feedback;      // shown in the debrief only
	int Score = 0;             // Single: decision quality 0..100
	bool bCorrect = false;     // Multi: belongs to the right set
	double Cost = 0.0;         // spend the choice commits; above the player's authority it must be escalated
	bool bEscalates = false;   // routes the decision to someone with the authority
	std::vector<MetricEffect> Effects;
	std::vector<std::string> SetsFlags;
	std::vector<ConsequenceDef> Schedules;
};

struct MissionQuestionDef
{
	std::string Id;
	std::string Prompt;
	EQuestionKind Kind = EQuestionKind::Single;
	ECompetency Competency = ECompetency::RiskAssessment;
	int Weight = 0; // percent; a mission's weights sum to 100
	std::vector<MissionOptionDef> Options;
};

struct MissionEvidenceDef
{
	std::string Id;
	std::string Title;
	std::string Body;
	bool bRequired = true; // must be opened before submitting
};

// Briefing text that appears only when the organisation's state matches: this is how earlier
// decisions and incidents change later missions.
struct ContextNoteDef
{
	std::string Flag;            // condition on a flag (empty: use the metric condition)
	bool bWhenAbsent = false;    // true: shown when the flag is NOT set
	bool bUseMetric = false;
	EOrgMetric Metric = EOrgMetric::StakeholderTrust;
	bool bBelow = true;          // metric < Threshold (else >=)
	double Threshold = 0.0;
	std::string Text;
};

struct MissionDef
{
	std::string Id;
	std::string Title;
	EMissionKind Kind = EMissionKind::Mission;
	std::string Department;
	std::string Briefing;
	int MinLevel = 1;
	std::vector<std::string> RequiresPassed;  // mission ids (M001 counts)
	std::vector<std::string> RequiresFlags;
	std::vector<std::string> ExcludedByFlags;
	std::vector<ContextNoteDef> Context;
	std::vector<MissionEvidenceDef> Evidence;
	std::vector<MissionQuestionDef> Questions;
	int PassMark = 60;
	int Xp = 100;             // feedback only, never a promotion input
	int WeeksToComplete = 2;  // organisation time that passes when the mission is submitted
	std::string Lesson;       // added to the learning history
};

struct MissionCatalogue
{
	std::vector<MissionDef> Missions;
	RB_API const MissionDef* Find(const std::string& Id) const;
};

// Content validation: every problem as "<mission id>: <message>". Empty means valid.
RB_API std::vector<std::string> ValidateMission(const MissionDef& M);
// Also checks unique ids and that RequiresPassed refers to M001 or a catalogue mission.
RB_API std::vector<std::string> ValidateCatalogue(const MissionCatalogue& C);

// ---- Player state ----
struct MissionAnswer
{
	std::string QuestionId;
	std::vector<std::string> OptionIds;
};

struct MissionRecord
{
	std::string MissionId;
	int Week = 0;          // organisation week when submitted
	int Level = 0;         // career level when submitted (sets the authority used for scoring)
	int Score = 0;
	bool bPassed = false;
	std::vector<MissionAnswer> Answers;
};

struct CampaignState
{
	std::string ActiveMissionId;
	std::vector<std::string> ViewedEvidence; // for the active mission
	std::vector<MissionRecord> Records;      // every submitted attempt, oldest first
};

struct MissionAvailability
{
	bool bAvailable = false;
	bool bReview = false;             // a passed case reopened for practice (see IsCampaignStuck)
	std::vector<std::string> Reasons; // why not, human-readable
};

// With a catalogue, a passed mission at the player's current level is reopened as a review
// when the player is stuck: nothing new is open, they are not at the top, and the promotion
// review still fails (their evidence is below the bar). Reviews add competence evidence only.
RB_API MissionAvailability CheckAvailability(const GameState& State, const MissionDef& M, const MissionCatalogue* Catalogue = nullptr);
RB_API bool IsCampaignStuck(const GameState& State, const MissionCatalogue& C);
RB_API std::vector<const MissionDef*> AvailableMissions(const GameState& State, const MissionCatalogue& C);
// The order in which a question's options are shown: a seeded Fisher-Yates shuffle of 0..N-1 keyed
// by the game seed, mission id and question id. Content lists the best answer anywhere (usually
// first); players must never be able to win by position. Stable across save/load and retries.
RB_API std::vector<int> OptionDisplayOrder(uint64_t GameSeed, const std::string& MissionId, const std::string& QuestionId, int OptionCount);

// Briefing paragraphs that apply now (base briefing first, then matching context notes).
RB_API std::vector<std::string> BriefingFor(const GameState& State, const MissionDef& M);

enum class EMissionError
{
	None,
	Unavailable,
	AnotherMissionActive,
	NotActive,
	UnknownEvidence,
	RequiredEvidenceNotViewed,
	UnansweredQuestion,
	UnknownOption,
	SingleNeedsOneOption
};

RB_API const char* ToString(EMissionError E);

RB_API EMissionError StartMission(GameState& State, const MissionDef& M, const MissionCatalogue* Catalogue = nullptr);
RB_API EMissionError ViewMissionEvidence(GameState& State, const MissionDef& M, const std::string& EvidenceId);
RB_API EMissionError AbandonMission(GameState& State);
RB_API EMissionError ValidateAnswers(const GameState& State, const MissionDef& M, const std::vector<MissionAnswer>& Answers);

struct MissionCriterion
{
	std::string QuestionId;
	std::string Prompt;
	ECompetency Competency = ECompetency::RiskAssessment;
	int Weight = 0;
	int Score = 0;
	bool bAuthorityExceeded = false;
	std::vector<std::string> Feedback;
};

struct MissionDebrief
{
	std::string MissionId;
	int Score = 0;
	bool bPassed = false;
	std::vector<MissionCriterion> Criteria;
	std::vector<AssessmentResult> CompetencyEvidence; // one per competency the mission assessed
	int XpAwarded = 0;
	std::string Lesson;
	// Filled by SubmitMission (not by the pure scorer):
	std::vector<ResolvedConsequence> Resolved; // consequences that came due while the mission ran
	bool bPromoted = false;
	bool bReview = false;                      // practice attempt at an already-passed case
	int Level = 0;                             // career level after the promotion review
	std::vector<std::string> PromotionGaps;    // what still blocks the next promotion
};

// Choices that commit more than this authority without escalating are capped.
constexpr int AuthorityBreachScoreCap = 40;
// Multi questions: each wrong pick costs this much.
constexpr int MultiFalsePickPenalty = 25;

// Pure scoring (answers must be valid). Level sets the decision authority.
RB_API MissionDebrief ScoreMission(const MissionDef& M, const std::vector<MissionAnswer>& Answers, int Level);

// Scores, updates career evidence, records the organisational decision and its consequences,
// advances organisation time (resolving anything due, from any earlier mission), then runs the
// promotion review. Nothing changes if validation fails. Reviewing an already-passed mission
// updates competence evidence and the record only: no organisational effects, flags,
// consequences or time.
RB_API EMissionError SubmitMission(GameState& State, const MissionDef& M, const std::vector<MissionAnswer>& Answers, MissionDebrief* OutDebrief = nullptr);
} // namespace rb
