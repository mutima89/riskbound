#include "RbTest.h"

#include "RbQuant.h"
#include "RbRng.h"

using namespace rb;

TEST(ExpectedLoss_IsWeightedAverageNotWorstCase)
{
	const std::vector<LossScenario> S = {{"a", 0.5, 40000}, {"b", 0.15, 290000}, {"c", 0.03, 1800000}};
	CHECK_NEAR(ExpectedLoss(S), 117500.0, 1e-6);
	CHECK_NEAR(WorstCaseLoss(S), 1800000.0, 1e-6);
	CHECK(ExpectedLoss(S) < WorstCaseLoss(S));
	CHECK(ValidateScenarios(S, true));
	CHECK(!ValidateScenarios({{"x", 0.7, 1}, {"y", 0.6, 1}}, true));
	CHECK(ValidateScenarios({{"x", 0.7, 1}, {"y", 0.6, 1}}, false));
	CHECK(!ValidateScenarios({{"neg", 0.1, -5}}, false));
	CHECK_NEAR(AnnualisedLossExpectancy(0.25, 80000), 20000.0, 1e-9);
}

TEST(Appraisal_NpvRoiDiscounting)
{
	TreatmentAppraisalInput In;
	In.ExpectedLossBefore = 117500;
	In.ExpectedLossAfter = 23700;
	In.OneOffCost = 18000;
	In.AnnualCost = 12000;
	In.Years = 3;
	In.DiscountRate = 0.08;
	const TreatmentAppraisal A = AppraiseTreatment(In);
	const double Annuity = 1 / 1.08 + 1 / (1.08 * 1.08) + 1 / (1.08 * 1.08 * 1.08);
	CHECK_NEAR(A.AnnualRiskReduction, 93800, 1e-6);
	CHECK_NEAR(A.PvBenefit, 93800 * Annuity, 1e-6);
	CHECK_NEAR(A.PvCost, 18000 + 12000 * Annuity, 1e-6);
	CHECK_NEAR(A.Npv, A.PvBenefit - A.PvCost, 1e-9);
	CHECK_NEAR(A.Roi, A.Npv / A.PvCost, 1e-12);
	CHECK(A.bCostEffective);

	TreatmentAppraisalInput Zero;
	const TreatmentAppraisal Z = AppraiseTreatment(Zero);
	CHECK_NEAR(Z.Roi, 0.0, 1e-12);
	CHECK(!Z.bCostEffective);
}

TEST(Kri_ThresholdsBothDirectionsAndEscalation)
{
	CHECK(EvaluateKri({"alarms", 50, 60, 80, true}) == EKriStatus::Green);
	CHECK(EvaluateKri({"alarms", 60, 60, 80, true}) == EKriStatus::Amber);
	CHECK(EvaluateKri({"alarms", 95, 60, 80, true}) == EKriStatus::Red);
	CHECK(EvaluateKri({"liquidity days", 40, 30, 15, false}) == EKriStatus::Green);
	CHECK(EvaluateKri({"liquidity days", 20, 30, 15, false}) == EKriStatus::Amber);
	CHECK(EvaluateKri({"liquidity days", 10, 30, 15, false}) == EKriStatus::Red);
	CHECK(KriRequiresEscalation(EKriStatus::Red, 0));
	CHECK(!KriRequiresEscalation(EKriStatus::Amber, 2));
	CHECK(KriRequiresEscalation(EKriStatus::Amber, 3));
	CHECK(!KriRequiresEscalation(EKriStatus::Green, 10));
}

TEST(Sensitivity_RanksByInfluence)
{
	// EL = p * loss + fixed. Loss is large so it dominates.
	const ModelFn Model = [](const std::vector<double>& P) { return P[0] * P[1] + P[2]; };
	const auto Rows = SensitivityAnalysis(Model, {0.2, 1000000, 5000}, {"probability", "loss", "fixed"}, 0.1);
	REQUIRE(Rows.size() == size_t(3));
	CHECK(Rows[2].Name == "fixed");
	CHECK_NEAR(Rows[0].Swing, 40000.0, 1e-6); // p and loss tie: 0.2*1e6*0.2
	CHECK_NEAR(Rows[2].Swing, 1000.0, 1e-6);
}

TEST(Stress_ShocksAndCapacity)
{
	const std::vector<LossScenario> S = {{"a", 0.5, 100}, {"b", 0.6, 1000}};
	const StressResult R = StressTest(S, {"severe", 2.0, 1.5}, 1200);
	CHECK_NEAR(R.BaseExpectedLoss, 650, 1e-9);
	CHECK_NEAR(R.StressedExpectedLoss, 1.0 * 150 + 1.0 * 1500, 1e-9); // probabilities capped at 1
	CHECK_NEAR(R.StressedWorstCase, 1500, 1e-9);
	CHECK(R.bBreachesCapacity);
}

TEST(Rng_DeterministicAndInRange)
{
	Rng A(42), B(42), C(43);
	bool bSame = true;
	bool bDiffers = false;
	for (int i = 0; i < 1000; ++i)
	{
		const uint64_t a = A.NextU64();
		if (a != B.NextU64()) bSame = false;
		if (a != C.NextU64()) bDiffers = true;
	}
	CHECK(bSame);
	CHECK(bDiffers);
	Rng R(7);
	double Sum = 0;
	for (int i = 0; i < 20000; ++i)
	{
		const double U = R.Uniform();
		CHECK(U >= 0.0 && U < 1.0);
		Sum += R.Poisson(2.0);
	}
	CHECK_NEAR(Sum / 20000, 2.0, 0.05);
}

TEST(MonteCarlo_ReproducibleAndMatchesAnalyticMean)
{
	MonteCarloInput In;
	In.EventsPerYear = 2.0;
	In.SeverityMu = 10.0;
	In.SeveritySigma = 0.5;
	In.Trials = 50000;
	In.Seed = 99;
	In.LossThreshold = 100000;
	const MonteCarloResult A = RunMonteCarlo(In);
	const MonteCarloResult B = RunMonteCarlo(In);
	CHECK_NEAR(A.Mean, B.Mean, 0.0);
	CHECK_NEAR(A.P99, B.P99, 0.0);
	const double Analytic = 2.0 * std::exp(10.0 + 0.5 * 0.5 * 0.5); // lambda * E[lognormal]
	CHECK_NEAR(A.Mean / Analytic, 1.0, 0.02);
	CHECK(A.P50 <= A.P90 && A.P90 <= A.P95 && A.P95 <= A.P99 && A.P99 <= A.Max);
	CHECK(A.ProbExceedThreshold > 0.0 && A.ProbExceedThreshold < 1.0);

	In.ControlSeverityReduction = 0.5;
	const MonteCarloResult Ctl = RunMonteCarlo(In);
	CHECK_NEAR(Ctl.Mean, A.Mean * 0.5, A.Mean * 1e-9); // same draws, halved severities

	double Mu = 0, Sigma = 0;
	LogNormalFromMedianP90(50000, 150000, Mu, Sigma);
	CHECK_NEAR(std::exp(Mu), 50000, 1e-6);
	CHECK_NEAR(std::exp(Mu + 1.2815515655446004 * Sigma), 150000, 1e-6);
}

TEST(Quant_AppraisalSensitivityHandComputed)
{
	TreatmentAppraisalInput In;
	In.ExpectedLossBefore = 117500.0;
	In.ExpectedLossAfter = 37500.0;
	In.OneOffCost = 18000.0;
	In.AnnualCost = 12000.0;
	In.Years = 3;
	In.DiscountRate = 0.08;
	const std::vector<SensitivityRow> Rows = AppraisalSensitivity(In, 0.10);
	REQUIRE(Rows.size() == size_t(5));
	// NPV is linear in EL before with slope = annuity factor sum(1/1.08^t, t=1..3) = 2.5770970.
	// Swing = 2 x 10% x 117,500 x 2.5770970 = 60,561.78, the largest of the five.
	CHECK(Rows[0].Name == "Expected loss before");
	CHECK_NEAR(Rows[0].Swing, 60561.78, 0.05);
	for (size_t i = 1; i < Rows.size(); ++i) CHECK(Rows[i - 1].Swing >= Rows[i].Swing);
	// One-off cost: NPV falls 1:1, swing = 2 x 1,800 = 3,600.
	for (const SensitivityRow& R : Rows)
	{
		if (R.Name == "One-off cost") CHECK_NEAR(R.Swing, 3600.0, 1e-6);
	}
}

TEST(Quant_StressPresetsHandComputed)
{
	const std::vector<StressShock>& Presets = StandardStressShocks();
	REQUIRE(Presets.size() == size_t(4));
	const StressShock& Combined = Presets[3];
	CHECK_NEAR(Combined.ProbabilityMultiplier, 2.5, 0.0);
	const std::vector<LossScenario> Base = {{"minor", 0.50, 40000.0}, {"major", 0.15, 290000.0}, {"terminate", 0.03, 1800000.0}};
	const StressResult R = StressTest(Base, Combined, 2000000.0);
	// p: 1.25 capped to 1.0, 0.375, 0.075; L x2: 80k, 580k, 3.6m -> EL = 80,000 + 217,500 + 270,000.
	CHECK_NEAR(R.StressedExpectedLoss, 567500.0, 1e-6);
	CHECK_NEAR(R.StressedWorstCase, 3600000.0, 1e-6);
	CHECK(R.bBreachesCapacity);
}

TEST(Quant_MonteCarloHistogramCountsEveryTrial)
{
	MonteCarloInput In;
	In.EventsPerYear = 2.0;
	In.SeverityMu = 10.0;
	In.SeveritySigma = 0.8;
	In.Trials = 5000;
	In.Seed = 11;
	const MonteCarloResult Plain = RunMonteCarlo(In);
	CHECK(Plain.Histogram.empty());
	In.HistogramBins = 20;
	const MonteCarloResult R = RunMonteCarlo(In);
	REQUIRE(R.Histogram.size() == size_t(20));
	int Total = 0;
	for (int C : R.Histogram) Total += C;
	CHECK_EQ(Total, 5000);
	CHECK_NEAR(R.BinWidth, R.P99 / 20.0, 1e-9);
	CHECK_NEAR(R.Mean, Plain.Mean, 0.0); // the histogram never changes the statistics
	CHECK(R.Histogram.back() >= 50);     // at least the 1% above P99 lands in the last bin
}
