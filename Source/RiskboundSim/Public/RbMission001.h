// Mission 001 "The Unresolved Warning": simulation side of the vertical slice.
// Presentation (level, NPCs, UMG) drives this through the public API only.
// The rubric lives in the .cpp and is never exposed before Submit().
#pragma once

#include "RbApi.h"

#include "RbActions.h"
#include "RbCareer.h"
#include "RbOrganization.h"
#include "RbQuant.h"
#include "RbRiskModel.h"

#include <string>
#include <vector>

namespace rb
{
enum class EDifficulty
{
	Analyst,      // hints available, incidents less likely
	Professional, // baseline
	Executive     // no hints, incidents more likely
};

RB_API const char* ToString(EDifficulty D);
RB_API double IncidentProbabilityMultiplier(EDifficulty D);

namespace m001
{
extern RB_API const char* const MissionId; // "M001"

enum class EObjective
{
	MeetManager,
	ReviewIncidentReport,
	InspectRiskRegister,
	ReviewControlEvidence,
	IdentifyMissingInformation,
	AssessLikelihoodImpact,
	DetermineResidualRisk,
	RecommendTreatment,
	SelectActionOwner,
	DocumentRationale,
	ReceiveDebrief,
	Count
};

constexpr int ObjectiveCount = static_cast<int>(EObjective::Count);
RB_API const char* ObjectiveText(EObjective O);

struct EvidenceItem
{
	std::string Id;
	std::string Title;
	std::string Location; // where it is found in the level
	std::string Body;
	bool bControlEvidence = false;
};

struct Option
{
	std::string Id;
	std::string Label;
	std::string Description;
};

// Static mission content.
RB_API const std::vector<EvidenceItem>& Evidence();
RB_API const std::vector<Option>& MissingInfoCandidates();
RB_API const std::vector<Option>& TreatmentOptions();
RB_API const std::vector<Option>& ActionOwnerOptions();
RB_API const std::vector<Option>& RationaleTags();
RB_API const std::vector<Control>& ExistingControls(); // the controls under review (names only matter to the player)
RB_API const std::vector<LossScenario>& BaselineScenarios();
RB_API TreatmentAppraisalInput AppraisalInputFor(const std::string& TreatmentId);
// The pre-existing, out-of-date register entry the player inspects.
RB_API RiskEntry LegacyRegisterEntry();

struct Submission
{
	std::vector<std::string> MissingInfo;
	int InherentLikelihood = 0;
	int InherentImpact = 0;
	std::vector<EControlEffectiveness> ControlRatings; // one per ExistingControls()
	int ResidualLikelihood = 0;
	int ResidualImpact = 0;
	std::string TreatmentId;
	std::string ActionOwnerId;
	std::vector<std::string> RationaleTagIds;
	std::string RationaleText;
};

struct CriterionResult
{
	std::string Name;
	int Score = 0;   // 0..100
	int Weight = 0;  // percent
	std::string Feedback;
};

struct Debrief
{
	bool bPassed = false;
	int OverallScore = 0; // decision quality, 0..100
	std::vector<CriterionResult> Criteria;
	std::vector<std::string> Assumptions;
	std::vector<std::string> TradeOffs;
	std::vector<std::string> Improvements;
	TreatmentAppraisal ChosenAppraisal;
	TreatmentAppraisal RecommendedAppraisal;
	double BaselineExpectedLoss = 0.0;
	double BaselineWorstCase = 0.0;
	std::vector<AssessmentResult> CompetencyEvidence;
	int XpAwarded = 0;
};

enum class ESubmitError
{
	None,
	AlreadySubmitted,
	ManagerNotMet,
	IncidentReportNotReviewed,
	RegisterNotInspected,
	IncompleteSubmission
};

class RB_API MissionState
{
public:
	bool bObjectives[ObjectiveCount] = {};
	std::vector<std::string> ViewedEvidence;
	bool bSubmitted = false;
	Submission Submitted;
	Debrief Result;

	void MeetManager();
	bool ViewEvidence(const std::string& EvidenceId); // false for unknown id
	bool IsComplete(EObjective O) const { return bObjectives[static_cast<int>(O)]; }
	// Index of the first incomplete objective, or ObjectiveCount when done.
	int CurrentObjective() const;

	ESubmitError Validate(const Submission& S) const;
	// Scores the submission, writes the debrief, updates career + organisation + register.
	ESubmitError Submit(const Submission& S, EDifficulty Difficulty, CareerState& Career, OrganizationState& Org, RiskRegister& Register);
};

// Corrective actions implied by a submitted treatment (owner = the chosen action owner's name),
// due relative to Week. Empty for an unknown treatment id.
RB_API std::vector<CorrectiveAction> TreatmentActions(const Submission& S, int Week);

// Pure scoring, exposed for tests. Does not mutate anything.
RB_API Debrief Score(const Submission& S, const std::vector<std::string>& ViewedEvidence);
} // namespace m001
} // namespace rb
