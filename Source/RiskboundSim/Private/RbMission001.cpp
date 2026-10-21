#include "RbMission001.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace rb
{
const char* ToString(EDifficulty D)
{
	switch (D)
	{
	case EDifficulty::Analyst: return "Analyst";
	case EDifficulty::Professional: return "Professional";
	case EDifficulty::Executive: return "Executive";
	}
	return "?";
}

double IncidentProbabilityMultiplier(EDifficulty D)
{
	switch (D)
	{
	case EDifficulty::Analyst: return 0.8;
	case EDifficulty::Professional: return 1.0;
	case EDifficulty::Executive: return 1.2;
	}
	return 1.0;
}

namespace m001
{
const char* const MissionId = "M001";

// ---- Ground truth (hidden from the player until the debrief) ----
static const int TrueInherentL = 4;
static const int TrueInherentI = 4;
static const char* const RecommendedTreatment = "repair_and_escalation";
static const char* const CorrectOwner = "ops_site_manager";
static const char* const RegisterRiskId = "R-014";
static const char* const IncidentFlag = "northgate_controls_fixed";

const char* ObjectiveText(EObjective O)
{
	switch (O)
	{
	case EObjective::MeetManager: return "Meet Priya Okafor, Risk Manager, in her office";
	case EObjective::ReviewIncidentReport: return "Review the Northgate incident report";
	case EObjective::InspectRiskRegister: return "Inspect risk R-014 in the risk register";
	case EObjective::ReviewControlEvidence: return "Review evidence of control effectiveness";
	case EObjective::IdentifyMissingInformation: return "Identify missing information";
	case EObjective::AssessLikelihoodImpact: return "Assess inherent likelihood and impact";
	case EObjective::DetermineResidualRisk: return "Determine residual risk";
	case EObjective::RecommendTreatment: return "Recommend a treatment";
	case EObjective::SelectActionOwner: return "Select an action owner";
	case EObjective::DocumentRationale: return "Document your rationale";
	case EObjective::ReceiveDebrief: return "Receive your debrief";
	case EObjective::Count: break;
	}
	return "?";
}

const std::vector<EvidenceItem>& Evidence()
{
	static const std::vector<EvidenceItem> E = {
		{"incident_report", "Incident report IR-2291", "Junior analyst workspace - inbox",
			"Northgate cold store, Bay 3. Temperature reached 11C for 47 minutes overnight. One pallet of client "
			"vaccines-grade stock written off (42,000). Third excursion in six months. Alarm was acknowledged by the "
			"night security guard; no engineer was called.", false},
		{"risk_register_R014", "Risk register entry R-014", "Junior analyst workstation - risk register",
			"Cold-store temperature excursion. Likelihood 2, Impact 3 (Medium). Controls rated Effective. "
			"Last reviewed 14 months ago. Owner: 'Operations'.", false},
		{"alarm_log", "Alarm acknowledgement log", "Risk monitoring room - terminal",
			"212 temperature alarms in 90 days. 68% acknowledged within 2 minutes with no follow-up action recorded. "
			"Acknowledgement silences the alarm for 4 hours.", true},
		{"maintenance_log", "Compressor maintenance log", "Meeting room - binder",
			"Bay 3 compressor quarterly service: last completed 9 months ago. Two services skipped 'due to peak volume'. "
			"No record of compressor age or failure history.", true},
		{"manual_temp_log", "Weekly manual temperature checks", "Training area - clipboard",
			"Supervisors record readings weekly. 10 of the last 13 weeks completed. Readings taken during day shift only.", true},
		{"insurance_policy", "Stock and liability insurance schedule", "Manager's office - filing cabinet",
			"Excess 100,000 per claim. Excludes losses arising from gradual deterioration or failure to maintain "
			"equipment. Consequential loss of contract not covered.", false},
		{"client_contract", "Client service agreement (extract)", "Executive boardroom - document wallet",
			"Penalty of 250,000 per major temperature breach affecting client stock. Client may terminate after two "
			"major breaches in 12 months. Contract worth 1.8m margin over remaining term.", false},
	};
	return E;
}

const std::vector<Option>& MissingInfoCandidates()
{
	static const std::vector<Option> O = {
		{"compressor_failure_history", "Compressor age and failure history", "Needed to estimate the likelihood of a full failure."},
		{"after_hours_escalation", "After-hours escalation procedure", "Who should act when an alarm fires at night?"},
		{"alarm_threshold_config", "Alarm threshold configuration", "At what temperature and duration does the alarm trigger?"},
		{"client_penalty_terms", "Client penalty terms", "The financial penalty for a major breach."},
		{"insurance_excess", "Insurance excess", "The amount the company bears per claim."},
		{"fleet_fuel_costs", "Fleet fuel costs", "Diesel spend for the delivery fleet."},
	};
	return O;
}

static bool IsTrulyMissing(const std::string& Id)
{
	return Id == "compressor_failure_history" || Id == "after_hours_escalation" || Id == "alarm_threshold_config";
}

const std::vector<Option>& TreatmentOptions()
{
	static const std::vector<Option> O = {
		{"accept_monitor", "Accept and monitor", "Keep the current controls, re-review R-014 next quarter, and ask the site to report any further excursion the same day."},
		{"repair_and_escalation", "Service compressor and fix alarm escalation",
			"Overdue service now, 24/7 on-call engineer rota, alarms cannot be silenced without a logged action."},
		{"replace_plant", "Replace the Bay 3 refrigeration plant", "New plant with remote monitoring and a five-year maintenance contract; about twelve weeks to install."},
		{"increase_insurance", "Increase insurance cover", "Raise stock and liability limits with the current insurer so that a major excursion is fully covered."},
	};
	return O;
}

const std::vector<Option>& ActionOwnerOptions()
{
	static const std::vector<Option> O = {
		{"ops_site_manager", "Daniel Reyes, Northgate Site Manager (Operations)", "Runs the site, staff and maintenance budget."},
		{"it_systems", "Mei Lin, Building Systems Administrator (IT)", "Administers the alarm software."},
		{"risk_team", "You (Risk team)", "The risk function that raised the issue."},
		{"facilities_contractor", "Kelvarin Refrigeration Services (external maintenance contractor)", "Performs compressor servicing."},
	};
	return O;
}

const std::vector<Option>& RationaleTags()
{
	static const std::vector<Option> O = {
		{"alarm_ignored", "Alarms are acknowledged without action", ""},
		{"service_overdue", "Compressor servicing is overdue", ""},
		{"insurance_exclusion", "Insurance excludes failure-to-maintain losses", ""},
		{"penalty_exposure", "Client penalties and contract loss dominate the impact", ""},
		{"outside_appetite", "Residual risk exceeds operational risk appetite", ""},
		{"site_is_new", "The site was refurbished recently", ""},
		{"noise_complaints", "Staff complained about alarm noise", ""},
		{"competitor_fire", "A competitor had a warehouse fire", ""},
	};
	return O;
}

static bool IsValidRationale(const std::string& Id)
{
	return Id == "alarm_ignored" || Id == "service_overdue" || Id == "insurance_exclusion" || Id == "penalty_exposure" ||
		Id == "outside_appetite";
}

const std::vector<Control>& ExistingControls()
{
	static const std::vector<Control> C = {
		{"Temperature alarm monitoring", EControlType::Detective, 3, 1},
		{"Weekly manual temperature checks", EControlType::Detective, 2, 2},
		{"Quarterly compressor maintenance", EControlType::Preventive, 3, 1},
	};
	return C;
}

const std::vector<LossScenario>& BaselineScenarios()
{
	static const std::vector<LossScenario> S = {
		{"Minor excursion, stock written off", 0.50, 40000.0},
		{"Major spoilage with client penalty", 0.15, 290000.0},
		{"Client terminates contract", 0.03, 1800000.0},
	};
	return S;
}

static std::vector<LossScenario> ScenariosAfter(const std::string& TreatmentId)
{
	std::vector<LossScenario> S = BaselineScenarios();
	if (TreatmentId == "repair_and_escalation")
	{
		S[0].AnnualProbability = 0.15;
		S[1].AnnualProbability = 0.03;
		S[2].AnnualProbability = 0.005;
	}
	else if (TreatmentId == "replace_plant")
	{
		S[0].AnnualProbability = 0.08;
		S[1].AnnualProbability = 0.015;
		S[2].AnnualProbability = 0.003;
	}
	else if (TreatmentId == "increase_insurance")
	{
		// Only half of major-spoilage claims are sudden failures (claimable); recovery above the excess.
		S[1].Loss = 290000.0 - 0.5 * (290000.0 - 100000.0);
	}
	return S;
}

TreatmentAppraisalInput AppraisalInputFor(const std::string& TreatmentId)
{
	TreatmentAppraisalInput In;
	In.ExpectedLossBefore = ExpectedLoss(BaselineScenarios());
	In.ExpectedLossAfter = ExpectedLoss(ScenariosAfter(TreatmentId));
	In.Years = 3;
	In.DiscountRate = 0.08;
	if (TreatmentId == "repair_and_escalation")
	{
		In.OneOffCost = 18000.0;
		In.AnnualCost = 12000.0;
	}
	else if (TreatmentId == "replace_plant")
	{
		In.OneOffCost = 400000.0;
		In.AnnualCost = 5000.0;
	}
	else if (TreatmentId == "increase_insurance")
	{
		In.AnnualCost = 35000.0;
	}
	return In;
}

RiskEntry LegacyRegisterEntry()
{
	RiskEntry E;
	E.Id = RegisterRiskId;
	E.Title = "Cold-store temperature excursion";
	E.Cause = "Refrigeration failure or door left open";
	E.Event = "Stock held above 8C";
	E.Consequence = "Stock write-off";
	E.Category = "Operational";
	E.Owner = "Operations";
	E.InherentLikelihood = 2;
	E.InherentImpact = 3;
	E.Controls = {{"Temperature alarm monitoring", EControlType::Detective, 3, 3}};
	E.Treatment = ETreatment::Accept;
	E.Status = ERiskStatus::Open;
	return E;
}

// ---- Mission state ----
void MissionState::MeetManager()
{
	bObjectives[static_cast<int>(EObjective::MeetManager)] = true;
}

bool MissionState::ViewEvidence(const std::string& EvidenceId)
{
	const auto& All = Evidence();
	const auto It = std::find_if(All.begin(), All.end(), [&](const EvidenceItem& E) { return E.Id == EvidenceId; });
	if (It == All.end())
	{
		return false;
	}
	if (std::find(ViewedEvidence.begin(), ViewedEvidence.end(), EvidenceId) == ViewedEvidence.end())
	{
		ViewedEvidence.push_back(EvidenceId);
	}
	if (EvidenceId == "incident_report") bObjectives[static_cast<int>(EObjective::ReviewIncidentReport)] = true;
	if (EvidenceId == "risk_register_R014") bObjectives[static_cast<int>(EObjective::InspectRiskRegister)] = true;
	if (It->bControlEvidence) bObjectives[static_cast<int>(EObjective::ReviewControlEvidence)] = true;
	return true;
}

int MissionState::CurrentObjective() const
{
	for (int i = 0; i < ObjectiveCount; ++i)
	{
		if (!bObjectives[i]) return i;
	}
	return ObjectiveCount;
}

ESubmitError MissionState::Validate(const Submission& S) const
{
	if (bSubmitted) return ESubmitError::AlreadySubmitted;
	if (!IsComplete(EObjective::MeetManager)) return ESubmitError::ManagerNotMet;
	if (!IsComplete(EObjective::ReviewIncidentReport)) return ESubmitError::IncidentReportNotReviewed;
	if (!IsComplete(EObjective::InspectRiskRegister)) return ESubmitError::RegisterNotInspected;
	const auto InRange = [](int V) { return V >= 1 && V <= 5; };
	if (!InRange(S.InherentLikelihood) || !InRange(S.InherentImpact) || !InRange(S.ResidualLikelihood) || !InRange(S.ResidualImpact) ||
		S.ControlRatings.size() != ExistingControls().size() || S.TreatmentId.empty() || S.ActionOwnerId.empty() ||
		S.RationaleTagIds.empty() || S.RationaleTagIds.size() > 4)
	{
		return ESubmitError::IncompleteSubmission;
	}
	return ESubmitError::None;
}

static int Clamp100(int V)
{
	return std::max(0, std::min(100, V));
}

static int ClosenessScore(int Given, int Truth)
{
	const int D = std::abs(Given - Truth);
	return D == 0 ? 100 : (D == 1 ? 60 : 0);
}

static std::string Money(double V)
{
	return FormatMoney(V);
}

Debrief Score(const Submission& S, const std::vector<std::string>& ViewedEvidence)
{
	Debrief D;
	const RiskMatrix Matrix;
	const auto& Controls = ExistingControls();

	// 1. Evidence coverage
	const int Coverage = Clamp100(static_cast<int>(std::lround(100.0 * ViewedEvidence.size() / Evidence().size())));
	D.Criteria.push_back({"Evidence considered", Coverage, 10,
		Coverage == 100 ? "You reviewed every available source before concluding."
						: "Some evidence was not reviewed. Unreviewed sources can hide the facts that change a rating."});

	// 2. Missing information
	int Hits = 0;
	int False = 0;
	for (const std::string& Id : S.MissingInfo)
	{
		IsTrulyMissing(Id) ? ++Hits : ++False;
	}
	const int Missing = Clamp100(static_cast<int>(std::lround(100.0 * std::max(0, Hits - False) / 3.0)));
	D.Criteria.push_back({"Missing information", Missing, 15,
		"Genuinely missing: compressor failure history, the after-hours escalation procedure and the alarm threshold "
		"configuration. Penalty terms and the insurance excess were already in the evidence."});

	// 3. Inherent likelihood and impact
	const int LiScore = (ClosenessScore(S.InherentLikelihood, TrueInherentL) + ClosenessScore(S.InherentImpact, TrueInherentI)) / 2;
	D.Criteria.push_back({"Inherent likelihood and impact", LiScore, 20,
		"Three excursions in six months supports Likely (4). Client penalties and possible contract loss support "
		"Major impact (4). The register's 2 x 3 understated both."});

	// 4. Control evaluation and residual
	int ControlPts = 0;
	for (size_t i = 0; i < Controls.size(); ++i)
	{
		if (S.ControlRatings[i] == AssessControl(Controls[i])) ControlPts += 1;
	}
	const ResidualResult TrueResidual = ComputeResidual(Matrix, TrueInherentL, TrueInherentI, Controls);
	const int ResidualPts = (ClosenessScore(S.ResidualLikelihood, TrueResidual.Likelihood) + ClosenessScore(S.ResidualImpact, TrueResidual.Impact)) / 2;
	const int ControlScore = Clamp100(static_cast<int>(std::lround(50.0 * ControlPts / Controls.size() + 0.5 * ResidualPts)));
	D.Criteria.push_back({"Control effectiveness and residual risk", ControlScore, 15,
		"The alarm and the maintenance schedule are well designed but not operating, so they are ineffective. Weekly "
		"checks partly operate. Residual risk is " + std::to_string(TrueResidual.Likelihood) + " x " +
		std::to_string(TrueResidual.Impact) + " (" + ToString(Matrix.Rate(TrueResidual.Likelihood, TrueResidual.Impact)) +
		"), above the operational appetite limit of 8."});

	// 5. Treatment
	int TreatScore = 0;
	if (S.TreatmentId == RecommendedTreatment) TreatScore = 100;
	else if (S.TreatmentId == "replace_plant") TreatScore = 45;
	else if (S.TreatmentId == "increase_insurance") TreatScore = 25;
	D.ChosenAppraisal = AppraiseTreatment(AppraisalInputFor(S.TreatmentId));
	D.RecommendedAppraisal = AppraiseTreatment(AppraisalInputFor(RecommendedTreatment));
	D.Criteria.push_back({"Treatment recommendation", TreatScore, 25,
		"Servicing the compressor and fixing alarm escalation addresses both root causes at low cost (3-year NPV " +
		Money(D.RecommendedAppraisal.Npv) + "). Replacing the plant also reduces risk but costs more than it saves "
		"over three years and exceeds your approval authority. Insurance does not cover failure-to-maintain losses. "
		"Accepting leaves the risk outside appetite."});

	// 6. Action owner
	int OwnerScore = 0;
	if (S.ActionOwnerId == CorrectOwner) OwnerScore = 100;
	else if (S.ActionOwnerId == "it_systems") OwnerScore = 50;
	D.Criteria.push_back({"Action owner", OwnerScore, 5,
		"The site manager controls the people, maintenance budget and process, so can be held accountable. The risk "
		"team advises and challenges; it should not own first-line actions. A contractor cannot be accountable."});

	// 7. Rationale
	int Valid = 0;
	int Invalid = 0;
	for (const std::string& Id : S.RationaleTagIds)
	{
		IsValidRationale(Id) ? ++Valid : ++Invalid;
	}
	const int RationaleScore = Clamp100(25 * Valid - 25 * Invalid);
	D.Criteria.push_back({"Rationale", RationaleScore, 10,
		"Strong rationales cite the ignored alarms, the overdue servicing, the insurance exclusion, the penalty "
		"exposure and the appetite breach. Site age, noise and competitors' events do not bear on this risk."});

	double Weighted = 0.0;
	for (const CriterionResult& C : D.Criteria)
	{
		Weighted += C.Score * C.Weight / 100.0;
	}
	D.OverallScore = Clamp100(static_cast<int>(std::lround(Weighted)));
	D.bPassed = D.OverallScore >= 60 && TreatScore > 0;

	D.BaselineExpectedLoss = ExpectedLoss(BaselineScenarios());
	D.BaselineWorstCase = WorstCaseLoss(BaselineScenarios());
	D.Assumptions = {
		"Annual probabilities are estimates from six months of incidents; a longer history would narrow them.",
		"Expected loss before treatment is " + Money(D.BaselineExpectedLoss) + " per year. This is an average, not "
			"the worst case; a single contract termination would cost " + Money(D.BaselineWorstCase) + ".",
		"Cost-benefit uses 3 years at an 8% discount rate.",
		"Ordinal scores (1-5) rank the risk; they are not converted into money."};
	D.TradeOffs = {
		"Plant replacement lowers risk most but ties up 400,000 of capital the business needs elsewhere.",
		"An on-call rota adds running cost and depends on staff following the new procedure.",
		"Insurance shifts some cost but leaves the cause, and the client relationship, exposed."};
	for (const CriterionResult& C : D.Criteria)
	{
		if (C.Score < 70)
		{
			D.Improvements.push_back(C.Name + ": " + C.Feedback);
		}
	}

	const int Ident = (Coverage + Missing) / 2;
	D.CompetencyEvidence = {
		{ECompetency::RiskIdentification, Ident},
		{ECompetency::RiskAssessment, LiScore},
		{ECompetency::ControlEvaluation, ControlScore},
		{ECompetency::QuantitativeAnalysis, TreatScore},
		{ECompetency::Judgement, (TreatScore + OwnerScore) / 2},
		{ECompetency::Communication, RationaleScore},
	};
	D.XpAwarded = 100 + D.OverallScore;
	return D;
}

std::vector<CorrectiveAction> TreatmentActions(const Submission& S, int Week)
{
	std::string Owner = S.ActionOwnerId;
	for (const Option& O : ActionOwnerOptions())
	{
		if (O.Id == S.ActionOwnerId) Owner = O.Label;
	}
	const auto Make = [&](const char* Title, int DueInWeeks) {
		CorrectiveAction A;
		A.RiskId = RegisterRiskId;
		A.Title = Title;
		A.Owner = Owner;
		A.CreatedWeek = Week;
		A.DueWeek = Week + DueInWeeks;
		return A;
	};
	std::vector<CorrectiveAction> Out;
	if (S.TreatmentId == "repair_and_escalation")
	{
		Out.push_back(Make("Complete the overdue Bay 3 compressor service", 2));
		Out.push_back(Make("Introduce a 24/7 on-call engineer rota for temperature alarms", 4));
		Out.push_back(Make("Require a logged action before an alarm can be silenced", 4));
	}
	else if (S.TreatmentId == "replace_plant")
	{
		Out.push_back(Make("Escalate the Bay 3 plant replacement capital request (above delegated authority)", 3));
		Out.push_back(Make("Interim: complete the overdue Bay 3 compressor service", 2));
	}
	else if (S.TreatmentId == "increase_insurance")
	{
		Out.push_back(Make("Negotiate increased stock cover with the insurer", 4));
	}
	else if (S.TreatmentId == "accept_monitor")
	{
		Out.push_back(Make("Re-review R-014 next quarter", 13));
	}
	return Out;
}

ESubmitError MissionState::Submit(const Submission& S, EDifficulty Difficulty, CareerState& Career, OrganizationState& Org, RiskRegister& Register)
{
	const ESubmitError Err = Validate(S);
	if (Err != ESubmitError::None)
	{
		return Err;
	}
	Submitted = S;
	Result = Score(S, ViewedEvidence);
	bSubmitted = true;
	for (int O = static_cast<int>(EObjective::IdentifyMissingInformation); O < ObjectiveCount; ++O)
	{
		bObjectives[O] = true;
	}

	for (const AssessmentResult& A : Result.CompetencyEvidence)
	{
		Career.ApplyAssessment(A);
	}
	Career.RecordMission(MissionId, Result.bPassed, Result.XpAwarded);

	// Organisation consequences. Immediate effects come from the chosen treatment's costs.
	const TreatmentAppraisalInput Cost = AppraisalInputFor(S.TreatmentId);
	DecisionRecord Rec;
	Rec.Id = "M001-treatment";
	Rec.MissionId = MissionId;
	Rec.Summary = "Northgate cold store: " + S.TreatmentId + ", owner " + S.ActionOwnerId;
	Rec.QualityScore = Result.OverallScore;
	Rec.Effects.push_back({EOrgMetric::Cash, -Cost.OneOffCost});
	Rec.Effects.push_back({EOrgMetric::Costs, Cost.AnnualCost});
	if (S.TreatmentId == "repair_and_escalation")
	{
		Rec.Effects.push_back({EOrgMetric::RiskExposure, -12});
		Rec.Effects.push_back({EOrgMetric::ControlEffectiveness, 8});
		Org.SetFlag(IncidentFlag);
	}
	else if (S.TreatmentId == "replace_plant")
	{
		Rec.Effects.push_back({EOrgMetric::RiskExposure, -14});
		Rec.Effects.push_back({EOrgMetric::ControlEffectiveness, 6});
		Rec.Effects.push_back({EOrgMetric::StrategicProgress, -3});
		Org.SetFlag(IncidentFlag);
	}
	else if (S.TreatmentId == "increase_insurance")
	{
		Rec.Effects.push_back({EOrgMetric::RiskExposure, -2});
	}
	else
	{
		Rec.Effects.push_back({EOrgMetric::StakeholderTrust, -4});
		Org.SetFlag("m001_warning_ignored");
	}
	if (S.ActionOwnerId != CorrectOwner)
	{
		Rec.Effects.push_back({EOrgMetric::StakeholderTrust, -3});
	}
	Org.RecordDecision(Rec);

	PendingConsequence Incident;
	Incident.Id = "M001-northgate-failure";
	Incident.SourceDecisionId = Rec.Id;
	Incident.Description = "Northgate Bay 3 compressor fails during a weekend heatwave";
	Incident.TriggerWeek = Org.Week + 6;
	Incident.Probability = std::min(1.0, 0.55 * IncidentProbabilityMultiplier(Difficulty));
	Incident.EffectsIfOccurs = {{EOrgMetric::Cash, -290000}, {EOrgMetric::Reputation, -8}, {EOrgMetric::StakeholderTrust, -10},
		{EOrgMetric::RiskExposure, 10}};
	Incident.MitigatingFlag = IncidentFlag;
	Incident.MitigatedProbabilityFactor = 0.25;
	Incident.MitigatedSeverityFactor = 0.5;
	Org.Schedule(Incident);

	// The register entry is rewritten with the player's assessment; residual is recomputed by the engine.
	RiskEntry Updated = Register.Find(RegisterRiskId) ? *Register.Find(RegisterRiskId) : LegacyRegisterEntry();
	Updated.Cause = "Overdue compressor servicing and alarms silenced without action";
	Updated.Event = "Cold-store temperature excursion above 8C";
	Updated.Consequence = "Stock write-off, client penalties, possible contract termination";
	Updated.InherentLikelihood = S.InherentLikelihood;
	Updated.InherentImpact = S.InherentImpact;
	Updated.Controls = ExistingControls();
	Updated.Owner = "Northgate Site Manager";
	Updated.ActionOwner = S.ActionOwnerId;
	Updated.Evidence = ViewedEvidence;
	if (S.TreatmentId == "accept_monitor")
	{
		Updated.Treatment = ETreatment::Accept;
		Updated.Status = ERiskStatus::Open;
	}
	else if (S.TreatmentId == "increase_insurance")
	{
		Updated.Treatment = ETreatment::Transfer;
		Updated.Status = ERiskStatus::InTreatment;
	}
	else
	{
		Updated.Treatment = ETreatment::Reduce;
		Updated.Status = S.TreatmentId == "replace_plant" ? ERiskStatus::Escalated : ERiskStatus::InTreatment;
	}
	if (!Register.Update(Updated))
	{
		Register.Add(Updated);
	}
	return ESubmitError::None;
}
} // namespace m001
} // namespace rb
