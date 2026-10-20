#include "RbGameState.h"

#include <algorithm>
#include <functional>

namespace rb
{
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
} // namespace rb
