#include "RbToolsSubsystem.h"

#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RbConvert.h"
#include "RbGameSubsystem.h"
#include "RbKri.h"
#include "RbQuant.h"
#include "RbReport.h"
#include "RbSaveSubsystem.h"

#include <algorithm>
#include <set>

using namespace RbConvert;

namespace
{
std::vector<rb::LossScenario> ToSim(const TArray<FRbScenarioInput>& In)
{
	std::vector<rb::LossScenario> Out;
	for (const FRbScenarioInput& S : In)
	{
		Out.push_back({ToStd(S.Name), S.AnnualProbability, S.Loss});
	}
	return Out;
}

rb::TreatmentAppraisalInput ToSim(const FRbAppraisalInput& In)
{
	rb::TreatmentAppraisalInput A;
	A.ExpectedLossBefore = In.ExpectedLossBefore;
	A.ExpectedLossAfter = In.ExpectedLossAfter;
	A.OneOffCost = In.OneOffCost;
	A.AnnualCost = In.AnnualCost;
	A.Years = FMath::Clamp(In.Years, 1, 30);
	A.DiscountRate = In.DiscountRate;
	return A;
}

FRbRegisterRow ToRow(const rb::RiskEntry& E, const rb::RiskRegister& Reg)
{
	FRbRegisterRow R;
	R.Id = ToFString(E.Id);
	R.Title = ToFString(E.Title);
	R.Category = ToFString(E.Category);
	R.Owner = ToFString(E.Owner);
	R.ResidualLikelihood = E.ResidualLikelihood;
	R.ResidualImpact = E.ResidualImpact;
	R.ResidualScore = Reg.Matrix().Score(E.ResidualLikelihood, E.ResidualImpact);
	R.Rating = ToFString(rb::ToString(Reg.Matrix().Rate(E.ResidualLikelihood, E.ResidualImpact)));
	R.Appetite = ToFString(rb::ToString(E.Appetite));
	R.Status = ToFString(rb::ToString(E.Status));
	R.Treatment = ToFString(rb::ToString(E.Treatment));
	return R;
}

bool FillEntry(const FRbRiskInput& In, rb::RiskEntry& E, FString& OutError)
{
	if (In.Title.TrimStartAndEnd().IsEmpty() || In.Owner.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("A risk needs a title and an owner.");
		return false;
	}
	E.Title = ToStd(In.Title.TrimStartAndEnd());
	E.Category = ToStd(In.Category);
	E.Owner = ToStd(In.Owner.TrimStartAndEnd());
	E.Cause = ToStd(In.Cause);
	E.Event = ToStd(In.Event);
	E.Consequence = ToStd(In.Consequence);
	E.InherentLikelihood = In.Likelihood;
	E.InherentImpact = In.Impact;
	return true;
}
} // namespace

void URbToolsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<URbGameSubsystem>();
}

URbGameSubsystem* URbToolsSubsystem::Game() const
{
	return GetGameInstance()->GetSubsystem<URbGameSubsystem>();
}

// ---------------------------------------------------------------- register

TArray<FRbRegisterRow> URbToolsSubsystem::GetRegister(const FRbRegisterFilter& Filter) const
{
	const rb::RiskRegister& Reg = Game()->GetState().Register;
	rb::RiskFilter F;
	F.Category = ToStd(Filter.Category);
	F.bOnlyOutsideAppetite = Filter.bOnlyOutsideAppetite;
	std::vector<const rb::RiskEntry*> Hits = Reg.Filter(F);
	if (!Filter.Search.TrimStartAndEnd().IsEmpty())
	{
		const std::vector<const rb::RiskEntry*> Found = Reg.Search(ToStd(Filter.Search.TrimStartAndEnd()));
		const std::set<const rb::RiskEntry*> Keep(Found.begin(), Found.end());
		Hits.erase(std::remove_if(Hits.begin(), Hits.end(), [&Keep](const rb::RiskEntry* E) { return Keep.count(E) == 0; }), Hits.end());
	}
	TArray<FRbRegisterRow> Out;
	for (const rb::RiskEntry* E : Hits) Out.Add(ToRow(*E, Reg));
	return Out;
}

TArray<FString> URbToolsSubsystem::GetCategories() const
{
	TArray<FString> Out;
	for (const rb::AppetiteRule& R : Game()->GetState().Register.Appetite().Rules) Out.Add(ToFString(R.Category));
	return Out;
}

TArray<FRbMatrixCell> URbToolsSubsystem::GetMatrix() const
{
	const rb::RiskRegister& Reg = Game()->GetState().Register;
	const int32 N = Reg.Matrix().Size;
	TArray<FRbMatrixCell> Out;
	for (int32 L = N; L >= 1; --L)
	{
		for (int32 I = 1; I <= N; ++I)
		{
			FRbMatrixCell C;
			C.Likelihood = L;
			C.Impact = I;
			C.Rating = ToFString(rb::ToString(Reg.Matrix().Rate(L, I)));
			for (const rb::RiskEntry& E : Reg.Entries())
			{
				if (E.Status != rb::ERiskStatus::Closed && E.ResidualLikelihood == L && E.ResidualImpact == I) ++C.Count;
			}
			Out.Add(C);
		}
	}
	return Out;
}

FString URbToolsSubsystem::AddRisk(const FRbRiskInput& Input, FString& OutError)
{
	rb::RiskEntry E;
	if (!FillEntry(Input, E, OutError)) return FString();
	const std::string Id = Game()->EditState().Register.Add(E);
	if (Id.empty()) OutError = TEXT("The register rejected the risk.");
	Game()->StateEdited();
	return ToFString(Id);
}

bool URbToolsSubsystem::UpdateRisk(const FString& RiskId, const FRbRiskInput& Input, FString& OutError)
{
	rb::RiskRegister& Reg = Game()->EditState().Register;
	const rb::RiskEntry* Existing = Reg.Find(ToStd(RiskId));
	if (!Existing)
	{
		OutError = TEXT("Unknown risk.");
		return false;
	}
	rb::RiskEntry E = *Existing;
	if (!FillEntry(Input, E, OutError)) return false;
	const bool bOk = Reg.Update(E);
	Game()->StateEdited();
	return bOk;
}

FRbRiskInput URbToolsSubsystem::GetRiskInput(const FString& RiskId) const
{
	FRbRiskInput In;
	if (const rb::RiskEntry* E = Game()->GetState().Register.Find(ToStd(RiskId)))
	{
		In.Title = ToFString(E->Title);
		In.Category = ToFString(E->Category);
		In.Owner = ToFString(E->Owner);
		In.Cause = ToFString(E->Cause);
		In.Event = ToFString(E->Event);
		In.Consequence = ToFString(E->Consequence);
		In.Likelihood = E->InherentLikelihood;
		In.Impact = E->InherentImpact;
	}
	return In;
}

// ---------------------------------------------------------------- KRIs

TArray<FRbKriView> URbToolsSubsystem::GetKris() const
{
	TArray<FRbKriView> Out;
	for (const rb::KriSeries& S : Game()->GetState().Kris.Series)
	{
		const rb::KriDefinition* D = rb::FindKri(S.Id);
		const rb::KriReading* Last = S.Latest();
		if (!D || !Last) continue;
		FRbKriView V;
		V.Id = ToFString(D->Id);
		V.Name = ToFString(D->Name);
		V.Unit = ToFString(D->Unit);
		V.Value = Last->Value;
		V.Status = ToFString(rb::ToString(Last->Status));
		V.AmberStreak = Last->AmberStreak;
		V.bEscalate = Last->bEscalate;
		V.Amber = D->Amber;
		V.Red = D->Red;
		V.bHigherIsWorse = D->bHigherIsWorse;
		for (const rb::KriReading& R : S.History)
		{
			V.History.Add(R.Value);
			V.HistoryWeeks.Add(R.Week);
		}
		Out.Add(V);
	}
	return Out;
}

// ---------------------------------------------------------------- quant

FRbLossView URbToolsSubsystem::EvaluateScenarios(const TArray<FRbScenarioInput>& Scenarios) const
{
	FRbLossView V;
	const std::vector<rb::LossScenario> S = ToSim(Scenarios);
	if (S.empty())
	{
		V.Problem = TEXT("Add at least one scenario.");
		return V;
	}
	V.bValid = rb::ValidateScenarios(S, true);
	if (!V.bValid)
	{
		V.Problem = TEXT("Probabilities must be between 0 and 1 and add up to at most 1; losses cannot be negative.");
	}
	V.ExpectedLoss = rb::ExpectedLoss(S);
	V.WorstCase = rb::WorstCaseLoss(S);
	return V;
}

bool URbToolsSubsystem::SaveQuantification(const FString& RiskId, const TArray<FRbScenarioInput>& Scenarios)
{
	const bool bOk = rb::SetQuantification(Game()->EditState(), ToStd(RiskId), ToSim(Scenarios));
	if (bOk) Game()->StateEdited();
	return bOk;
}

TArray<FRbScenarioInput> URbToolsSubsystem::GetQuantification(const FString& RiskId) const
{
	TArray<FRbScenarioInput> Out;
	for (const rb::QuantifiedRisk& Q : Game()->GetState().Quantified)
	{
		if (Q.RiskId != ToStd(RiskId)) continue;
		for (const rb::LossScenario& S : Q.Scenarios) Out.Add({ToFString(S.Name), S.AnnualProbability, S.Loss});
	}
	return Out;
}

FRbAppraisalView URbToolsSubsystem::Appraise(const FRbAppraisalInput& Input) const
{
	const rb::TreatmentAppraisal A = rb::AppraiseTreatment(ToSim(Input));
	FRbAppraisalView V;
	V.TreatmentLabel = TEXT("Your inputs");
	V.AnnualRiskReduction = A.AnnualRiskReduction;
	V.PvBenefit = A.PvBenefit;
	V.PvCost = A.PvCost;
	V.Npv = A.Npv;
	V.Roi = A.Roi;
	V.BenefitCostRatio = A.BenefitCostRatio;
	V.bCostEffective = A.bCostEffective;
	return V;
}

TArray<FRbSensitivityRowView> URbToolsSubsystem::Tornado(const FRbAppraisalInput& Input, double Fraction) const
{
	TArray<FRbSensitivityRowView> Out;
	for (const rb::SensitivityRow& R : rb::AppraisalSensitivity(ToSim(Input), FMath::Clamp(Fraction, 0.01, 0.9)))
	{
		Out.Add({ToFString(R.Name), R.OutputLow, R.OutputHigh, R.Swing});
	}
	return Out;
}

TArray<FRbStressPreset> URbToolsSubsystem::GetStressPresets() const
{
	TArray<FRbStressPreset> Out;
	for (const rb::StressShock& S : rb::StandardStressShocks()) Out.Add({ToFString(S.Name), S.ProbabilityMultiplier, S.LossMultiplier});
	return Out;
}

FRbStressView URbToolsSubsystem::Stress(const TArray<FRbScenarioInput>& Scenarios, const FRbStressPreset& Shock, double LossCapacity) const
{
	const rb::StressResult R = rb::StressTest(ToSim(Scenarios), {ToStd(Shock.Name), Shock.ProbabilityMultiplier, Shock.LossMultiplier}, LossCapacity);
	return {ToFString(R.ShockName), R.BaseExpectedLoss, R.StressedExpectedLoss, R.StressedWorstCase, R.bBreachesCapacity};
}

FRbMonteCarloView URbToolsSubsystem::MonteCarlo(const FRbMonteCarloInput& Input, int32 HistogramBins) const
{
	FRbMonteCarloView V;
	rb::LogNormalFromMedianP90(FMath::Max(1.0, Input.MedianLoss), FMath::Max(Input.MedianLoss, Input.P90Loss), V.Mu, V.Sigma);
	rb::MonteCarloInput In;
	In.EventsPerYear = FMath::Max(0.0, Input.EventsPerYear);
	In.SeverityMu = V.Mu;
	In.SeveritySigma = V.Sigma;
	In.ControlSeverityReduction = FMath::Clamp(Input.ControlSeverityReduction, 0.0, 1.0);
	In.LossThreshold = Input.LossThreshold;
	In.Trials = FMath::Clamp(Input.Trials, 100, 200000);
	In.Seed = static_cast<uint64_t>(FMath::Max(0, Input.Seed));
	In.HistogramBins = FMath::Clamp(HistogramBins, 0, 200);
	const rb::MonteCarloResult R = rb::RunMonteCarlo(In);
	V.Mean = R.Mean;
	V.StdDev = R.StdDev;
	V.P50 = R.P50;
	V.P90 = R.P90;
	V.P95 = R.P95;
	V.P99 = R.P99;
	V.Max = R.Max;
	V.ProbExceedThreshold = R.ProbExceedThreshold;
	V.Trials = R.Trials;
	for (int C : R.Histogram) V.Histogram.Add(C);
	V.BinWidth = R.BinWidth;
	return V;
}

// ---------------------------------------------------------------- actions

TArray<FRbActionView> URbToolsSubsystem::GetActions() const
{
	const rb::GameState& S = Game()->GetState();
	TArray<FRbActionView> Out;
	for (const rb::CorrectiveAction& A : S.Actions.All())
	{
		FRbActionView V;
		V.Id = ToFString(A.Id);
		V.RiskId = ToFString(A.RiskId);
		V.Title = ToFString(A.Title);
		V.Owner = ToFString(A.Owner);
		V.DueWeek = A.DueWeek;
		V.Status = ToFString(rb::ToString(rb::ActionStatus(A, S.Org.Week)));
		V.bDone = A.bDone;
		V.Evidence = ToFString(A.CompletionEvidence);
		Out.Add(V);
	}
	return Out;
}

FString URbToolsSubsystem::CreateAction(const FString& RiskId, const FString& Title, const FString& Owner, int32 DueInWeeks, FString& OutError)
{
	rb::GameState& S = Game()->EditState();
	if (!S.Register.Find(ToStd(RiskId)))
	{
		OutError = TEXT("Choose an existing risk id (for example R-014).");
		return FString();
	}
	rb::CorrectiveAction A;
	A.RiskId = ToStd(RiskId);
	A.Title = ToStd(Title.TrimStartAndEnd());
	A.Owner = ToStd(Owner.TrimStartAndEnd());
	A.CreatedWeek = S.Org.Week;
	A.DueWeek = S.Org.Week + FMath::Max(0, DueInWeeks);
	const std::string Id = S.Actions.Create(A);
	if (Id.empty()) OutError = TEXT("An action needs a title and an owner.");
	Game()->StateEdited();
	return ToFString(Id);
}

bool URbToolsSubsystem::StartAction(const FString& ActionId)
{
	const bool bOk = Game()->EditState().Actions.Start(ToStd(ActionId));
	Game()->StateEdited();
	return bOk;
}

bool URbToolsSubsystem::CloseAction(const FString& ActionId, const FString& Evidence, FString& OutError)
{
	rb::GameState& S = Game()->EditState();
	const bool bOk = S.Actions.Close(ToStd(ActionId), S.Org.Week, ToStd(Evidence.TrimStartAndEnd()));
	if (!bOk) OutError = TEXT("Closing an action needs evidence of completion.");
	Game()->StateEdited();
	return bOk;
}

int32 URbToolsSubsystem::GetWeek() const
{
	return Game()->GetState().Org.Week;
}

TArray<FString> URbToolsSubsystem::AdvanceWeek()
{
	TArray<FString> Events;
	for (const rb::ResolvedConsequence& R : rb::AdvanceGameWeeks(Game()->EditState(), 1))
	{
		Events.Add(FString::Printf(TEXT("%s: %s"), R.bOccurred ? TEXT("Incident") : TEXT("Avoided"), *ToFString(R.Description)));
	}
	for (const FString& E : Events) Game()->Notify(E);
	Game()->Notify(FString::Printf(TEXT("Week %d"), GetWeek()));
	Game()->StateEdited();
	if (URbSaveSubsystem* Save = GetGameInstance()->GetSubsystem<URbSaveSubsystem>()) Save->Autosave();
	return Events;
}

// ---------------------------------------------------------------- report

FString URbToolsSubsystem::GetReportMarkdown() const
{
	return ToFString(rb::ReportToMarkdown(rb::BuildExecutiveReport(Game()->GetState())));
}

FString URbToolsSubsystem::ExportReport() const
{
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Reports"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString Path = Dir / FString::Printf(TEXT("ExecutiveReport_Week%d_%s.md"), GetWeek(), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	return FFileHelper::SaveStringToFile(GetReportMarkdown(), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ? Path : FString();
}
