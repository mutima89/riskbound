#include "RbReport.h"

#include "RbGameState.h"
#include "RbKri.h"

#include <algorithm>
#include <cstdio>

namespace rb
{
static ReportRiskLine RiskLine(const RiskEntry& E, const RiskRegister& Reg)
{
	ReportRiskLine L;
	L.Id = E.Id;
	L.Title = E.Title;
	L.Category = E.Category;
	L.Owner = E.Owner;
	L.ResidualLikelihood = E.ResidualLikelihood;
	L.ResidualImpact = E.ResidualImpact;
	L.ResidualScore = Reg.Matrix().Score(E.ResidualLikelihood, E.ResidualImpact);
	L.Rating = Reg.Matrix().Rate(E.ResidualLikelihood, E.ResidualImpact);
	L.Appetite = E.Appetite;
	L.AppetiteLimit = Reg.Appetite().LimitFor(E.Category);
	L.Treatment = E.Treatment;
	L.Status = E.Status;
	return L;
}

static ReportActionLine ActionLine(const CorrectiveAction& A, int Week)
{
	return {A.Id, A.RiskId, A.Title, A.Owner, A.DueWeek, ActionStatus(A, Week)};
}

ExecutiveReport BuildExecutiveReport(const GameState& State, int TopN)
{
	ExecutiveReport R;
	R.OrganisationName = State.Org.Name;
	R.Week = State.Org.Week;

	std::vector<ReportRiskLine> Risks;
	for (const RiskEntry& E : State.Register.Entries())
	{
		if (E.Status == ERiskStatus::Closed) continue;
		Risks.push_back(RiskLine(E, State.Register));
	}
	R.OpenRisks = static_cast<int>(Risks.size());
	std::stable_sort(Risks.begin(), Risks.end(), [](const ReportRiskLine& A, const ReportRiskLine& B) {
		return A.ResidualScore != B.ResidualScore ? A.ResidualScore > B.ResidualScore : A.Id < B.Id;
	});
	for (const ReportRiskLine& L : Risks)
	{
		if (static_cast<int>(R.TopRisks.size()) < TopN) R.TopRisks.push_back(L);
		if (L.Appetite == EAppetiteStatus::Exceeds) R.AppetiteBreaches.push_back(L);
	}

	for (const KriSeries& S : State.Kris.Series)
	{
		const KriDefinition* D = FindKri(S.Id);
		const KriReading* Last = S.Latest();
		if (!D || !Last) continue;
		R.Kris.push_back({D->Id, D->Name, D->Unit, Last->Value, Last->Status, Last->AmberStreak, Last->bEscalate});
	}

	for (const CorrectiveAction& A : State.Actions.All())
	{
		const ReportActionLine L = ActionLine(A, State.Org.Week);
		if (L.Status == EActionStatus::Overdue) R.OverdueActions.push_back(L);
		else if (L.Status != EActionStatus::Done) R.OpenActions.push_back(L);
	}
	const auto ByDue = [](const ReportActionLine& A, const ReportActionLine& B) {
		return A.DueWeek != B.DueWeek ? A.DueWeek < B.DueWeek : A.Id < B.Id;
	};
	std::stable_sort(R.OpenActions.begin(), R.OpenActions.end(), ByDue);
	std::stable_sort(R.OverdueActions.begin(), R.OverdueActions.end(), ByDue);

	for (const QuantifiedRisk& Q : State.Quantified)
	{
		R.Quantified.push_back({Q.RiskId, ExpectedLoss(Q.Scenarios), WorstCaseLoss(Q.Scenarios), static_cast<int>(Q.Scenarios.size())});
	}
	std::stable_sort(R.Quantified.begin(), R.Quantified.end(), [](const ReportQuantLine& A, const ReportQuantLine& B) { return A.RiskId < B.RiskId; });

	for (const DecisionRecord& D : State.Org.Decisions)
	{
		R.Decisions.push_back({D.Id, D.Summary, D.Week, D.QualityScore, D.Outcome.empty() ? std::string("Pending") : D.Outcome});
	}
	return R;
}

static std::string Money(double V)
{
	return FormatMoney(V);
}

static std::string Num1(double V)
{
	char Buf[32];
	std::snprintf(Buf, sizeof(Buf), "%.1f", V);
	return Buf;
}

// Table cells must not contain the column separator.
static std::string Cell(std::string V)
{
	std::replace(V.begin(), V.end(), '|', '/');
	std::replace(V.begin(), V.end(), '\n', ' ');
	return V;
}

static void RiskTable(std::string& Md, const std::vector<ReportRiskLine>& Lines)
{
	Md += "| Risk | Title | Category | Residual (L x I) | Rating | Appetite | Treatment | Owner |\n|---|---|---|---|---|---|---|---|\n";
	for (const ReportRiskLine& L : Lines)
	{
		Md += "| " + L.Id + " | " + Cell(L.Title) + " | " + Cell(L.Category) + " | " + std::to_string(L.ResidualLikelihood) + " x " +
			std::to_string(L.ResidualImpact) + " = " + std::to_string(L.ResidualScore) + " | " + ToString(L.Rating) + " | " +
			ToString(L.Appetite) + " (limit " + std::to_string(L.AppetiteLimit) + ") | " + ToString(L.Treatment) + " | " + Cell(L.Owner) + " |\n";
	}
}

static void ActionTable(std::string& Md, const std::vector<ReportActionLine>& Lines)
{
	Md += "| Action | Risk | Title | Owner | Due week | Status |\n|---|---|---|---|---|---|\n";
	for (const ReportActionLine& L : Lines)
	{
		Md += "| " + L.Id + " | " + L.RiskId + " | " + Cell(L.Title) + " | " + Cell(L.Owner) + " | " + std::to_string(L.DueWeek) + " | " + ToString(L.Status) + " |\n";
	}
}

std::string ReportToMarkdown(const ExecutiveReport& R)
{
	std::string Md = "# Executive risk report: " + R.OrganisationName + "\n\nWeek " + std::to_string(R.Week) + ". Open risks: " +
		std::to_string(R.OpenRisks) + ". Appetite breaches: " + std::to_string(R.AppetiteBreaches.size()) + ". Overdue actions: " +
		std::to_string(R.OverdueActions.size()) + ".\n\n";
	Md += "Residual scores are ordinal (likelihood x impact on a 5-point scale), not money.\n\n";

	Md += "## Top risks by residual rating\n\n";
	if (R.TopRisks.empty()) Md += "No open risks.\n";
	else RiskTable(Md, R.TopRisks);

	Md += "\n## Appetite breaches\n\n";
	if (R.AppetiteBreaches.empty()) Md += "None.\n";
	else RiskTable(Md, R.AppetiteBreaches);

	Md += "\n## Key risk indicators\n\n| KRI | Latest | Status | Amber streak | Escalate |\n|---|---|---|---|---|\n";
	for (const ReportKriLine& K : R.Kris)
	{
		Md += "| " + Cell(K.Name) + " | " + Num1(K.Value) + " " + K.Unit + " | " + ToString(K.Status) + " | " + std::to_string(K.AmberStreak) +
			" | " + (K.bEscalate ? "Yes" : "No") + " |\n";
	}

	Md += "\n## Overdue actions\n\n";
	if (R.OverdueActions.empty()) Md += "None.\n";
	else ActionTable(Md, R.OverdueActions);
	Md += "\n## Open actions\n\n";
	if (R.OpenActions.empty()) Md += "None.\n";
	else ActionTable(Md, R.OpenActions);

	Md += "\n## Quantified risks\n\nExpected loss is a probability-weighted annual average; the worst case is the largest single scenario.\n\n";
	if (R.Quantified.empty()) Md += "No risks quantified yet.\n";
	else
	{
		Md += "| Risk | Expected loss / year | Worst case | Scenarios |\n|---|---|---|---|\n";
		for (const ReportQuantLine& Q : R.Quantified)
		{
			Md += "| " + Q.RiskId + " | " + Money(Q.ExpectedLoss) + " | " + Money(Q.WorstCase) + " | " + std::to_string(Q.Scenarios) + " |\n";
		}
	}

	Md += "\n## Decisions: quality vs outcome\n\nDecision quality is judged on the information available at the time; the outcome is what chance produced.\n\n";
	if (R.Decisions.empty()) Md += "No decisions recorded yet.\n";
	else
	{
		Md += "| Decision | Week | Quality | Outcome |\n|---|---|---|---|\n";
		for (const ReportDecisionLine& D : R.Decisions)
		{
			Md += "| " + Cell(D.Summary) + " | " + std::to_string(D.Week) + " | " + std::to_string(D.QualityScore) + "/100 | " + Cell(D.Outcome) + " |\n";
		}
	}
	return Md;
}
} // namespace rb
