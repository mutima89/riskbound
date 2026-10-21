// Campaign: generic decision missions after Mission 001, with career progression and
// organisational consequences that carry from one mission to the next.
// Mission content is data (MissionDef); rules live here. Canonical rules: Docs/CareerModel.md
// ("Campaign progression"). Rubric scores stay hidden until SubmitMission().
#pragma once

#include "RbApi.h"

#include "RbCareer.h"
#include "RbOrganization.h"
#include "RbPeople.h"

#include <cstdint>
#include <string>
#include <utility>
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
	Single,  // pick one option; the option's Score is the result
	Multi,   // pick all that apply; scored against the options marked bCorrect
	Numeric, // type a figure (usually worked out in a workstation tool); scored by how close it is
	Rating   // place the risk on the 5x5 matrix (likelihood, impact); scored by distance from the target
};

RB_API const char* ToString(EQuestionKind K); // "single", "multi", "numeric", "rating"

// A wrong figure players commonly reach, with feedback that explains it (e.g. the worst case
// offered as expected loss).
struct NumericMistake
{
	double Value = 0.0;
	std::string Feedback;
};

// Numeric scoring bands, by relative error |value - answer| / max(|answer|, 1).
constexpr int NumericScoreExact = 100; // within Tolerance
constexpr int NumericScoreClose = 70;  // within 3 x Tolerance
constexpr int NumericScoreNear = 35;   // within 10 x Tolerance
constexpr int NumericScoreMistake = 10;
// Rating: each step of distance in likelihood or impact costs this much.
constexpr int RatingStepPenalty = 30;

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
	std::vector<RelationEffect> Relations; // how the people involved feel about this choice
	int Stress = 0;                        // change to the player's stress (overtime, conflict, relief)
};

struct MissionQuestionDef
{
	std::string Id;
	std::string Prompt;
	EQuestionKind Kind = EQuestionKind::Single;
	ECompetency Competency = ECompetency::RiskAssessment;
	int Weight = 0; // percent; a mission's weights sum to 100
	std::vector<MissionOptionDef> Options;   // Single and Multi only
	// Numeric
	double Answer = 0.0;
	double Tolerance = 0.02;                 // relative
	std::string Unit;                        // shown next to the input, e.g. "a year"
	std::vector<NumericMistake> Mistakes;
	// Rating
	int TargetLikelihood = 0;
	int TargetImpact = 0;
	// Numeric and Rating: the worked answer, shown in the debrief only.
	std::string Explanation;
};

// The answer a perfect player gives (best option, every correct option, the exact figure or the
// target rating). For tests, the self-test and the scripted walkthrough.
RB_API struct MissionAnswer BestAnswer(const MissionQuestionDef& Q);
// Numeric answers are carried as one option string ("20800"); parsing is strict (finite, no junk).
RB_API bool ParseNumericAnswer(const std::string& Text, double& Out);

struct MissionEvidenceDef
{
	std::string Id;
	std::string Title;
	std::string Body;
	bool bRequired = true; // must be opened before submitting
	std::string Clue;      // story clue gained when opened (Data/Story.json), may be empty
	std::string At;        // HQ location where it must be examined (empty: the workstation), see KnownLocations()
	std::string Person;    // interview: the character met there (Data/People.json), may be empty
};

// Places in the Valtren HQ where casework happens in person: ids and display names.
RB_API const std::vector<std::pair<std::string, std::string>>& KnownLocations();
RB_API std::string LocationName(const std::string& Id); // "" if unknown

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
	std::string RelationId;       // condition on a relationship (empty: none)
	bool bRelationBelow = false;  // relation < RelationThreshold (else >=)
	int RelationThreshold = 0;
	int MinClues = 0;             // story clues collected (0: no condition)
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
	int RequiresClues = 0;    // story missions: clues collected before it opens
	// Crises: weeks the case can wait once it opens. Past that, it escalates without you:
	// EscalationEffects apply, flag "<id>_escalated" is set and the case closes (UpdateWorld).
	int DeadlineWeeks = 0;
	std::vector<MetricEffect> EscalationEffects;
	std::string EscalationText;
	std::string PresentAt;    // submitted in person at this HQ location (a board presentation), empty: anywhere
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
// For a crisis opened by an incident: which earlier decision led here ("" if none).
RB_API std::string CrisisCause(const GameState& State, const MissionDef& M);
// Weeks left before an open crisis escalates (-1: no deadline or not open yet).
RB_API int WeeksUntilEscalation(const GameState& State, const MissionDef& M);

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
	SingleNeedsOneOption,
	EvidenceElsewhere, // located evidence must be examined where it is
	PresentInPerson    // this mission is presented at its PresentAt location
};

RB_API const char* ToString(EMissionError E);

RB_API EMissionError StartMission(GameState& State, const MissionDef& M, const MissionCatalogue* Catalogue = nullptr);
// Where: the HQ location the player is examining it in ("" at the workstation).
RB_API EMissionError ViewMissionEvidence(GameState& State, const MissionDef& M, const std::string& EvidenceId, const std::string& Where = std::string());
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
	std::vector<std::string> Fallout;          // weak answers that may now cause an incident (descriptions)
	std::vector<std::string> NewMessages;      // inbox message ids delivered after the mission (UpdateWorld)
	std::vector<std::string> Escalated;        // crises that escalated while time passed
	bool bBurnout = false;                     // stress forced the player to take leave
};

struct WorldContent;

// Weak decisions leave latent risk behind: a single-choice answer scoring below this (with no
// consequence of its own) or a wrong pick in a multi-select may cause a later incident.
constexpr int FalloutScoreThreshold = 50;
constexpr double FalloutMaxProbability = 0.6;   // at score 0; scales linearly to 0 at the threshold
constexpr double FalloutMultiProbability = 0.2; // per question with wrong picks

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
// With World content: also schedules fallout from weak answers, applies relationship and stress
// effects and runs UpdateWorld after time passes (inbox events, escalations, burnout, news).
RB_API EMissionError SubmitMission(GameState& State, const MissionDef& M, const std::vector<MissionAnswer>& Answers, MissionDebrief* OutDebrief = nullptr,
	const MissionCatalogue* Catalogue = nullptr, const WorldContent* World = nullptr, const std::string& Where = std::string());
} // namespace rb
