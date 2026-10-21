// Workstation tool hub view models (Gate 2). Thin calls into RiskboundSim: no formula lives here.
// Quant tools work on the player's own inputs only, so they never reveal a mission rubric.
#pragma once

#include "CoreMinimal.h"
#include "RbToolTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RbToolsSubsystem.generated.h"

class URbGameSubsystem;

UCLASS()
class RISKBOUND_API URbToolsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// ---- Risk register ----
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Register")
	TArray<FRbRegisterRow> GetRegister(const FRbRegisterFilter& Filter) const;
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Register")
	TArray<FString> GetCategories() const;
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Register")
	TArray<FRbMatrixCell> GetMatrix() const; // Size x Size cells, likelihood rows from high to low
	// Returns the new id, or empty with OutError set.
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Register")
	FString AddRisk(const FRbRiskInput& Input, FString& OutError);
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Register")
	bool UpdateRisk(const FString& RiskId, const FRbRiskInput& Input, FString& OutError);
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Register")
	FRbRiskInput GetRiskInput(const FString& RiskId) const;

	// ---- KRIs ----
	UFUNCTION(BlueprintCallable, Category = "Riskbound|KRI")
	TArray<FRbKriView> GetKris() const;

	// ---- Loss scenarios / expected loss ----
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Quant")
	FRbLossView EvaluateScenarios(const TArray<FRbScenarioInput>& Scenarios) const;
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Quant")
	bool SaveQuantification(const FString& RiskId, const TArray<FRbScenarioInput>& Scenarios);
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Quant")
	TArray<FRbScenarioInput> GetQuantification(const FString& RiskId) const;

	// ---- Cost-benefit, sensitivity, stress, Monte Carlo ----
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Quant")
	FRbAppraisalView Appraise(const FRbAppraisalInput& Input) const;
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Quant")
	TArray<FRbSensitivityRowView> Tornado(const FRbAppraisalInput& Input, double Fraction) const;
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Quant")
	TArray<FRbStressPreset> GetStressPresets() const;
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Quant")
	FRbStressView Stress(const TArray<FRbScenarioInput>& Scenarios, const FRbStressPreset& Shock, double LossCapacity) const;
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Quant")
	FRbMonteCarloView MonteCarlo(const FRbMonteCarloInput& Input, int32 HistogramBins = 30) const;

	// ---- Corrective actions and time ----
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Actions")
	TArray<FRbActionView> GetActions() const;
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Actions")
	FString CreateAction(const FString& RiskId, const FString& Title, const FString& Owner, int32 DueInWeeks, FString& OutError);
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Actions")
	bool StartAction(const FString& ActionId);
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Actions")
	bool CloseAction(const FString& ActionId, const FString& Evidence, FString& OutError);
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Actions")
	int32 GetWeek() const;
	// Advances organisation time one week: due consequences resolve, KRIs record a reading. Autosaves.
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Actions")
	TArray<FString> AdvanceWeek();

	// ---- Executive report ----
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Report")
	FString GetReportMarkdown() const;
	// Writes Saved/Reports/ExecutiveReport_Week<N>_<timestamp>.md; returns the full path or empty on failure.
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Report")
	FString ExportReport() const;

private:
	URbGameSubsystem* Game() const;
};
