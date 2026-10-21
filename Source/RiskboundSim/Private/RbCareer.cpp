#include "RbCareer.h"

#include <algorithm>
#include <cmath>

namespace rb
{
const char* ToString(ECompetency C)
{
	switch (C)
	{
	case ECompetency::RiskIdentification: return "Risk identification";
	case ECompetency::RiskAssessment: return "Risk assessment";
	case ECompetency::ControlEvaluation: return "Control evaluation";
	case ECompetency::QuantitativeAnalysis: return "Quantitative analysis";
	case ECompetency::Communication: return "Communication";
	case ECompetency::StakeholderManagement: return "Stakeholder management";
	case ECompetency::Judgement: return "Judgement";
	case ECompetency::Leadership: return "Leadership";
	case ECompetency::StrategicThinking: return "Strategic thinking";
	case ECompetency::Governance: return "Governance";
	case ECompetency::Count: break;
	}
	return "?";
}

const char* ToString(ESpecialism S)
{
	switch (S)
	{
	case ESpecialism::None: return "None";
	case ESpecialism::Enterprise: return "Enterprise risk";
	case ESpecialism::Financial: return "Financial risk";
	case ESpecialism::Operational: return "Operational risk";
	case ESpecialism::Strategic: return "Strategic risk";
	case ESpecialism::Cyber: return "Cyber risk";
	case ESpecialism::Compliance: return "Compliance risk";
	case ESpecialism::BusinessContinuity: return "Business continuity";
	case ESpecialism::Project: return "Project risk";
	case ESpecialism::Environmental: return "Environmental risk";
	}
	return "?";
}

using C = ECompetency;

static std::vector<CareerLevelDef> BuildLadder()
{
	std::vector<CareerLevelDef> L;
	L.push_back({1, "Risk Management Intern / Graduate Trainee",
		"Maintain register entries, gather evidence, shadow analysts.",
		"Accurate, timely, asks good questions.",
		"Risk register (read/edit own entries)", 0.0,
		{{C::RiskIdentification, 40}, {C::Communication, 35}}, {"M000"}, 1});
	L.push_back({2, "Junior Risk Analyst",
		"Assess assigned operational risks, evaluate controls, draft recommendations.",
		"Evidence-based assessments, clear rationale.",
		"Risk matrix, control assessment, expected loss", 5000.0,
		{{C::RiskIdentification, 55}, {C::RiskAssessment, 55}, {C::ControlEvaluation, 50}, {C::Communication, 45}},
		{"M001", "M002", "M003"}, 2});
	L.push_back({3, "Risk Analyst",
		"Own a portfolio of risks, run KRIs, quantify losses.",
		"Independent analysis, sound quantification.",
		"KRI dashboard, cost-benefit analysis", 25000.0,
		{{C::RiskAssessment, 62}, {C::QuantitativeAnalysis, 55}, {C::ControlEvaluation, 60}, {C::Judgement, 50}},
		{"M004", "M005", "M006"}, 2});
	L.push_back({4, "Senior Risk Analyst",
		"Lead complex assessments, challenge first line, mentor juniors.",
		"Credible challenge, quality review of others' work.",
		"Monte Carlo, sensitivity analysis", 75000.0,
		{{C::QuantitativeAnalysis, 65}, {C::Judgement, 60}, {C::StakeholderManagement, 55}, {C::Communication, 60}},
		{"M007", "M008", "M009"}, 2});
	L.push_back({5, "Risk Supervisor / Team Lead",
		"Run the team's work plan, review assessments, escalate breaches.",
		"Reliable delivery, timely escalation.",
		"Corrective action tracker, team workload view", 150000.0,
		{{C::Leadership, 55}, {C::StakeholderManagement, 62}, {C::Judgement, 65}, {C::Governance, 50}},
		{"M010", "M011", "M012"}, 2});
	L.push_back({6, "Risk Manager",
		"Own the risk framework for a business unit; set appetite proposals.",
		"Business partnering, independent opinion.",
		"Risk appetite configuration, stress testing", 300000.0,
		{{C::Governance, 62}, {C::StrategicThinking, 55}, {C::Leadership, 62}, {C::QuantitativeAnalysis, 68}},
		{"M013", "M014", "M015"}, 2});
	L.push_back({7, "Senior Risk Manager",
		"Lead cross-functional risk programmes and crisis responses.",
		"Composure in crises, cross-department influence.",
		"Crisis room, scenario planner", 750000.0,
		{{C::StrategicThinking, 65}, {C::StakeholderManagement, 70}, {C::Judgement, 72}, {C::Leadership, 68}},
		{"M016", "M017", "M018", "C001"}, 2});
	L.push_back({8, "Head of Risk / Enterprise Risk Lead",
		"Own the enterprise risk profile and reporting to executives.",
		"Enterprise view, prioritisation, concise reporting.",
		"Enterprise risk profile, executive report builder", 1500000.0,
		{{C::StrategicThinking, 72}, {C::Governance, 72}, {C::Communication, 72}, {C::Leadership, 72}},
		{"M019", "M020", "M021", "M022", "C002"}, 3});
	L.push_back({9, "Director of Risk Management",
		"Set risk strategy, advise the executive, oversee all risk functions.",
		"Trusted adviser to executives, board-ready.",
		"Board pack, capital and liquidity stress tests", 5000000.0,
		{{C::StrategicThinking, 78}, {C::Governance, 78}, {C::StakeholderManagement, 78}, {C::Judgement, 78}},
		{"M023", "M024", "M025", "M026", "C003"}, 3});
	L.push_back({10, "Chief Risk Officer",
		"Accountable to the board for the enterprise risk framework and appetite.",
		"Independent, decisive, credible with board and regulators.",
		"Board appetite statement, enterprise crisis command", 25000000.0,
		{}, {}, 0});
	return L;
}

const std::vector<CareerLevelDef>& CareerLadder()
{
	static const std::vector<CareerLevelDef> Ladder = BuildLadder();
	return Ladder;
}

const CareerLevelDef& LevelDef(int Level)
{
	const auto& L = CareerLadder();
	const int Idx = std::max(1, std::min(static_cast<int>(L.size()), Level)) - 1;
	return L[static_cast<size_t>(Idx)];
}

const std::vector<std::string>& AdvancedPathways()
{
	static const std::vector<std::string> P = {
		"Group CRO", "Chief Executive Officer", "Chief Operating Officer", "Specialist Executive", "Board Risk Adviser"};
	return P;
}

void CareerState::ApplyAssessment(const AssessmentResult& R)
{
	CompetencyRecord& Rec = Competencies[static_cast<size_t>(R.Competency)];
	const int Clamped = std::max(0, std::min(100, R.Score));
	if (Rec.Assessments == 0)
	{
		Rec.Score = Clamped;
	}
	else
	{
		Rec.Score = static_cast<int>(std::lround(Rec.Score * (1.0 - BlendWeight) + Clamped * BlendWeight));
	}
	++Rec.Assessments;
}

void CareerState::RecordMission(const std::string& MissionId, bool bPassed, int XpAward)
{
	if (std::find(AttemptedMissions.begin(), AttemptedMissions.end(), MissionId) == AttemptedMissions.end())
	{
		AttemptedMissions.push_back(MissionId);
	}
	if (bPassed && !HasPassed(MissionId))
	{
		PassedMissions.push_back(MissionId);
	}
	Xp += std::max(0, XpAward);
}

bool CareerState::HasPassed(const std::string& MissionId) const
{
	return std::find(PassedMissions.begin(), PassedMissions.end(), MissionId) != PassedMissions.end();
}

PromotionCheck CareerState::CheckPromotion() const
{
	PromotionCheck Out;
	const CareerLevelDef& Def = LevelDef(Level);
	if (Level >= static_cast<int>(CareerLadder().size()))
	{
		Out.UnmetGates.push_back("Already at the top of the ladder");
		return Out;
	}
	for (const CompetencyRequirement& Req : Def.PromotionRequirements)
	{
		const CompetencyRecord& Rec = Get(Req.Competency);
		if (Rec.Score < Req.MinScore)
		{
			Out.UnmetGates.push_back(std::string(ToString(Req.Competency)) + " below " + std::to_string(Req.MinScore));
		}
		if (Rec.Assessments < Def.MinAssessmentsPerRequirement)
		{
			Out.UnmetGates.push_back(std::string(ToString(Req.Competency)) + " needs more assessed evidence");
		}
	}
	for (const std::string& M : Def.RequiredMissions)
	{
		if (!HasPassed(M))
		{
			Out.UnmetGates.push_back("Mission " + M + " not passed");
		}
	}
	Out.bEligible = Out.UnmetGates.empty();
	return Out;
}

bool CareerState::TryPromote()
{
	if (!CheckPromotion().bEligible)
	{
		return false;
	}
	++Level;
	return true;
}
} // namespace rb
