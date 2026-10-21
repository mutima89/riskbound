#include "RbGameState.h"

#include "RbCampaign.h"
#include "RbMissionData.h"

#include <set>

#include <algorithm>
#include <cmath>
#include <functional>

namespace rb
{
PlayerSettings ClampSettings(const PlayerSettings& In)
{
	const PlayerSettings Def;
	auto Clamp = [](double V, double Lo, double Hi, double Fallback) { return std::isfinite(V) ? std::max(Lo, std::min(Hi, V)) : Fallback; };
	PlayerSettings Out = In;
	Out.MouseSensitivity = Clamp(In.MouseSensitivity, 0.1, 5.0, Def.MouseSensitivity);
	Out.TextScale = Clamp(In.TextScale, 0.8, 1.6, Def.TextScale);
	Out.MasterVolume = Clamp(In.MasterVolume, 0.0, 1.0, Def.MasterVolume);
	Out.GraphicsQuality = std::max(0, std::min(3, In.GraphicsQuality));
	return Out;
}

RiskRegister MakeDefaultRegister()
{
	RiskAppetite Appetite;
	Appetite.DefaultMaxResidualScore = 9;
	Appetite.Rules = {{"Operational", 8}, {"Compliance", 4}, {"Financial", 10}, {"Strategic", 12}, {"Cyber", 6}};
	RiskRegister Reg(RiskMatrix{}, Appetite);
	Reg.SetNextIdNumber(15);
	Reg.Add(m001::LegacyRegisterEntry());
	return Reg;
}

GameState NewGame(const std::string& ProfileName, EDifficulty Difficulty, uint64_t Seed, const std::string& OrgProfileId)
{
	GameState S;
	S.ProfileName = ProfileName;
	S.Difficulty = Difficulty;
	S.Seed = Seed;
	for (const OrganizationProfile& P : OrganizationProfiles())
	{
		if (P.Id == OrgProfileId)
		{
			S.Org = OrganizationState(P);
		}
	}
	S.Org.EventSeed = Seed * 0x2545F4914F6CDD1DULL + 1;
	S.Register = MakeDefaultRegister();
	S.Settings.bHints = Difficulty != EDifficulty::Executive;
	S.Kris = MakeKriMonitor(S.Org, Seed, KriHistoryWeeksAtStart);
	return S;
}

m001::ESubmitError SubmitMission001(GameState& State, const m001::Submission& S)
{
	const m001::ESubmitError Err = State.Mission001.Submit(S, State.Difficulty, State.Career, State.Org, State.Register);
	if (Err == m001::ESubmitError::None)
	{
		for (const CorrectiveAction& A : m001::TreatmentActions(S, State.Org.Week))
		{
			State.Actions.Create(A);
		}
	}
	return Err;
}

std::vector<ResolvedConsequence> AdvanceGameWeeks(GameState& State, int Weeks)
{
	std::vector<ResolvedConsequence> Resolved = State.Org.AdvanceWeeks(Weeks);
	RecordKriWeeks(State.Kris, State.Org);
	return Resolved;
}

bool SetQuantification(GameState& State, const std::string& RiskId, const std::vector<LossScenario>& Scenarios)
{
	if (!State.Register.Find(RiskId) || Scenarios.empty() || !ValidateScenarios(Scenarios, true)) return false;
	for (QuantifiedRisk& Q : State.Quantified)
	{
		if (Q.RiskId == RiskId)
		{
			Q.Scenarios = Scenarios;
			return true;
		}
	}
	State.Quantified.push_back({RiskId, Scenarios});
	return true;
}

struct AchievementRule
{
	AchievementDef Def;
	std::function<bool(const GameState&)> Condition;
};

static const std::vector<AchievementRule>& Rules()
{
	using namespace m001;
	static const std::vector<AchievementRule> R = {
		{{"first_day", "First Day", "Meet your manager at Valtren."},
			[](const GameState& S) { return S.Mission001.IsComplete(EObjective::MeetManager); }},
		{{"paper_trail", "Paper Trail", "Review every piece of evidence in Mission 001."},
			[](const GameState& S) { return S.Mission001.ViewedEvidence.size() == Evidence().size(); }},
		{{"unresolved_no_more", "Unresolved No More", "Pass Mission 001."},
			[](const GameState& S) { return S.Career.HasPassed(MissionId); }},
		{{"right_owner", "Accountable", "Assign a risk action to the person who can actually deliver it."},
			[](const GameState& S) { return S.Mission001.bSubmitted && S.Mission001.Submitted.ActionOwnerId == "ops_site_manager"; }},
		{{"value_for_money", "Value for Money", "Recommend a treatment with a positive 3-year NPV."},
			[](const GameState& S) { return S.Mission001.bSubmitted && S.Mission001.Result.ChosenAppraisal.bCostEffective; }},
		{{"sharp_eye", "Sharp Eye", "Identify all missing information without any false leads."},
			[](const GameState& S) {
				if (!S.Mission001.bSubmitted) return false;
				for (const auto& C : S.Mission001.Result.Criteria)
				{
					if (C.Name == "Missing information") return C.Score == 100;
				}
				return false;
			}},
		{{"good_call_bad_luck", "Good Call, Bad Luck", "Make a high-quality decision and suffer an incident anyway."},
			[](const GameState& S) {
				for (const auto& D : S.Org.Decisions)
				{
					for (const auto& H : S.Org.History)
					{
						if (H.SourceDecisionId == D.Id && H.bOccurred && D.QualityScore >= 80) return true;
					}
				}
				return false;
			}},
	};
	return R;
}

const std::vector<AchievementDef>& AchievementDefs()
{
	static const std::vector<AchievementDef> Defs = [] {
		std::vector<AchievementDef> Out;
		for (const AchievementRule& R : Rules()) Out.push_back(R.Def);
		return Out;
	}();
	return Defs;
}

std::vector<std::string> EvaluateAchievements(GameState& State)
{
	std::vector<std::string> New;
	for (const AchievementRule& R : Rules())
	{
		const bool bHave = std::find(State.Achievements.begin(), State.Achievements.end(), R.Def.Id) != State.Achievements.end();
		if (!bHave && R.Condition(State))
		{
			State.Achievements.push_back(R.Def.Id);
			New.push_back(R.Def.Id);
		}
	}
	return New;
}

// ---------------------------------------------------------------- data-driven achievements

static int CountIf(const std::vector<ResolvedConsequence>& H, bool bMitigated)
{
	int N = 0;
	for (const ResolvedConsequence& R : H) N += bMitigated ? (R.bMitigated ? 1 : 0) : (R.bOccurred ? 0 : 1);
	return N;
}

bool IsAchievementMet(const GameState& State, const AchievementDef& Def, const MissionCatalogue& Catalogue)
{
	const CampaignState& Camp = State.Campaign;
	switch (Def.Condition)
	{
	case EAchievementCondition::Code:
		for (const AchievementRule& R : Rules())
		{
			if (R.Def.Id == Def.Id) return R.Condition(State);
		}
		return false;
	case EAchievementCondition::PassMission: return State.Career.HasPassed(Def.Target);
	case EAchievementCondition::ReachLevel: return State.Career.Level >= Def.Value;
	case EAchievementCondition::MissionsPassed: return static_cast<double>(State.Career.PassedMissions.size()) >= Def.Value;
	case EAchievementCondition::CrisesPassed:
	{
		int N = 0;
		for (const std::string& Id : State.Career.PassedMissions)
		{
			const MissionDef* M = Catalogue.Find(Id);
			N += M && M->Kind == EMissionKind::Crisis ? 1 : 0;
		}
		return N >= Def.Value;
	}
	case EAchievementCondition::MissionScoreAtLeast:
		if (Def.Target == m001::MissionId) return State.Mission001.bSubmitted && State.Mission001.Result.OverallScore >= Def.Value;
		for (const MissionRecord& R : Camp.Records)
		{
			if (R.MissionId == Def.Target && R.Score >= Def.Value) return true;
		}
		return false;
	case EAchievementCondition::AnyScoreAtLeast:
		for (const MissionRecord& R : Camp.Records)
		{
			if (R.Score >= Def.Value) return true;
		}
		return false;
	case EAchievementCondition::ScoreAtLeastCount:
	{
		std::set<std::string> Ids;
		for (const MissionRecord& R : Camp.Records)
		{
			if (R.bPassed && R.Score >= Def.Value) Ids.insert(R.MissionId);
		}
		return static_cast<int>(Ids.size()) >= Def.Count;
	}
	case EAchievementCondition::Flag: return State.Org.HasFlag(Def.Target);
	case EAchievementCondition::MetricAtLeast:
	case EAchievementCondition::MetricBelow:
	{
		EOrgMetric M;
		if (!MetricFromName(Def.Target, M)) return false;
		const double V = State.Org.Get(M);
		return Def.Condition == EAchievementCondition::MetricAtLeast ? V >= Def.Value : V < Def.Value;
	}
	case EAchievementCondition::IncidentsAvoided: return CountIf(State.Org.History, false) >= Def.Value;
	case EAchievementCondition::IncidentsMitigated: return CountIf(State.Org.History, true) >= Def.Value;
	case EAchievementCondition::CompetencyAtLeast:
	{
		ECompetency C;
		return CompetencyFromName(Def.Target, C) && State.Career.Get(C).Assessments > 0 && State.Career.Get(C).Score >= Def.Value;
	}
	case EAchievementCondition::AllCompetenciesAtLeast:
		for (int i = 0; i < CompetencyCount; ++i)
		{
			const CompetencyRecord& R = State.Career.Competencies[static_cast<size_t>(i)];
			if (R.Assessments == 0 || R.Score < Def.Value) return false;
		}
		return true;
	case EAchievementCondition::LearningEntries: return static_cast<double>(State.LearningHistory.size()) >= Def.Value;
	case EAchievementCondition::RetryPassed:
	{
		std::set<std::string> Failed, Retried;
		for (const MissionRecord& R : Camp.Records)
		{
			if (!R.bPassed) Failed.insert(R.MissionId);
			else if (Failed.count(R.MissionId)) Retried.insert(R.MissionId);
		}
		return static_cast<double>(Retried.size()) >= Def.Value;
	}
	case EAchievementCondition::Escalations:
	{
		int N = 0;
		for (const MissionRecord& R : Camp.Records)
		{
			const MissionDef* M = Catalogue.Find(R.MissionId);
			if (!M) continue;
			for (const MissionAnswer& A : R.Answers)
			{
				for (const MissionQuestionDef& Q : M->Questions)
				{
					if (Q.Id != A.QuestionId) continue;
					for (const MissionOptionDef& O : Q.Options)
					{
						const bool bPicked = std::find(A.OptionIds.begin(), A.OptionIds.end(), O.Id) != A.OptionIds.end();
						if (bPicked && O.bEscalates && O.Cost > LevelDef(R.Level).DecisionAuthority) ++N;
					}
				}
			}
		}
		return N >= Def.Value;
	}
	}
	return false;
}

std::vector<std::string> EvaluateAchievements(GameState& State, const std::vector<AchievementDef>& Defs, const MissionCatalogue& Catalogue)
{
	std::vector<std::string> New;
	for (const AchievementDef& D : Defs)
	{
		const bool bHave = std::find(State.Achievements.begin(), State.Achievements.end(), D.Id) != State.Achievements.end();
		if (!bHave && IsAchievementMet(State, D, Catalogue))
		{
			State.Achievements.push_back(D.Id);
			New.push_back(D.Id);
		}
	}
	return New;
}

std::vector<std::string> ValidateAchievements(const std::vector<AchievementDef>& Defs, const MissionCatalogue& Catalogue)
{
	std::vector<std::string> Out;
	std::set<std::string> Ids;
	for (const AchievementDef& D : Defs)
	{
		const std::string P = (D.Id.empty() ? std::string("(no id)") : D.Id) + ": ";
		if (D.Id.empty() || !Ids.insert(D.Id).second) Out.push_back(P + "id missing or duplicated");
		if (D.Title.empty() || D.Description.empty()) Out.push_back(P + "needs a title and description");
		switch (D.Condition)
		{
		case EAchievementCondition::Code:
		{
			const bool bKnown = std::any_of(Rules().begin(), Rules().end(), [&](const AchievementRule& R) { return R.Def.Id == D.Id; });
			if (!bKnown) Out.push_back(P + "no built-in rule with this id");
			break;
		}
		case EAchievementCondition::PassMission:
		case EAchievementCondition::MissionScoreAtLeast:
			if (D.Target != m001::MissionId && !Catalogue.Find(D.Target)) Out.push_back(P + "unknown mission '" + D.Target + "'");
			break;
		case EAchievementCondition::MetricAtLeast:
		case EAchievementCondition::MetricBelow:
		{
			EOrgMetric M;
			if (!MetricFromName(D.Target, M)) Out.push_back(P + "unknown metric '" + D.Target + "'");
			break;
		}
		case EAchievementCondition::CompetencyAtLeast:
		{
			ECompetency C;
			if (!CompetencyFromName(D.Target, C)) Out.push_back(P + "unknown competency '" + D.Target + "'");
			break;
		}
		case EAchievementCondition::Flag:
			if (D.Target.empty()) Out.push_back(P + "missing flag");
			break;
		case EAchievementCondition::ReachLevel:
			if (D.Value < 2 || D.Value > static_cast<double>(CareerLadder().size())) Out.push_back(P + "level outside 2..10");
			break;
		case EAchievementCondition::ScoreAtLeastCount:
			if (D.Count < 1) Out.push_back(P + "count must be at least 1");
			break;
		default:
			break;
		}
		if (D.Value < 0.0) Out.push_back(P + "negative value");
	}
	return Out;
}
} // namespace rb
