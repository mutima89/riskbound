// Quantitative risk and financial tools. All money values are in the organisation's
// reporting currency (whole units). All probabilities are annual unless stated.
// Formulas, units and limitations: Docs/RiskMethodology.md.
#pragma once

#include "RbApi.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace rb
{
// ---------- Expected loss ----------
struct LossScenario
{
	std::string Name;
	double AnnualProbability = 0.0; // 0..1
	double Loss = 0.0;              // currency, >= 0
};

// EL = sum(p_i * L_i). A probability-weighted AVERAGE, never the worst case.
RB_API double ExpectedLoss(const std::vector<LossScenario>& Scenarios);
// The worst single scenario loss, reported next to EL so the two are not confused.
RB_API double WorstCaseLoss(const std::vector<LossScenario>& Scenarios);
// True when all probabilities are in [0,1] and losses are >= 0. If bMutuallyExclusive, also sum(p) <= 1.
RB_API bool ValidateScenarios(const std::vector<LossScenario>& Scenarios, bool bMutuallyExclusive);

// ALE = annual rate of occurrence * single loss expectancy.
RB_API double AnnualisedLossExpectancy(double AnnualRateOfOccurrence, double SingleLossExpectancy);

// ---------- Treatment cost-benefit ----------
struct TreatmentAppraisalInput
{
	double ExpectedLossBefore = 0.0; // per year
	double ExpectedLossAfter = 0.0;  // per year
	double OneOffCost = 0.0;         // year 0
	double AnnualCost = 0.0;         // years 1..N
	int Years = 3;
	double DiscountRate = 0.08;      // per year
};

struct TreatmentAppraisal
{
	double AnnualRiskReduction = 0.0;  // EL before - EL after
	double PvBenefit = 0.0;            // discounted risk reduction
	double PvCost = 0.0;               // one-off + discounted running cost
	double Npv = 0.0;                  // PvBenefit - PvCost
	double Roi = 0.0;                  // Npv / PvCost (0 when PvCost == 0)
	double BenefitCostRatio = 0.0;     // PvBenefit / PvCost
	bool bCostEffective = false;       // Npv > 0
};

RB_API TreatmentAppraisal AppraiseTreatment(const TreatmentAppraisalInput& In);

// ---------- Key risk indicators ----------
enum class EKriStatus
{
	Green,
	Amber,
	Red
};

RB_API const char* ToString(EKriStatus S);

struct Kri
{
	std::string Name;
	double Value = 0.0;
	double AmberThreshold = 0.0;
	double RedThreshold = 0.0;
	bool bHigherIsWorse = true;
};

RB_API EKriStatus EvaluateKri(const Kri& K);
// Red always escalates. Amber escalates after this many consecutive amber periods.
RB_API bool KriRequiresEscalation(EKriStatus Current, int ConsecutiveAmberPeriods, int AmberPeriodsBeforeEscalation = 3);

// ---------- Sensitivity (tornado) ----------
struct SensitivityRow
{
	int ParameterIndex = 0;
	std::string Name;
	double OutputLow = 0.0;
	double OutputHigh = 0.0;
	double Swing = 0.0; // |OutputHigh - OutputLow|
};

using ModelFn = std::function<double(const std::vector<double>&)>;

// One-at-a-time: each parameter moved by +/- Fraction of its base value. Rows sorted by swing, largest first.
std::vector<SensitivityRow> SensitivityAnalysis(const ModelFn& Model, const std::vector<double>& Base,
	const std::vector<std::string>& Names, double Fraction);

// ---------- Stress testing ----------
struct StressShock
{
	std::string Name;
	double ProbabilityMultiplier = 1.0;
	double LossMultiplier = 1.0;
};

struct StressResult
{
	std::string ShockName;
	double BaseExpectedLoss = 0.0;
	double StressedExpectedLoss = 0.0;
	double StressedWorstCase = 0.0;
	bool bBreachesCapacity = false; // stressed worst case > loss capacity
};

RB_API StressResult StressTest(const std::vector<LossScenario>& Scenarios, const StressShock& Shock, double LossCapacity);

// ---------- Monte Carlo (frequency x severity) ----------
struct MonteCarloInput
{
	double EventsPerYear = 1.0;      // Poisson lambda
	double SeverityMu = 10.0;        // lognormal mu of a single loss
	double SeveritySigma = 1.0;      // lognormal sigma
	double ControlSeverityReduction = 0.0; // 0..1 fraction removed from each loss
	double LossThreshold = 0.0;      // reports P(annual loss > threshold)
	int Trials = 10000;
	uint64_t Seed = 1;
};

struct MonteCarloResult
{
	double Mean = 0.0;
	double StdDev = 0.0;
	double P50 = 0.0;
	double P90 = 0.0;
	double P95 = 0.0;
	double P99 = 0.0;
	double Max = 0.0;
	double ProbExceedThreshold = 0.0;
	int Trials = 0;
};

// Lognormal parameters from a median and a 90th-percentile single-loss estimate (how
// subject-matter experts usually give ranges).
RB_API void LogNormalFromMedianP90(double Median, double P90, double& OutMu, double& OutSigma);

RB_API MonteCarloResult RunMonteCarlo(const MonteCarloInput& In);
} // namespace rb
