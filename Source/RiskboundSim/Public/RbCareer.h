// Career levels, competencies and promotion gates. XP is tracked for feedback only and
// is never an input to promotion. Canonical rules: Docs/CareerModel.md.
#pragma once

#include <array>
#include <string>
#include <vector>

namespace rb
{
enum class ECompetency
{
	RiskIdentification,
	RiskAssessment,
	ControlEvaluation,
	QuantitativeAnalysis,
	Communication,
	StakeholderManagement,
	Judgement,
	Leadership,
	StrategicThinking,
	Governance,
	Count
};

constexpr int CompetencyCount = static_cast<int>(ECompetency::Count);
const char* ToString(ECompetency C);

enum class ESpecialism
{
	None,
	Enterprise,
	Financial,
	Operational,
	Strategic,
	Cyber,
	Compliance,
	BusinessContinuity,
	Project,
	Environmental
};

const char* ToString(ESpecialism S);

struct CompetencyRequirement
{
	ECompetency Competency;
	int MinScore; // 0..100
};

struct CareerLevelDef
{
	int Level = 1; // 1..10
	std::string Title;
	std::string Responsibilities;
	std::string StakeholderExpectations;
	std::string NewTools;
	double DecisionAuthority = 0.0; // max spend the player can approve without escalation
	std::vector<CompetencyRequirement> PromotionRequirements; // to move to Level+1
	std::vector<std::string> RequiredMissions;                // passed missions needed for promotion
	int MinAssessmentsPerRequirement = 1;                    // evidence count per required competency
};

// The ten-level ladder. Index 0 = level 1.
const std::vector<CareerLevelDef>& CareerLadder();
const CareerLevelDef& LevelDef(int Level);

// Post-CRO optional roles.
const std::vector<std::string>& AdvancedPathways();

struct CompetencyRecord
{
	int Score = 0;          // 0..100, blended from assessments
	int Assessments = 0;    // number of assessed pieces of evidence
};

struct AssessmentResult
{
	ECompetency Competency;
	int Score; // 0..100 for this piece of work
};

struct PromotionCheck
{
	bool bEligible = false;
	std::vector<std::string> UnmetGates; // human-readable reasons
};

class CareerState
{
public:
	static constexpr double BlendWeight = 0.4; // weight of newest assessment

	int Level = 2; // Mission 001 starts at Junior Risk Analyst
	int Xp = 0;
	ESpecialism Specialism = ESpecialism::None;
	std::array<CompetencyRecord, CompetencyCount> Competencies{};
	std::vector<std::string> PassedMissions;
	std::vector<std::string> AttemptedMissions;

	void ApplyAssessment(const AssessmentResult& R);
	void RecordMission(const std::string& MissionId, bool bPassed, int XpAward);
	bool HasPassed(const std::string& MissionId) const;

	PromotionCheck CheckPromotion() const;
	// Promotes only when CheckPromotion() is eligible. Returns true on promotion.
	bool TryPromote();

	const CompetencyRecord& Get(ECompetency C) const { return Competencies[static_cast<size_t>(C)]; }
};
} // namespace rb
