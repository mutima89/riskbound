#include "RbOrganization.h"

#include "RbRng.h"

#include <algorithm>

namespace rb
{
const char* ToString(EOrgMetric M)
{
	switch (M)
	{
	case EOrgMetric::Revenue: return "Revenue";
	case EOrgMetric::Costs: return "Costs";
	case EOrgMetric::Cash: return "Cash";
	case EOrgMetric::RiskExposure: return "Risk exposure";
	case EOrgMetric::ControlEffectiveness: return "Control effectiveness";
	case EOrgMetric::Reputation: return "Reputation";
	case EOrgMetric::StakeholderTrust: return "Stakeholder trust";
	case EOrgMetric::EmployeeMorale: return "Employee morale";
	case EOrgMetric::Resilience: return "Resilience";
	case EOrgMetric::StrategicProgress: return "Strategic progress";
	case EOrgMetric::Count: break;
	}
	return "?";
}

bool IsIndexMetric(EOrgMetric M)
{
	return M != EOrgMetric::Revenue && M != EOrgMetric::Costs && M != EOrgMetric::Cash;
}

const char* ToString(EDepartment D)
{
	switch (D)
	{
	case EDepartment::Finance: return "Finance";
	case EDepartment::Operations: return "Operations";
	case EDepartment::IT: return "IT";
	case EDepartment::HR: return "HR";
	case EDepartment::LegalCompliance: return "Legal and Compliance";
	case EDepartment::Procurement: return "Procurement";
	case EDepartment::Commercial: return "Commercial";
	case EDepartment::SecuritySafety: return "Security and Safety";
	case EDepartment::Executive: return "Executive Management";
	case EDepartment::Count: break;
	}
	return "?";
}

const std::vector<OrganizationProfile>& OrganizationProfiles()
{
	// Revenue, Costs, Cash, Exposure, ControlEff, Reputation, Trust, Morale, Resilience, Strategy
	static const std::vector<OrganizationProfile> P = {
		{"valtren", "Valtren Coldline Logistics", "Temperature-controlled logistics",
			{48000000, 44500000, 3200000, 55, 50, 62, 58, 60, 45, 30}},
		{"orrinbrook", "Orrinbrook Mutual Assurance", "Financial services",
			{120000000, 104000000, 18000000, 45, 62, 70, 66, 55, 60, 40}},
		{"quellard", "Quellard Grid Projects", "Infrastructure construction",
			{260000000, 248000000, 9000000, 65, 45, 55, 50, 52, 40, 35}},
	};
	return P;
}

OrganizationState::OrganizationState()
	: OrganizationState(OrganizationProfiles().front())
{
}

OrganizationState::OrganizationState(const OrganizationProfile& Profile)
	: ProfileId(Profile.Id)
	, Name(Profile.Name)
{
	std::copy(Profile.Metrics, Profile.Metrics + OrgMetricCount, Metrics);
}

void OrganizationState::Apply(const MetricEffect& E, double Scale)
{
	double& V = Metrics[static_cast<int>(E.Metric)];
	V += E.Delta * Scale;
	if (IsIndexMetric(E.Metric))
	{
		V = std::max(0.0, std::min(100.0, V));
	}
}

void OrganizationState::Apply(const std::vector<MetricEffect>& Effects, double Scale)
{
	for (const MetricEffect& E : Effects)
	{
		Apply(E, Scale);
	}
}

void OrganizationState::SetFlag(const std::string& Flag)
{
	if (!HasFlag(Flag))
	{
		Flags.push_back(Flag);
	}
}

bool OrganizationState::HasFlag(const std::string& Flag) const
{
	return std::find(Flags.begin(), Flags.end(), Flag) != Flags.end();
}

void OrganizationState::RecordDecision(DecisionRecord D)
{
	D.Week = Week;
	Apply(D.Effects);
	Decisions.push_back(std::move(D));
}

void OrganizationState::Schedule(PendingConsequence C)
{
	Pending.push_back(std::move(C));
}

double OrganizationState::NextUniform()
{
	// Re-derive the stream from seed + draw count so the sequence survives save/load
	// without serialising generator internals.
	Rng R(EventSeed ^ (0x9E3779B97F4A7C15ULL * (EventDraws + 1)));
	++EventDraws;
	return R.Uniform();
}

std::vector<ResolvedConsequence> OrganizationState::AdvanceWeeks(int Weeks)
{
	std::vector<ResolvedConsequence> Resolved;
	Week += std::max(0, Weeks);
	std::stable_sort(Pending.begin(), Pending.end(),
		[](const PendingConsequence& A, const PendingConsequence& B) { return A.TriggerWeek < B.TriggerWeek; });
	std::vector<PendingConsequence> Remaining;
	for (PendingConsequence& C : Pending)
	{
		if (C.TriggerWeek > Week)
		{
			Remaining.push_back(std::move(C));
			continue;
		}
		ResolvedConsequence R;
		R.Id = C.Id;
		R.SourceDecisionId = C.SourceDecisionId;
		R.Description = C.Description;
		R.Week = C.TriggerWeek;
		R.FlagIfOccurs = C.FlagIfOccurs;
		R.bMitigated = !C.MitigatingFlag.empty() && HasFlag(C.MitigatingFlag);
		R.EffectiveProbability = std::min(1.0, std::max(0.0, C.Probability * (R.bMitigated ? C.MitigatedProbabilityFactor : 1.0)));
		R.bOccurred = NextUniform() < R.EffectiveProbability;
		if (R.bOccurred)
		{
			Apply(C.EffectsIfOccurs, R.bMitigated ? C.MitigatedSeverityFactor : 1.0);
			if (!C.FlagIfOccurs.empty()) SetFlag(C.FlagIfOccurs);
		}
		for (DecisionRecord& D : Decisions)
		{
			if (D.Id == C.SourceDecisionId)
			{
				D.Outcome = R.bOccurred ? "Incident occurred: " + C.Description : "Incident avoided: " + C.Description;
			}
		}
		History.push_back(R);
		Resolved.push_back(R);
	}
	Pending = std::move(Remaining);
	return Resolved;
}
} // namespace rb
