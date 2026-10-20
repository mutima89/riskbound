// Key risk indicator set for an organisation, with a deterministic weekly history.
// Each weekly value depends only on (definition, org flags, seed, week), so the same game
// always shows the same history, before and after a reload. Rules: Docs/RiskMethodology.md section 13.
#pragma once

#include "RbApi.h"
#include "RbQuant.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rb
{
class OrganizationState;

struct KriDefinition
{
	std::string Id;
	std::string Name;
	std::string Unit;          // "%" or "per month"
	double Amber = 0.0;        // thresholds are inclusive (value at threshold takes the worse status)
	double Red = 0.0;
	bool bHigherIsWorse = true;
	double Baseline = 0.0;     // typical value while the underlying problem is untreated
	double Improved = 0.0;     // typical value once ImprovingFlag is set on the organisation
	std::string ImprovingFlag; // org flag that moves the target from Baseline to Improved
	double Noise = 0.0;        // relative weekly variation, 0..1
	bool bPercent = false;     // clamp to 0..100
};

struct KriReading
{
	int Week = 0;
	double Value = 0.0;
	EKriStatus Status = EKriStatus::Green;
	int AmberStreak = 0;       // consecutive amber readings up to and including this one
	bool bEscalate = false;    // KriRequiresEscalation(Status, AmberStreak)
};

struct KriSeries
{
	std::string Id;
	std::vector<KriReading> History; // oldest first, at most KriHistoryLimit readings
	const KriReading* Latest() const { return History.empty() ? nullptr : &History.back(); }
};

struct KriMonitor
{
	uint64_t Seed = 1;
	std::vector<KriSeries> Series; // one per definition, same order as the definitions
};

constexpr int KriHistoryLimit = 52;
constexpr int KriAmberWeeksBeforeEscalation = 3;

// Valtren Coldline Logistics cold-chain KRIs (the set Mission 001 is about).
RB_API const std::vector<KriDefinition>& ValtrenKris();
RB_API const KriDefinition* FindKri(const std::string& Id);

// Value = round1( clamp( Target x (1 + Noise x (2u - 1)) ) ), Target = Improved if bImproved else Baseline,
// u in [0,1) drawn from an Rng seeded by (Seed, Week, Index).
RB_API double KriValue(const KriDefinition& D, int Index, bool bImproved, uint64_t Seed, int Week);
// Status from thresholds; amber streak continues from Previous (if it is the prior reading).
RB_API KriReading MakeReading(const KriDefinition& D, double Value, int Week, const KriReading* Previous);

// A monitor with WeeksOfHistory readings ending at Org.Week, using the organisation's current flags.
RB_API KriMonitor MakeKriMonitor(const OrganizationState& Org, uint64_t Seed, int WeeksOfHistory);
// Appends readings for every week after the last recorded one up to Org.Week. Returns readings added per series.
RB_API int RecordKriWeeks(KriMonitor& M, const OrganizationState& Org);
} // namespace rb
