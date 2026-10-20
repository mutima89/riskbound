// View models for the workstation tool hub (Gate 2). Inputs are the player's own numbers;
// every figure is computed by RiskboundSim through URbToolsSubsystem.
#pragma once

#include "CoreMinimal.h"
#include "RbTypes.h" // FRbAppraisalView
#include "RbToolTypes.generated.h"

USTRUCT(BlueprintType)
struct FRbRiskInput
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") FString Title;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") FString Category = TEXT("Operational");
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") FString Owner;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") FString Cause;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") FString Event;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") FString Consequence;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") int32 Likelihood = 3;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") int32 Impact = 3;
};

USTRUCT(BlueprintType)
struct FRbRegisterRow
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Category;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Owner;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 ResidualLikelihood = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 ResidualImpact = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 ResidualScore = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Rating;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Appetite;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Status;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Treatment;
};

USTRUCT(BlueprintType)
struct FRbRegisterFilter
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") FString Search;    // matches id, title, cause, event, consequence, owner
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") FString Category;  // empty = any
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") bool bOnlyOutsideAppetite = false;
};

USTRUCT(BlueprintType)
struct FRbMatrixCell
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Likelihood = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Impact = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Rating;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Count = 0; // open risks with this residual L x I
};

USTRUCT(BlueprintType)
struct FRbKriView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Unit;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double Value = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Status;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 AmberStreak = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bEscalate = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double Amber = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double Red = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bHigherIsWorse = true;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<double> History; // oldest first
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<int32> HistoryWeeks;
};

USTRUCT(BlueprintType)
struct FRbScenarioInput
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") FString Name;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") double AnnualProbability = 0.0;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") double Loss = 0.0;
};

USTRUCT(BlueprintType)
struct FRbLossView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Problem;  // why the scenarios are invalid
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double ExpectedLoss = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double WorstCase = 0.0;
};

USTRUCT(BlueprintType)
struct FRbAppraisalInput
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") double ExpectedLossBefore = 0.0;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") double ExpectedLossAfter = 0.0;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") double OneOffCost = 0.0;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") double AnnualCost = 0.0;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") int32 Years = 3;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") double DiscountRate = 0.08;
};

USTRUCT(BlueprintType)
struct FRbSensitivityRowView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double OutputLow = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double OutputHigh = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double Swing = 0.0;
};

USTRUCT(BlueprintType)
struct FRbStressView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString ShockName;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double BaseExpectedLoss = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double StressedExpectedLoss = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double StressedWorstCase = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bBreachesCapacity = false;
};

USTRUCT(BlueprintType)
struct FRbStressPreset
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double ProbabilityMultiplier = 1.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double LossMultiplier = 1.0;
};

USTRUCT(BlueprintType)
struct FRbMonteCarloInput
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") double EventsPerYear = 2.0;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") double MedianLoss = 25000.0; // single-loss median (expert estimate)
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") double P90Loss = 120000.0;   // single-loss 90th percentile
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") double ControlSeverityReduction = 0.0;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") double LossThreshold = 250000.0;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") int32 Trials = 10000;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") int32 Seed = 1;
};

USTRUCT(BlueprintType)
struct FRbMonteCarloView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double Mu = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double Sigma = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double Mean = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double StdDev = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double P50 = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double P90 = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double P95 = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double P99 = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double Max = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double ProbExceedThreshold = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Trials = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<int32> Histogram;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double BinWidth = 0.0;
};

USTRUCT(BlueprintType)
struct FRbActionView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString RiskId;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Owner;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 DueWeek = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Status;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bDone = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Evidence;
};
