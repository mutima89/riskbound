#include "RbQuant.h"

#include "RbRng.h"

#include <algorithm>
#include <cmath>

namespace rb
{
double ExpectedLoss(const std::vector<LossScenario>& Scenarios)
{
	double Sum = 0.0;
	for (const LossScenario& S : Scenarios)
	{
		Sum += S.AnnualProbability * S.Loss;
	}
	return Sum;
}

double WorstCaseLoss(const std::vector<LossScenario>& Scenarios)
{
	double Worst = 0.0;
	for (const LossScenario& S : Scenarios)
	{
		if (S.AnnualProbability > 0.0)
		{
			Worst = std::max(Worst, S.Loss);
		}
	}
	return Worst;
}

bool ValidateScenarios(const std::vector<LossScenario>& Scenarios, bool bMutuallyExclusive)
{
	double Total = 0.0;
	for (const LossScenario& S : Scenarios)
	{
		if (!(S.AnnualProbability >= 0.0 && S.AnnualProbability <= 1.0) || !(S.Loss >= 0.0))
		{
			return false;
		}
		Total += S.AnnualProbability;
	}
	return !bMutuallyExclusive || Total <= 1.0 + 1e-9;
}

double AnnualisedLossExpectancy(double AnnualRateOfOccurrence, double SingleLossExpectancy)
{
	return std::max(0.0, AnnualRateOfOccurrence) * std::max(0.0, SingleLossExpectancy);
}

TreatmentAppraisal AppraiseTreatment(const TreatmentAppraisalInput& In)
{
	TreatmentAppraisal Out;
	Out.AnnualRiskReduction = In.ExpectedLossBefore - In.ExpectedLossAfter;
	Out.PvCost = In.OneOffCost;
	for (int Year = 1; Year <= In.Years; ++Year)
	{
		const double Df = 1.0 / std::pow(1.0 + In.DiscountRate, Year);
		Out.PvBenefit += Out.AnnualRiskReduction * Df;
		Out.PvCost += In.AnnualCost * Df;
	}
	Out.Npv = Out.PvBenefit - Out.PvCost;
	if (Out.PvCost > 0.0)
	{
		Out.Roi = Out.Npv / Out.PvCost;
		Out.BenefitCostRatio = Out.PvBenefit / Out.PvCost;
	}
	Out.bCostEffective = Out.Npv > 0.0;
	return Out;
}

const char* ToString(EKriStatus S)
{
	switch (S)
	{
	case EKriStatus::Green: return "Green";
	case EKriStatus::Amber: return "Amber";
	case EKriStatus::Red: return "Red";
	}
	return "?";
}

EKriStatus EvaluateKri(const Kri& K)
{
	if (K.bHigherIsWorse)
	{
		if (K.Value >= K.RedThreshold) return EKriStatus::Red;
		if (K.Value >= K.AmberThreshold) return EKriStatus::Amber;
		return EKriStatus::Green;
	}
	if (K.Value <= K.RedThreshold) return EKriStatus::Red;
	if (K.Value <= K.AmberThreshold) return EKriStatus::Amber;
	return EKriStatus::Green;
}

bool KriRequiresEscalation(EKriStatus Current, int ConsecutiveAmberPeriods, int AmberPeriodsBeforeEscalation)
{
	if (Current == EKriStatus::Red) return true;
	return Current == EKriStatus::Amber && ConsecutiveAmberPeriods >= AmberPeriodsBeforeEscalation;
}

std::vector<SensitivityRow> SensitivityAnalysis(const ModelFn& Model, const std::vector<double>& Base,
	const std::vector<std::string>& Names, double Fraction)
{
	std::vector<SensitivityRow> Rows;
	for (size_t i = 0; i < Base.size(); ++i)
	{
		std::vector<double> Lo = Base;
		std::vector<double> Hi = Base;
		Lo[i] = Base[i] * (1.0 - Fraction);
		Hi[i] = Base[i] * (1.0 + Fraction);
		SensitivityRow R;
		R.ParameterIndex = static_cast<int>(i);
		R.Name = i < Names.size() ? Names[i] : std::string();
		R.OutputLow = Model(Lo);
		R.OutputHigh = Model(Hi);
		R.Swing = std::fabs(R.OutputHigh - R.OutputLow);
		Rows.push_back(R);
	}
	std::stable_sort(Rows.begin(), Rows.end(), [](const SensitivityRow& A, const SensitivityRow& B) { return A.Swing > B.Swing; });
	return Rows;
}

std::vector<SensitivityRow> AppraisalSensitivity(const TreatmentAppraisalInput& Base, double Fraction)
{
	const ModelFn Npv = [Base](const std::vector<double>& P) {
		TreatmentAppraisalInput In = Base;
		In.ExpectedLossBefore = P[0];
		In.ExpectedLossAfter = P[1];
		In.OneOffCost = P[2];
		In.AnnualCost = P[3];
		In.DiscountRate = P[4];
		return AppraiseTreatment(In).Npv;
	};
	return SensitivityAnalysis(Npv, {Base.ExpectedLossBefore, Base.ExpectedLossAfter, Base.OneOffCost, Base.AnnualCost, Base.DiscountRate},
		{"Expected loss before", "Expected loss after", "One-off cost", "Annual cost", "Discount rate"}, Fraction);
}

const std::vector<StressShock>& StandardStressShocks()
{
	static const std::vector<StressShock> S = {
		{"Severe heatwave", 2.0, 1.2},
		{"Key supplier failure", 1.5, 1.5},
		{"Regulatory penalty regime", 1.0, 1.8},
		{"Combined severe but plausible", 2.5, 2.0},
	};
	return S;
}

StressResult StressTest(const std::vector<LossScenario>& Scenarios, const StressShock& Shock, double LossCapacity)
{
	std::vector<LossScenario> Stressed = Scenarios;
	for (LossScenario& S : Stressed)
	{
		S.AnnualProbability = std::min(1.0, std::max(0.0, S.AnnualProbability * Shock.ProbabilityMultiplier));
		S.Loss = std::max(0.0, S.Loss * Shock.LossMultiplier);
	}
	StressResult R;
	R.ShockName = Shock.Name;
	R.BaseExpectedLoss = ExpectedLoss(Scenarios);
	R.StressedExpectedLoss = ExpectedLoss(Stressed);
	R.StressedWorstCase = WorstCaseLoss(Stressed);
	R.bBreachesCapacity = R.StressedWorstCase > LossCapacity;
	return R;
}

void LogNormalFromMedianP90(double Median, double P90, double& OutMu, double& OutSigma)
{
	const double Z90 = 1.2815515655446004;
	OutMu = std::log(std::max(1e-12, Median));
	OutSigma = std::max(0.0, (std::log(std::max(Median, P90)) - OutMu) / Z90);
}

static double Percentile(const std::vector<double>& Sorted, double P)
{
	if (Sorted.empty()) return 0.0;
	// Nearest-rank on [0, n-1] with linear interpolation.
	const double Pos = P * static_cast<double>(Sorted.size() - 1);
	const size_t Lo = static_cast<size_t>(std::floor(Pos));
	const size_t Hi = std::min(Sorted.size() - 1, Lo + 1);
	const double Frac = Pos - static_cast<double>(Lo);
	return Sorted[Lo] + (Sorted[Hi] - Sorted[Lo]) * Frac;
}

MonteCarloResult RunMonteCarlo(const MonteCarloInput& In)
{
	MonteCarloResult R;
	const int N = std::max(1, In.Trials);
	const double Keep = 1.0 - std::min(1.0, std::max(0.0, In.ControlSeverityReduction));
	Rng Random(In.Seed);
	std::vector<double> Annual(static_cast<size_t>(N));
	double Sum = 0.0;
	int Exceed = 0;
	for (int t = 0; t < N; ++t)
	{
		const int Events = Random.Poisson(In.EventsPerYear);
		double Loss = 0.0;
		for (int e = 0; e < Events; ++e)
		{
			Loss += Random.LogNormal(In.SeverityMu, In.SeveritySigma) * Keep;
		}
		Annual[static_cast<size_t>(t)] = Loss;
		Sum += Loss;
		if (Loss > In.LossThreshold) ++Exceed;
	}
	R.Trials = N;
	R.Mean = Sum / N;
	double Var = 0.0;
	for (double V : Annual)
	{
		Var += (V - R.Mean) * (V - R.Mean);
	}
	R.StdDev = N > 1 ? std::sqrt(Var / (N - 1)) : 0.0;
	std::sort(Annual.begin(), Annual.end());
	R.P50 = Percentile(Annual, 0.50);
	R.P90 = Percentile(Annual, 0.90);
	R.P95 = Percentile(Annual, 0.95);
	R.P99 = Percentile(Annual, 0.99);
	R.Max = Annual.back();
	R.ProbExceedThreshold = static_cast<double>(Exceed) / N;
	if (In.HistogramBins > 0)
	{
		const double Top = R.P99 > 0.0 ? R.P99 : (R.Max > 0.0 ? R.Max : 1.0);
		R.BinWidth = Top / In.HistogramBins;
		R.Histogram.assign(static_cast<size_t>(In.HistogramBins), 0);
		for (double V : Annual)
		{
			const int Bin = std::min(In.HistogramBins - 1, static_cast<int>(V / R.BinWidth));
			++R.Histogram[static_cast<size_t>(Bin)];
		}
	}
	return R;
}
} // namespace rb
