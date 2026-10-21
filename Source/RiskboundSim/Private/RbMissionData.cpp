#include "RbMissionData.h"

#include "RbGameState.h"
#include "RbJson.h"

#include <cmath>
#include <set>

namespace rb
{
namespace
{
const char* const CompetencyNames[CompetencyCount] = {"RiskIdentification", "RiskAssessment", "ControlEvaluation", "QuantitativeAnalysis",
	"Communication", "StakeholderManagement", "Judgement", "Leadership", "StrategicThinking", "Governance"};
const char* const MetricNames[OrgMetricCount] = {"Revenue", "Costs", "Cash", "RiskExposure", "ControlEffectiveness", "Reputation",
	"StakeholderTrust", "EmployeeMorale", "Resilience", "StrategicProgress"};

// Reads one JSON object into a struct, recording errors and rejecting keys it was not asked about.
class Reader
{
public:
	Reader(const JsonValue& InObj, const std::string& InWhere, std::vector<std::string>& InErrors)
		: Obj(InObj), Where(InWhere), Errors(InErrors)
	{
		if (!Obj.IsObject()) Error(Obj.Line, "expected an object");
	}

	~Reader()
	{
		if (!Obj.IsObject()) return;
		for (const auto& M : Obj.Members)
		{
			if (!Seen.count(M.first)) Error(M.second.Line, "unknown key \"" + M.first + "\"");
		}
	}

	void Error(int Line, const std::string& Msg) { Errors.push_back(Where + " (line " + std::to_string(Line) + "): " + Msg); }

	const JsonValue* Get(const char* Key, bool bRequired)
	{
		Seen.insert(Key);
		const JsonValue* V = Obj.Find(Key);
		if (!V && bRequired) Error(Obj.Line, std::string("missing \"") + Key + "\"");
		return V;
	}

	void Str(const char* Key, std::string& Out, bool bRequired)
	{
		const JsonValue* V = Get(Key, bRequired);
		if (!V) return;
		if (V->IsString()) Out = V->String;
		else Error(V->Line, std::string("\"") + Key + "\" must be a string");
	}

	// A string, or an array of strings joined as paragraphs.
	void Text(const char* Key, std::string& Out, bool bRequired)
	{
		const JsonValue* V = Get(Key, bRequired);
		if (!V) return;
		if (V->IsString())
		{
			Out = V->String;
			return;
		}
		if (V->IsArray())
		{
			Out.clear();
			for (const JsonValue& P : V->Items)
			{
				if (!P.IsString())
				{
					Error(P.Line, std::string("\"") + Key + "\" paragraphs must be strings");
					continue;
				}
				if (!Out.empty()) Out += "\n\n";
				Out += P.String;
			}
			return;
		}
		Error(V->Line, std::string("\"") + Key + "\" must be a string or an array of strings");
	}

	void Int(const char* Key, int& Out, bool bRequired)
	{
		const JsonValue* V = Get(Key, bRequired);
		if (!V) return;
		if (V->IsNumber() && std::floor(V->Number) == V->Number && std::fabs(V->Number) < 1e9) Out = static_cast<int>(V->Number);
		else Error(V->Line, std::string("\"") + Key + "\" must be a whole number");
	}

	void Dbl(const char* Key, double& Out, bool bRequired)
	{
		const JsonValue* V = Get(Key, bRequired);
		if (!V) return;
		if (V->IsNumber()) Out = V->Number;
		else Error(V->Line, std::string("\"") + Key + "\" must be a number");
	}

	void Bool(const char* Key, bool& Out)
	{
		const JsonValue* V = Get(Key, false);
		if (!V) return;
		if (V->IsBool()) Out = V->Bool;
		else Error(V->Line, std::string("\"") + Key + "\" must be true or false");
	}

	void Strings(const char* Key, std::vector<std::string>& Out)
	{
		const JsonValue* V = Get(Key, false);
		if (!V) return;
		if (!V->IsArray())
		{
			Error(V->Line, std::string("\"") + Key + "\" must be an array of strings");
			return;
		}
		for (const JsonValue& S : V->Items)
		{
			if (S.IsString()) Out.push_back(S.String);
			else Error(S.Line, std::string("\"") + Key + "\" items must be strings");
		}
	}

	// {"Reputation": -2, "Cash": -5000}
	void Effects(const char* Key, std::vector<MetricEffect>& Out)
	{
		const JsonValue* V = Get(Key, false);
		if (!V) return;
		if (!V->IsObject())
		{
			Error(V->Line, std::string("\"") + Key + "\" must be an object of metric: change");
			return;
		}
		for (const auto& M : V->Members)
		{
			EOrgMetric Metric;
			if (!MetricFromName(M.first, Metric)) Error(M.second.Line, "unknown metric \"" + M.first + "\"");
			else if (!M.second.IsNumber()) Error(M.second.Line, "effect on " + M.first + " must be a number");
			else Out.push_back({Metric, M.second.Number});
		}
	}

	const JsonValue& Obj;
	std::string Where;
	std::vector<std::string>& Errors;

private:
	std::set<std::string> Seen;
};

void ReadConsequence(const JsonValue& V, const std::string& Where, ConsequenceDef& C, std::vector<std::string>& Errors)
{
	Reader R(V, Where, Errors);
	R.Str("id", C.Id, true);
	R.Str("description", C.Description, true);
	R.Int("delayWeeks", C.DelayWeeks, true);
	R.Dbl("probability", C.Probability, true);
	R.Effects("effects", C.Effects);
	R.Str("flagIfOccurs", C.FlagIfOccurs, false);
	R.Str("mitigatedBy", C.MitigatingFlag, false);
	R.Dbl("mitigatedProbabilityFactor", C.MitigatedProbabilityFactor, false);
	R.Dbl("mitigatedSeverityFactor", C.MitigatedSeverityFactor, false);
}

void ReadOption(const JsonValue& V, const std::string& Where, MissionOptionDef& O, std::vector<std::string>& Errors)
{
	Reader R(V, Where, Errors);
	R.Str("id", O.Id, true);
	R.Str("label", O.Label, true);
	R.Text("feedback", O.Feedback, false);
	R.Int("score", O.Score, false);
	R.Bool("correct", O.bCorrect);
	R.Dbl("cost", O.Cost, false);
	R.Bool("escalates", O.bEscalates);
	R.Effects("effects", O.Effects);
	R.Strings("setsFlags", O.SetsFlags);
	if (const JsonValue* S = R.Get("schedules", false))
	{
		if (!S->IsArray()) R.Error(S->Line, "\"schedules\" must be an array");
		else
		{
			for (const JsonValue& C : S->Items)
			{
				ConsequenceDef Def;
				ReadConsequence(C, Where + " schedule", Def, Errors);
				O.Schedules.push_back(Def);
			}
		}
	}
}

void ReadQuestion(const JsonValue& V, const std::string& Where, MissionQuestionDef& Q, std::vector<std::string>& Errors)
{
	Reader R(V, Where, Errors);
	R.Str("id", Q.Id, true);
	R.Text("prompt", Q.Prompt, true);
	std::string Kind = "single";
	R.Str("kind", Kind, false);
	if (Kind == "single") Q.Kind = EQuestionKind::Single;
	else if (Kind == "multi") Q.Kind = EQuestionKind::Multi;
	else R.Error(V.Line, "kind must be \"single\" or \"multi\"");
	std::string Comp;
	R.Str("competency", Comp, true);
	if (!Comp.empty() && !CompetencyFromName(Comp, Q.Competency)) R.Error(V.Line, "unknown competency \"" + Comp + "\"");
	R.Int("weight", Q.Weight, true);
	if (const JsonValue* Opts = R.Get("options", true))
	{
		if (!Opts->IsArray()) R.Error(Opts->Line, "\"options\" must be an array");
		else
		{
			for (const JsonValue& O : Opts->Items)
			{
				MissionOptionDef Def;
				ReadOption(O, Where + " option", Def, Errors);
				Q.Options.push_back(Def);
			}
		}
	}
}

void ReadContext(const JsonValue& V, const std::string& Where, ContextNoteDef& C, std::vector<std::string>& Errors)
{
	Reader R(V, Where, Errors);
	R.Text("text", C.Text, true);
	std::string IfFlag, IfNotFlag, Metric;
	R.Str("ifFlag", IfFlag, false);
	R.Str("ifNotFlag", IfNotFlag, false);
	R.Str("ifMetric", Metric, false);
	const JsonValue* Below = R.Get("below", false);
	const JsonValue* AtLeast = R.Get("atLeast", false);
	if (!IfFlag.empty() && !IfNotFlag.empty()) R.Error(V.Line, "use either ifFlag or ifNotFlag");
	C.Flag = !IfFlag.empty() ? IfFlag : IfNotFlag;
	C.bWhenAbsent = !IfNotFlag.empty();
	if (!Metric.empty())
	{
		C.bUseMetric = true;
		if (!MetricFromName(Metric, C.Metric)) R.Error(V.Line, "unknown metric \"" + Metric + "\"");
		if ((Below != nullptr) == (AtLeast != nullptr)) R.Error(V.Line, "ifMetric needs exactly one of below / atLeast");
		const JsonValue* T = Below ? Below : AtLeast;
		if (T && !T->IsNumber()) R.Error(T->Line, "threshold must be a number");
		else if (T) C.Threshold = T->Number;
		C.bBelow = Below != nullptr;
	}
	else if (Below || AtLeast)
	{
		R.Error(V.Line, "below / atLeast need ifMetric");
	}
}
} // namespace

bool CompetencyFromName(const std::string& Name, ECompetency& Out)
{
	for (int i = 0; i < CompetencyCount; ++i)
	{
		if (Name == CompetencyNames[i])
		{
			Out = static_cast<ECompetency>(i);
			return true;
		}
	}
	return false;
}

bool MetricFromName(const std::string& Name, EOrgMetric& Out)
{
	for (int i = 0; i < OrgMetricCount; ++i)
	{
		if (Name == MetricNames[i])
		{
			Out = static_cast<EOrgMetric>(i);
			return true;
		}
	}
	return false;
}

const char* MetricName(EOrgMetric M)
{
	const int i = static_cast<int>(M);
	return i >= 0 && i < OrgMetricCount ? MetricNames[i] : "?";
}

const char* CompetencyName(ECompetency C)
{
	const int i = static_cast<int>(C);
	return i >= 0 && i < CompetencyCount ? CompetencyNames[i] : "?";
}

bool MissionFromJson(const std::string& Text, const std::string& SourceName, MissionDef& Out, std::vector<std::string>& OutErrors)
{
	const size_t ErrorsBefore = OutErrors.size();
	JsonValue Doc;
	std::string JsonError;
	if (!ParseJson(Text, Doc, JsonError))
	{
		OutErrors.push_back(SourceName + ": " + JsonError);
		return false;
	}
	MissionDef M;
	{
		Reader R(Doc, SourceName, OutErrors);
		int Schema = 0;
		R.Int("schema", Schema, true);
		if (Schema != MissionSchemaVersion) R.Error(Doc.Line, "schema must be " + std::to_string(MissionSchemaVersion));
		R.Str("id", M.Id, true);
		R.Str("title", M.Title, true);
		std::string Kind = "mission";
		R.Str("kind", Kind, false);
		if (Kind == "mission") M.Kind = EMissionKind::Mission;
		else if (Kind == "crisis") M.Kind = EMissionKind::Crisis;
		else if (Kind == "capstone") M.Kind = EMissionKind::Capstone;
		else R.Error(Doc.Line, "kind must be mission, crisis or capstone");
		R.Str("department", M.Department, true);
		R.Text("briefing", M.Briefing, true);
		R.Int("minLevel", M.MinLevel, true);
		R.Strings("requiresPassed", M.RequiresPassed);
		R.Strings("requiresFlags", M.RequiresFlags);
		R.Strings("excludedByFlags", M.ExcludedByFlags);
		R.Int("passMark", M.PassMark, false);
		R.Int("xp", M.Xp, false);
		R.Int("weeks", M.WeeksToComplete, false);
		R.Text("lesson", M.Lesson, false);
		if (const JsonValue* C = R.Get("context", false))
		{
			if (!C->IsArray()) R.Error(C->Line, "\"context\" must be an array");
			else
			{
				for (const JsonValue& N : C->Items)
				{
					ContextNoteDef Def;
					ReadContext(N, SourceName + " context", Def, OutErrors);
					M.Context.push_back(Def);
				}
			}
		}
		if (const JsonValue* E = R.Get("evidence", true))
		{
			if (!E->IsArray()) R.Error(E->Line, "\"evidence\" must be an array");
			else
			{
				for (const JsonValue& Item : E->Items)
				{
					MissionEvidenceDef Def;
					Reader ER(Item, SourceName + " evidence", OutErrors);
					ER.Str("id", Def.Id, true);
					ER.Str("title", Def.Title, true);
					ER.Text("body", Def.Body, true);
					ER.Bool("required", Def.bRequired);
					M.Evidence.push_back(Def);
				}
			}
		}
		if (const JsonValue* Q = R.Get("questions", true))
		{
			if (!Q->IsArray()) R.Error(Q->Line, "\"questions\" must be an array");
			else
			{
				for (const JsonValue& Item : Q->Items)
				{
					MissionQuestionDef Def;
					ReadQuestion(Item, SourceName + " question", Def, OutErrors);
					M.Questions.push_back(Def);
				}
			}
		}
	} // Reader destructor reports unknown top-level keys here.
	Out = std::move(M);
	return OutErrors.size() == ErrorsBefore;
}

bool LoadMissionCatalogue(const std::vector<std::pair<std::string, std::string>>& Files, MissionCatalogue& Out, std::vector<std::string>& OutErrors)
{
	const size_t ErrorsBefore = OutErrors.size();
	Out = MissionCatalogue();
	for (const auto& F : Files)
	{
		MissionDef M;
		if (!MissionFromJson(F.second, F.first, M, OutErrors)) continue;
		std::string Stem = F.first;
		const size_t Slash = Stem.find_last_of("/\\");
		if (Slash != std::string::npos) Stem = Stem.substr(Slash + 1);
		const size_t Dot = Stem.rfind('.');
		if (Dot != std::string::npos) Stem = Stem.substr(0, Dot);
		if (Stem != M.Id) OutErrors.push_back(F.first + ": file name must match the mission id \"" + M.Id + "\"");
		Out.Missions.push_back(std::move(M));
	}
	for (const std::string& E : ValidateCatalogue(Out)) OutErrors.push_back(E);
	return OutErrors.size() == ErrorsBefore;
}

bool AchievementsFromJson(const std::string& Text, const std::string& SourceName, std::vector<AchievementDef>& Out, std::vector<std::string>& OutErrors)
{
	const size_t ErrorsBefore = OutErrors.size();
	JsonValue Doc;
	std::string JsonError;
	if (!ParseJson(Text, Doc, JsonError))
	{
		OutErrors.push_back(SourceName + ": " + JsonError);
		return false;
	}
	struct FKind
	{
		const char* Key;
		EAchievementCondition Condition;
		bool bString;     // the key's value is a string target, not a number
		const char* Extra; // second key: target name (for numeric conditions) or count
	};
	static const FKind Kinds[] = {
		{"code", EAchievementCondition::Code, false, nullptr},
		{"passMission", EAchievementCondition::PassMission, true, nullptr},
		{"reachLevel", EAchievementCondition::ReachLevel, false, nullptr},
		{"missionsPassed", EAchievementCondition::MissionsPassed, false, nullptr},
		{"crisesPassed", EAchievementCondition::CrisesPassed, false, nullptr},
		{"missionScoreAtLeast", EAchievementCondition::MissionScoreAtLeast, false, "mission"},
		{"anyScoreAtLeast", EAchievementCondition::AnyScoreAtLeast, false, nullptr},
		{"scoreAtLeast", EAchievementCondition::ScoreAtLeastCount, false, "count"},
		{"flag", EAchievementCondition::Flag, true, nullptr},
		{"metricAtLeast", EAchievementCondition::MetricAtLeast, false, "metric"},
		{"metricBelow", EAchievementCondition::MetricBelow, false, "metric"},
		{"incidentsAvoided", EAchievementCondition::IncidentsAvoided, false, nullptr},
		{"incidentsMitigated", EAchievementCondition::IncidentsMitigated, false, nullptr},
		{"competencyAtLeast", EAchievementCondition::CompetencyAtLeast, false, "competency"},
		{"allCompetenciesAtLeast", EAchievementCondition::AllCompetenciesAtLeast, false, nullptr},
		{"learningEntries", EAchievementCondition::LearningEntries, false, nullptr},
		{"retryPassed", EAchievementCondition::RetryPassed, false, nullptr},
		{"escalations", EAchievementCondition::Escalations, false, nullptr},
	};
	std::vector<AchievementDef> Defs;
	{
		Reader R(Doc, SourceName, OutErrors);
		int Schema = 0;
		R.Int("schema", Schema, true);
		if (Schema != MissionSchemaVersion) R.Error(Doc.Line, "schema must be " + std::to_string(MissionSchemaVersion));
		const JsonValue* List = R.Get("achievements", true);
		if (List && !List->IsArray()) R.Error(List->Line, "\"achievements\" must be an array");
		for (const JsonValue& Item : List && List->IsArray() ? List->Items : std::vector<JsonValue>())
		{
			AchievementDef D;
			Reader AR(Item, SourceName + " achievement", OutErrors);
			AR.Str("id", D.Id, true);
			AR.Str("title", D.Title, true);
			AR.Str("description", D.Description, true);
			const JsonValue* When = AR.Get("when", true);
			if (When)
			{
				Reader WR(*When, SourceName + " achievement '" + D.Id + "' when", OutErrors);
				int Found = 0;
				for (const FKind& K : Kinds)
				{
					const JsonValue* V = WR.Get(K.Key, false);
					if (!V) continue;
					++Found;
					D.Condition = K.Condition;
					if (K.Condition == EAchievementCondition::Code)
					{
						if (!V->IsBool() || !V->Bool) WR.Error(V->Line, "\"code\" must be true");
					}
					else if (K.bString)
					{
						if (V->IsString()) D.Target = V->String;
						else WR.Error(V->Line, std::string("\"") + K.Key + "\" must be a string");
					}
					else if (V->IsNumber())
					{
						D.Value = V->Number;
					}
					else
					{
						WR.Error(V->Line, std::string("\"") + K.Key + "\" must be a number");
					}
					if (K.Extra)
					{
						if (std::string(K.Extra) == "count") WR.Int("count", D.Count, true);
						else WR.Str(K.Extra, D.Target, true);
					}
				}
				if (Found != 1) WR.Error(When->Line, "needs exactly one condition");
			}
			Defs.push_back(D);
		}
	}
	Out = std::move(Defs);
	return OutErrors.size() == ErrorsBefore;
}
} // namespace rb
