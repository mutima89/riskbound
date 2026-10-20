#include "RbKri.h"

#include "RbOrganization.h"
#include "RbRng.h"

#include <algorithm>
#include <cmath>

namespace rb
{
const std::vector<KriDefinition>& MeridianKris()
{
	static const std::vector<KriDefinition> K = {
		{"alarm_ack_no_action", "Alarm acknowledgements without follow-up", "%", 20.0, 40.0, true, 68.0, 8.0, "northgate_controls_fixed", 0.10, true},
		{"maintenance_overdue", "Planned maintenance overdue", "%", 10.0, 25.0, true, 35.0, 4.0, "northgate_controls_fixed", 0.15, true},
		{"excursions_per_month", "Temperature excursions", "per month", 0.4, 1.0, true, 0.5, 0.1, "northgate_controls_fixed", 0.40, false},
		{"manual_check_completion", "Manual temperature checks completed", "%", 90.0, 75.0, false, 77.0, 96.0, "northgate_controls_fixed", 0.05, true},
		{"client_complaints", "Client complaints", "per month", 2.0, 4.0, true, 3.0, 1.0, "northgate_controls_fixed", 0.30, false},
	};
	return K;
}

const KriDefinition* FindKri(const std::string& Id)
{
	for (const KriDefinition& D : MeridianKris())
	{
		if (D.Id == Id) return &D;
	}
	return nullptr;
}

double KriValue(const KriDefinition& D, int Index, bool bImproved, uint64_t Seed, int Week)
{
	const uint64_t Mixed = Seed ^ (0x9E3779B97F4A7C15ULL * static_cast<uint64_t>(static_cast<int64_t>(Week) + 4096)) ^
		(static_cast<uint64_t>(Index + 1) << 40);
	Rng Random(Mixed);
	const double U = Random.Uniform();
	const double Target = bImproved ? D.Improved : D.Baseline;
	double V = Target * (1.0 + D.Noise * (2.0 * U - 1.0));
	V = std::max(0.0, V);
	if (D.bPercent) V = std::min(100.0, V);
	return std::round(V * 10.0) / 10.0;
}

KriReading MakeReading(const KriDefinition& D, double Value, int Week, const KriReading* Previous)
{
	Kri K;
	K.Name = D.Name;
	K.Value = Value;
	K.AmberThreshold = D.Amber;
	K.RedThreshold = D.Red;
	K.bHigherIsWorse = D.bHigherIsWorse;

	KriReading R;
	R.Week = Week;
	R.Value = Value;
	R.Status = EvaluateKri(K);
	const bool bContinues = Previous && Previous->Week == Week - 1;
	R.AmberStreak = R.Status == EKriStatus::Amber ? (bContinues ? Previous->AmberStreak : 0) + 1 : 0;
	R.bEscalate = KriRequiresEscalation(R.Status, R.AmberStreak, KriAmberWeeksBeforeEscalation);
	return R;
}

static void Append(KriSeries& S, const KriDefinition& D, int Index, const OrganizationState& Org, uint64_t Seed, int Week)
{
	const bool bImproved = !D.ImprovingFlag.empty() && Org.HasFlag(D.ImprovingFlag);
	const double Value = KriValue(D, Index, bImproved, Seed, Week);
	S.History.push_back(MakeReading(D, Value, Week, S.Latest()));
	if (S.History.size() > static_cast<size_t>(KriHistoryLimit))
	{
		S.History.erase(S.History.begin());
	}
}

KriMonitor MakeKriMonitor(const OrganizationState& Org, uint64_t Seed, int WeeksOfHistory)
{
	KriMonitor M;
	M.Seed = Seed;
	const std::vector<KriDefinition>& Defs = MeridianKris();
	const int Weeks = std::max(1, std::min(WeeksOfHistory, KriHistoryLimit));
	for (size_t i = 0; i < Defs.size(); ++i)
	{
		KriSeries S;
		S.Id = Defs[i].Id;
		for (int W = Org.Week - Weeks + 1; W <= Org.Week; ++W)
		{
			Append(S, Defs[i], static_cast<int>(i), Org, Seed, W);
		}
		M.Series.push_back(std::move(S));
	}
	return M;
}

int RecordKriWeeks(KriMonitor& M, const OrganizationState& Org)
{
	const std::vector<KriDefinition>& Defs = MeridianKris();
	int Added = 0;
	for (size_t i = 0; i < M.Series.size() && i < Defs.size(); ++i)
	{
		KriSeries& S = M.Series[i];
		int AddedHere = 0;
		for (int W = S.Latest() ? S.Latest()->Week + 1 : Org.Week; W <= Org.Week; ++W)
		{
			Append(S, Defs[i], static_cast<int>(i), Org, M.Seed, W);
			++AddedHere;
		}
		Added = std::max(Added, AddedHere);
	}
	return Added;
}
} // namespace rb
