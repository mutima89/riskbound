// Workstation tool hub (Gate 2): risk register, KRI dashboard, loss scenarios, cost-benefit,
// sensitivity, stress test, Monte Carlo, action tracker and executive report. Every figure comes
// from URbToolsSubsystem; this widget only collects inputs and lays results out.
#pragma once

#include "CoreMinimal.h"
#include "RbToolTypes.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SButton;
class URbToolsSubsystem;

class SRbToolHub : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRbToolHub) {}
		SLATE_ARGUMENT(TWeakObjectPtr<URbToolsSubsystem>, Tools)
		SLATE_ARGUMENT(TFunction<void()>, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	TSharedPtr<SWidget> GetInitialFocus() const { return FirstTab; }

	enum class ETab : uint8 { Register, Kri, Loss, CostBenefit, Sensitivity, Stress, MonteCarlo, Actions, Report, Count };
	void ShowTab(ETab Tab);

private:
	void Rebuild();
	TSharedRef<SWidget> BuildRegister();
	TSharedRef<SWidget> BuildKri();
	TSharedRef<SWidget> BuildLoss();
	TSharedRef<SWidget> BuildCostBenefit();
	TSharedRef<SWidget> BuildSensitivity();
	TSharedRef<SWidget> BuildStress();
	TSharedRef<SWidget> BuildMonteCarlo();
	TSharedRef<SWidget> BuildActions();
	TSharedRef<SWidget> BuildReport();

	TSharedRef<SWidget> NumberField(const FText& Label, double* Value, double Min, double Max, double Delta, int32 Fraction = 0);
	TSharedRef<SWidget> TextField(const FText& Label, FString* Value, const FText& Hint = FText::GetEmpty());

	TWeakObjectPtr<URbToolsSubsystem> Tools;
	TFunction<void()> OnClose;
	TSharedPtr<SBox> Body;
	TSharedPtr<SWidget> FirstTab;
	ETab Current = ETab::Register;
	FText Status;

	// Tool inputs (kept while the hub is open).
	FRbRegisterFilter Filter;
	FString EditingId;
	FRbRiskInput RiskForm;
	FString QuantRiskId = TEXT("R-014");
	TArray<FRbScenarioInput> Scenarios;
	FRbAppraisalInput Appraisal;
	double SensitivityFraction = 0.1;
	FRbStressPreset Shock;
	double LossCapacity = 500000.0;
	FRbMonteCarloInput Mc;
	FRbMonteCarloView McResult;
	bool bMcRun = false;
	FString ActionRisk = TEXT("R-014");
	FString ActionTitle;
	FString ActionOwner;
	double ActionDue = 4.0;
	TMap<FString, FString> CloseEvidence;
	FString ExportedPath;
};
