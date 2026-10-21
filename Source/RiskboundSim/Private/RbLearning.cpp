#include "RbLearning.h"

#include "RbGameState.h"
#include "RbJsonReader.h"
#include "RbQuant.h"
#include "RbRng.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <sstream>

namespace rb
{
namespace
{
using json::Reader;

const std::vector<JsonValue>& Items(Reader& R, const char* Key, bool bRequired)
{
	static const std::vector<JsonValue> None;
	const JsonValue* V = R.Get(Key, bRequired);
	if (!V) return None;
	if (!V->IsArray())
	{
		R.Error(V->Line, std::string("\"") + Key + "\" must be an array");
		return None;
	}
	return V->Items;
}

bool Parse(const std::string& Text, const std::string& SourceName, JsonValue& Doc, std::vector<std::string>& OutErrors)
{
	std::string E;
	if (ParseJson(Text, Doc, E)) return true;
	OutErrors.push_back(SourceName + ": " + E);
	return false;
}
} // namespace

// ---------------------------------------------------------------- glossary

bool GlossaryFromJson(const std::string& Text, const std::string& SourceName, std::vector<GlossaryTerm>& Out, std::vector<std::string>& OutErrors)
{
	const size_t Before = OutErrors.size();
	JsonValue Doc;
	if (!Parse(Text, SourceName, Doc, OutErrors)) return false;
	std::vector<GlossaryTerm> Terms;
	{
		Reader R(Doc, SourceName, OutErrors);
		int Schema = 0;
		R.Int("schema", Schema, true);
		if (Schema != 1) R.Error(Doc.Line, "schema must be 1");
		for (const JsonValue& V : Items(R, "terms", true))
		{
			GlossaryTerm T;
			Reader TR(V, SourceName + " term", OutErrors);
			TR.Str("id", T.Id, true);
			TR.Str("term", T.Term, true);
			TR.Str("category", T.Category, true);
			TR.Text("definition", T.Definition, true);
			TR.Text("example", T.Example, false);
			TR.Strings("missions", T.Missions);
			Terms.push_back(T);
		}
	}
	Out = std::move(Terms);
	return OutErrors.size() == Before;
}

std::vector<std::string> ValidateGlossary(const std::vector<GlossaryTerm>& Terms, const MissionCatalogue* Missions)
{
	std::vector<std::string> Out;
	std::set<std::string> Ids;
	for (const GlossaryTerm& T : Terms)
	{
		const std::string P = "Glossary.json '" + T.Id + "'";
		if (T.Id.empty() || !Ids.insert(T.Id).second) Out.push_back(P + ": id missing or duplicated");
		if (T.Definition.size() < 20) Out.push_back(P + ": definition too short");
		for (const std::string& M : T.Missions)
		{
			if (Missions && M != "M001" && !Missions->Find(M)) Out.push_back(P + ": unknown mission '" + M + "'");
		}
	}
	return Out;
}

std::vector<const GlossaryTerm*> TermsForMission(const std::vector<GlossaryTerm>& Terms, const std::string& MissionId)
{
	std::vector<const GlossaryTerm*> Out;
	for (const GlossaryTerm& T : Terms)
	{
		if (std::find(T.Missions.begin(), T.Missions.end(), MissionId) != T.Missions.end()) Out.push_back(&T);
	}
	return Out;
}

// ---------------------------------------------------------------- endings

bool EndingsFromJson(const std::string& Text, const std::string& SourceName, std::vector<EndingDef>& Out, std::vector<std::string>& OutErrors)
{
	const size_t Before = OutErrors.size();
	JsonValue Doc;
	if (!Parse(Text, SourceName, Doc, OutErrors)) return false;
	std::vector<EndingDef> Endings;
	{
		Reader R(Doc, SourceName, OutErrors);
		int Schema = 0;
		R.Int("schema", Schema, true);
		if (Schema != 1) R.Error(Doc.Line, "schema must be 1");
		for (const JsonValue& V : Items(R, "endings", true))
		{
			EndingDef E;
			Reader ER(V, SourceName + " ending", OutErrors);
			ER.Str("id", E.Id, true);
			ER.Str("title", E.Title, true);
			ER.Text("epilogue", E.Epilogue, true);
			if (const JsonValue* W = ER.Get("when", false))
			{
				Reader WR(*W, SourceName + " ending '" + E.Id + "' when", OutErrors);
				WR.Strings("flags", E.When.Flags);
				WR.Strings("notFlags", E.When.NotFlags);
				for (const JsonValue& M : Items(WR, "metrics", false))
				{
					Reader MR(M, SourceName + " ending metric", OutErrors);
					WorldCondition::MetricTest T;
					std::string Name;
					MR.Str("metric", Name, true);
					if (!Name.empty() && !MetricFromName(Name, T.Metric)) MR.Error(M.Line, "unknown metric \"" + Name + "\"");
					const JsonValue* Below = MR.Get("below", false);
					const JsonValue* AtLeast = MR.Get("atLeast", false);
					if ((Below != nullptr) == (AtLeast != nullptr)) MR.Error(M.Line, "needs exactly one of below / atLeast");
					const JsonValue* Th = Below ? Below : AtLeast;
					if (Th && Th->IsNumber()) T.Threshold = Th->Number;
					T.bBelow = Below != nullptr;
					E.When.Metrics.push_back(T);
				}
			}
			Endings.push_back(E);
		}
	}
	Out = std::move(Endings);
	return OutErrors.size() == Before;
}

std::vector<std::string> ValidateEndings(const std::vector<EndingDef>& Endings)
{
	std::vector<std::string> Out;
	std::set<std::string> Ids;
	for (const EndingDef& E : Endings)
	{
		if (E.Id.empty() || !Ids.insert(E.Id).second) Out.push_back("Endings.json '" + E.Id + "': id missing or duplicated");
	}
	const bool bDefault = !Endings.empty() && Endings.back().When.Flags.empty() && Endings.back().When.Metrics.empty() && Endings.back().When.NotFlags.empty();
	if (!bDefault) Out.push_back("Endings.json: the last ending must have no condition (the default)");
	return Out;
}

const EndingDef* ChooseEnding(const GameState& State, const std::vector<EndingDef>& Endings)
{
	for (const EndingDef& E : Endings)
	{
		if (ConditionHolds(State, E.When)) return &E;
	}
	return nullptr;
}

// ---------------------------------------------------------------- review and certificate

PerformanceReview MakeReview(const GameState& State, const PeopleCatalogue& People)
{
	PerformanceReview R;
	R.Level = State.Career.Level;
	R.Title = LevelDef(State.Career.Level).Title;
	R.MissionsPassed = static_cast<int>(State.Career.PassedMissions.size());
	int Sum = 0;
	for (const MissionRecord& Rec : State.Campaign.Records) Sum += Rec.Score;
	R.AverageScore = State.Campaign.Records.empty() ? 0 : Sum / static_cast<int>(State.Campaign.Records.size());
	for (const ResolvedConsequence& H : State.Org.History)
	{
		R.IncidentsOccurred += H.bOccurred ? 1 : 0;
		R.IncidentsAvoided += H.bOccurred ? 0 : 1;
	}
	for (int i = 0; i < CompetencyCount; ++i)
	{
		const CompetencyRecord& C = State.Career.Competencies[static_cast<size_t>(i)];
		if (C.Assessments == 0) continue;
		if (C.Score >= 75) R.Strengths.push_back(ToString(static_cast<ECompetency>(i)));
		else if (C.Score < 60) R.Growth.push_back(ToString(static_cast<ECompetency>(i)));
	}
	int Best = RelationMin - 1, Worst = RelationMax + 1;
	for (const Relation& Rel : State.World.Relations)
	{
		const CharacterDef* P = People.Find(Rel.CharacterId);
		if (!P || P->bPersonal) continue;
		if (Rel.Value > Best)
		{
			Best = Rel.Value;
			R.Ally = P->Name;
		}
		if (Rel.Value < Worst)
		{
			Worst = Rel.Value;
			R.Strained = P->Name;
		}
	}
	if (Worst > RelationWary) R.Strained.clear();
	std::ostringstream S;
	S << "As " << R.Title << " you have passed " << R.MissionsPassed << " cases with an average score of " << R.AverageScore << ". ";
	if (!R.Strengths.empty()) S << "Your work is strongest in " << R.Strengths.front() << (R.Strengths.size() > 1 ? " and " + R.Strengths[1] : std::string()) << ". ";
	if (!R.Growth.empty()) S << "The next step is " << R.Growth.front() << ". ";
	S << R.IncidentsOccurred << " incidents happened on your watch and " << R.IncidentsAvoided << " were avoided. ";
	if (!R.Ally.empty()) S << R.Ally << " is your strongest supporter";
	if (!R.Strained.empty()) S << ", while things are strained with " << R.Strained;
	if (!R.Ally.empty()) S << ".";
	R.Summary = S.str();
	return R;
}

std::string CertificateMarkdown(const GameState& State, const PerformanceReview& Review, const EndingDef* Ending, const std::string& Date)
{
	std::ostringstream S;
	S << "# Riskbound career certificate\n\n";
	S << "**" << State.ProfileName << "**, " << Review.Title << " (level " << Review.Level << " of 10), Valtren Coldline Logistics\n\n";
	if (Ending) S << "Ending: **" << Ending->Title << "**\n\n";
	S << "| | |\n|---|---|\n";
	S << "| Cases passed | " << Review.MissionsPassed << " |\n";
	S << "| Average score | " << Review.AverageScore << " |\n";
	S << "| Incidents (occurred / avoided) | " << Review.IncidentsOccurred << " / " << Review.IncidentsAvoided << " |\n";
	S << "| Weeks in the role | " << State.Org.Week << " |\n";
	S << "| Achievements | " << State.Achievements.size() << " |\n";
	S << "| Difficulty | " << ToString(State.Difficulty) << " |\n\n";
	S << "## Competencies\n\n";
	for (int i = 0; i < CompetencyCount; ++i)
	{
		const CompetencyRecord& C = State.Career.Competencies[static_cast<size_t>(i)];
		if (C.Assessments > 0) S << "- " << ToString(static_cast<ECompetency>(i)) << ": " << C.Score << "\n";
	}
	S << "\n" << Review.Summary << "\n\n";
	S << "Issued " << Date << " by Riskbound, a risk-management career simulation. A record of play, not a professional qualification.\n";
	return S.str();
}

// ---------------------------------------------------------------- what if

std::vector<WhatIfQuestion> WhatIf(const MissionDef& M, const MissionRecord& Record)
{
	std::vector<WhatIfQuestion> Out;
	for (const MissionQuestionDef& Q : M.Questions)
	{
		WhatIfQuestion W;
		W.Prompt = Q.Prompt;
		W.Kind = ToString(Q.Kind);
		W.Explanation = Q.Explanation;
		const MissionAnswer* A = nullptr;
		for (const MissionAnswer& X : Record.Answers)
		{
			if (X.QuestionId == Q.Id) A = &X;
		}
		if (Q.Kind == EQuestionKind::Numeric || Q.Kind == EQuestionKind::Rating)
		{
			if (A)
			{
				for (const std::string& S : A->OptionIds) W.YourAnswer += (W.YourAnswer.empty() ? "" : " / ") + S;
			}
			Out.push_back(W);
			continue;
		}
		for (const MissionOptionDef& O : Q.Options)
		{
			WhatIfOption WO;
			WO.Label = O.Label;
			WO.Score = Q.Kind == EQuestionKind::Multi ? (O.bCorrect ? 100 : 0) : O.Score;
			WO.bChosen = A && std::find(A->OptionIds.begin(), A->OptionIds.end(), O.Id) != A->OptionIds.end();
			for (const ConsequenceDef& C : O.Schedules)
			{
				char Buf[64];
				std::snprintf(Buf, sizeof(Buf), " (%d%% within %d weeks)", static_cast<int>(C.Probability * 100.0 + 0.5), C.DelayWeeks);
				WO.Consequences.push_back(C.Description + Buf);
			}
			if (Q.Kind == EQuestionKind::Single && O.Score < FalloutScoreThreshold && O.Schedules.empty())
			{
				char Buf[96];
				std::snprintf(Buf, sizeof(Buf), "A weak call: about %d%% chance of a later incident",
					static_cast<int>(100.0 * FalloutMaxProbability * (FalloutScoreThreshold - O.Score) / FalloutScoreThreshold + 0.5));
				WO.Consequences.push_back(Buf);
			}
			W.Options.push_back(WO);
		}
		Out.push_back(W);
	}
	return Out;
}

// ---------------------------------------------------------------- challenges

const char* ToString(EGameMode M)
{
	switch (M)
	{
	case EGameMode::Career: return "career";
	case EGameMode::Survival: return "survival";
	case EGameMode::Daily: return "daily";
	}
	return "?";
}

void StartChallenge(GameState& State, const MissionCatalogue& C, EGameMode Mode, const std::string& Date)
{
	ChallengeState& Ch = State.Challenge;
	Ch = ChallengeState();
	Ch.Mode = Mode;
	Ch.Date = Date;
	if (Mode == EGameMode::Career) return;
	std::vector<std::string> Pool;
	for (const MissionDef& M : C.Missions)
	{
		const bool bCaseStudy = M.Id >= "M031" && M.Id <= "M036" && M.Id.size() == 4;
		if (M.Kind == EMissionKind::Crisis || (Mode == EGameMode::Daily && bCaseStudy)) Pool.push_back(M.Id);
	}
	std::sort(Pool.begin(), Pool.end());
	uint64_t Seed = State.Seed;
	if (Mode == EGameMode::Daily)
	{
		Seed = 1469598103934665603ULL;
		for (unsigned char Ch2 : Date)
		{
			Seed ^= Ch2;
			Seed *= 1099511628211ULL;
		}
	}
	Rng R(Seed);
	for (int i = static_cast<int>(Pool.size()) - 1; i > 0; --i) std::swap(Pool[static_cast<size_t>(i)], Pool[static_cast<size_t>(R.UniformInt(0, i))]);
	if (Mode == EGameMode::Daily && Pool.size() > 3) Pool.resize(3);
	Ch.Sequence = Pool;
	// Challenges are played at the top of the ladder: full approval authority, every tool.
	State.Career.Level = static_cast<int>(CareerLadder().size()) - 1;
}

void StartCustomChallenge(GameState& State, const std::vector<std::string>& Sequence)
{
	State.Challenge = ChallengeState();
	State.Challenge.Mode = EGameMode::Daily;
	State.Challenge.Sequence = Sequence;
	State.Career.Level = static_cast<int>(CareerLadder().size()) - 1;
}

std::string CurrentChallengeMission(const GameState& State)
{
	const ChallengeState& Ch = State.Challenge;
	if (Ch.Mode == EGameMode::Career || Ch.bOver || Ch.Index >= static_cast<int>(Ch.Sequence.size())) return std::string();
	return Ch.Sequence[static_cast<size_t>(Ch.Index)];
}

bool AdvanceChallenge(GameState& State, const MissionDebrief& D)
{
	ChallengeState& Ch = State.Challenge;
	if (Ch.Mode == EGameMode::Career || Ch.bOver || D.MissionId != CurrentChallengeMission(State)) return false;
	if (Ch.Mode == EGameMode::Survival)
	{
		if (D.bPassed) ++Ch.Points;
		else --Ch.Lives;
	}
	else
	{
		Ch.Points += D.Score;
	}
	++Ch.Index;
	Ch.bOver = Ch.Lives <= 0 || Ch.Index >= static_cast<int>(Ch.Sequence.size()) || State.World.Burnouts > 0;
	return Ch.bOver;
}

bool ChallengeUnlocks(const GameState& State, const std::string& MissionId)
{
	return !MissionId.empty() && MissionId == CurrentChallengeMission(State);
}

// ---------------------------------------------------------------- leaderboard

std::string SerializeLeaderboard(const std::vector<LeaderboardEntry>& Entries)
{
	auto Clean = [](std::string S)
	{
		std::replace(S.begin(), S.end(), '\t', ' ');
		std::replace(S.begin(), S.end(), '\n', ' ');
		return S;
	};
	std::ostringstream S;
	S << "RISKBOUND-LEADERBOARD 1\n";
	for (const LeaderboardEntry& E : Entries)
	{
		S << Clean(E.Mode) << '\t' << Clean(E.Profile) << '\t' << Clean(E.Detail) << '\t' << E.Points << '\t' << E.Weeks << '\t' << Clean(E.When) << '\n';
	}
	return S.str();
}

std::vector<LeaderboardEntry> DeserializeLeaderboard(const std::string& Text)
{
	std::vector<LeaderboardEntry> Out;
	std::istringstream In(Text);
	std::string Line;
	if (!std::getline(In, Line) || Line.rfind("RISKBOUND-LEADERBOARD", 0) != 0) return Out;
	while (std::getline(In, Line))
	{
		std::vector<std::string> F;
		std::string Cell;
		std::istringstream LS(Line);
		while (std::getline(LS, Cell, '\t')) F.push_back(Cell);
		if (F.size() != 6) continue;
		LeaderboardEntry E;
		E.Mode = F[0];
		E.Profile = F[1];
		E.Detail = F[2];
		char* End = nullptr;
		E.Points = static_cast<int>(std::strtol(F[3].c_str(), &End, 10));
		if (*End != '\0') continue;
		E.Weeks = static_cast<int>(std::strtol(F[4].c_str(), &End, 10));
		if (*End != '\0') continue;
		E.When = F[5];
		Out.push_back(E);
	}
	return Out;
}

void AddToLeaderboard(std::vector<LeaderboardEntry>& Entries, const LeaderboardEntry& E)
{
	Entries.push_back(E);
	std::stable_sort(Entries.begin(), Entries.end(), [](const LeaderboardEntry& A, const LeaderboardEntry& B)
	{
		if (A.Mode != B.Mode) return A.Mode < B.Mode;
		return A.Points > B.Points;
	});
	std::vector<LeaderboardEntry> Kept;
	std::string Mode;
	int N = 0;
	for (const LeaderboardEntry& X : Entries)
	{
		if (X.Mode != Mode)
		{
			Mode = X.Mode;
			N = 0;
		}
		if (N++ < 20) Kept.push_back(X);
	}
	Entries = std::move(Kept);
}
} // namespace rb
