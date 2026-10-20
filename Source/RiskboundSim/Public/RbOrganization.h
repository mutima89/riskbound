// Organisational state, decision records and deferred consequences.
// Decision QUALITY (how good the reasoning was) is recorded separately from the
// OUTCOME (what chance produced). See Docs/GameDesign.md "Consequences".
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rb
{
enum class EOrgMetric
{
	Revenue,            // currency / year
	Costs,              // currency / year
	Cash,               // currency
	RiskExposure,       // index 0..100 (higher = worse)
	ControlEffectiveness, // index 0..100
	Reputation,         // index 0..100
	StakeholderTrust,   // index 0..100
	EmployeeMorale,     // index 0..100
	Resilience,         // index 0..100
	StrategicProgress,  // index 0..100
	Count
};

constexpr int OrgMetricCount = static_cast<int>(EOrgMetric::Count);
const char* ToString(EOrgMetric M);
bool IsIndexMetric(EOrgMetric M); // clamped to 0..100

struct MetricEffect
{
	EOrgMetric Metric;
	double Delta;
};

enum class EDepartment
{
	Finance,
	Operations,
	IT,
	HR,
	LegalCompliance,
	Procurement,
	Commercial,
	SecuritySafety,
	Executive,
	Count
};

const char* ToString(EDepartment D);

struct DecisionRecord
{
	std::string Id;
	std::string MissionId;
	std::string Summary;
	int Week = 0;
	int QualityScore = 0; // 0..100 from the rubric, independent of the outcome
	std::string Outcome;  // filled when consequences resolve
	std::vector<MetricEffect> Effects;
};

// A consequence scheduled for later. When it fires, a Bernoulli draw with
// Probability decides whether the incident happens. Mitigating flags (e.g. a control
// the player implemented) scale both probability and severity.
struct PendingConsequence
{
	std::string Id;
	std::string SourceDecisionId;
	std::string Description;
	int TriggerWeek = 0;
	double Probability = 1.0;
	std::vector<MetricEffect> EffectsIfOccurs;
	std::string MitigatingFlag;    // if set in Flags at trigger time...
	double MitigatedProbabilityFactor = 1.0; // ...probability multiplied by this
	double MitigatedSeverityFactor = 1.0;    // ...and effects multiplied by this
};

struct ResolvedConsequence
{
	std::string Id;
	std::string SourceDecisionId;
	std::string Description;
	int Week = 0;
	bool bOccurred = false;
	bool bMitigated = false;
	double EffectiveProbability = 0.0;
};

struct OrganizationProfile
{
	std::string Id;
	std::string Name;
	std::string Sector;
	double Metrics[OrgMetricCount];
};

const std::vector<OrganizationProfile>& OrganizationProfiles();

class OrganizationState
{
public:
	OrganizationState();
	explicit OrganizationState(const OrganizationProfile& Profile);

	std::string ProfileId;
	std::string Name;
	int Week = 0;
	uint64_t EventSeed = 1;   // reproducible random events
	uint64_t EventDraws = 0;  // draws consumed, so a reload continues the same sequence
	double Metrics[OrgMetricCount] = {};
	std::vector<std::string> Flags;
	std::vector<DecisionRecord> Decisions;
	std::vector<PendingConsequence> Pending;
	std::vector<ResolvedConsequence> History;

	double Get(EOrgMetric M) const { return Metrics[static_cast<int>(M)]; }
	void Apply(const MetricEffect& E, double Scale = 1.0);
	void Apply(const std::vector<MetricEffect>& Effects, double Scale = 1.0);

	void SetFlag(const std::string& Flag);
	bool HasFlag(const std::string& Flag) const;

	void RecordDecision(DecisionRecord D);
	void Schedule(PendingConsequence C);

	// Advances time and resolves every consequence whose trigger week has arrived.
	// Returns the consequences resolved during this call, in trigger order.
	std::vector<ResolvedConsequence> AdvanceWeeks(int Weeks);

private:
	double NextUniform();
};
} // namespace rb
