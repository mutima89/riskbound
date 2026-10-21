// Content files for the living world: Data/People.json, Data/Story.json, Data/Events.json.
// Format: Docs/WorldData.md. Strict like mission files (RbJsonReader.h).
#include "RbWorld.h"

#include "RbJsonReader.h"

#include <cctype>
#include <cmath>
#include <set>

namespace rb
{
namespace
{
using json::Reader;

const char* const CategoryNames[] = {"story", "incident", "regulator", "personal", "politics", "team", "news"};

bool ParseDoc(const std::string& Text, const std::string& SourceName, JsonValue& Doc, std::vector<std::string>& OutErrors)
{
	std::string JsonError;
	if (!ParseJson(Text, Doc, JsonError))
	{
		OutErrors.push_back(SourceName + ": " + JsonError);
		return false;
	}
	return true;
}

void Schema(Reader& R)
{
	int S = 0;
	R.Int("schema", S, true);
	if (S != 1) R.Error(R.Obj.Line, "schema must be 1");
}

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

void ReadCondition(const JsonValue& V, const std::string& Where, WorldCondition& C, std::vector<std::string>& Errors)
{
	Reader R(V, Where, Errors);
	R.Strings("flags", C.Flags);
	R.Strings("notFlags", C.NotFlags);
	R.Strings("passed", C.Passed);
	R.Int("minWeek", C.MinWeek, false);
	R.Int("minLevel", C.MinLevel, false);
	R.Int("maxLevel", C.MaxLevel, false);
	R.Int("minClues", C.MinClues, false);
	R.Int("minStress", C.MinStress, false);
	R.Int("minIncidents", C.MinIncidents, false);
	for (const JsonValue& M : Items(R, "metrics", false))
	{
		Reader MR(M, Where + " metric", Errors);
		WorldCondition::MetricTest T;
		std::string Name;
		MR.Str("metric", Name, true);
		if (!Name.empty() && !MetricFromName(Name, T.Metric)) MR.Error(M.Line, "unknown metric \"" + Name + "\"");
		const JsonValue* Below = MR.Get("below", false);
		const JsonValue* AtLeast = MR.Get("atLeast", false);
		if ((Below != nullptr) == (AtLeast != nullptr)) MR.Error(M.Line, "needs exactly one of below / atLeast");
		const JsonValue* Th = Below ? Below : AtLeast;
		if (Th && Th->IsNumber()) T.Threshold = Th->Number;
		else if (Th) MR.Error(Th->Line, "threshold must be a number");
		T.bBelow = Below != nullptr;
		C.Metrics.push_back(T);
	}
	for (const JsonValue& M : Items(R, "relations", false))
	{
		Reader RR(M, Where + " relation", Errors);
		WorldCondition::RelationTest T;
		RR.Str("id", T.CharacterId, true);
		const JsonValue* Below = RR.Get("below", false);
		const JsonValue* AtLeast = RR.Get("atLeast", false);
		if ((Below != nullptr) == (AtLeast != nullptr)) RR.Error(M.Line, "needs exactly one of below / atLeast");
		const JsonValue* Th = Below ? Below : AtLeast;
		if (Th && Th->IsNumber()) T.Threshold = static_cast<int>(Th->Number);
		else if (Th) RR.Error(Th->Line, "threshold must be a number");
		T.bBelow = Below != nullptr;
		C.Relations.push_back(T);
	}
}

void ReadChoice(const JsonValue& V, const std::string& Where, EventChoiceDef& C, std::vector<std::string>& Errors)
{
	Reader R(V, Where, Errors);
	R.Str("id", C.Id, true);
	R.Str("label", C.Label, true);
	R.Text("outcome", C.Outcome, true);
	R.Effects("effects", C.Effects);
	R.Relations("relations", C.Relations);
	R.Int("stress", C.Stress, false);
	R.Strings("setsFlags", C.SetsFlags);
	R.Str("clue", C.Clue, false);
	for (const JsonValue& S : Items(R, "schedules", false))
	{
		ConsequenceDef Def;
		json::ReadConsequence(S, Where + " schedule", Def, Errors);
		C.Schedules.push_back(Def);
	}
}

bool IsLowerId(const std::string& Id)
{
	if (Id.empty()) return false;
	for (char Ch : Id)
	{
		if (!(std::islower(static_cast<unsigned char>(Ch)) || std::isdigit(static_cast<unsigned char>(Ch)) || Ch == '_')) return false;
	}
	return true;
}

void CheckEffects(const std::vector<MetricEffect>& Effects, const std::string& Where, std::vector<std::string>& Out)
{
	for (const MetricEffect& E : Effects)
	{
		if (!std::isfinite(E.Delta)) Out.push_back(Where + ": effect is not a number");
		else if (IsIndexMetric(E.Metric) && std::fabs(E.Delta) > 50.0) Out.push_back(Where + ": index effect above 50 points");
	}
}
} // namespace

std::vector<std::string> ValidatePeople(const PeopleCatalogue& P)
{
	std::vector<std::string> Out;
	std::set<std::string> Ids;
	for (const CharacterDef& C : P.People)
	{
		const std::string W = "People.json '" + C.Id + "'";
		if (!IsLowerId(C.Id)) Out.push_back(W + ": id must be lower-case letters, digits or _");
		if (!Ids.insert(C.Id).second) Out.push_back(W + ": duplicate id");
		if (C.Name.empty() || C.Role.empty()) Out.push_back(W + ": needs a name and role");
		if (C.StartRelation < RelationMin || C.StartRelation > RelationMax) Out.push_back(W + ": startRelation outside -100..100");
		if (!C.Faction.empty() && C.Faction != "growth" && C.Faction != "prudence") Out.push_back(W + ": faction must be growth or prudence");
		if (C.bPersonal && (C.bExecutive || !C.Faction.empty())) Out.push_back(W + ": personal contacts take no part in office politics");
	}
	return Out;
}

bool PeopleFromJson(const std::string& Text, const std::string& SourceName, PeopleCatalogue& Out, std::vector<std::string>& OutErrors)
{
	const size_t Before = OutErrors.size();
	JsonValue Doc;
	if (!ParseDoc(Text, SourceName, Doc, OutErrors)) return false;
	PeopleCatalogue P;
	{
		Reader R(Doc, SourceName, OutErrors);
		Schema(R);
		for (const JsonValue& Item : Items(R, "people", true))
		{
			CharacterDef C;
			Reader CR(Item, SourceName + " person", OutErrors);
			CR.Str("id", C.Id, true);
			CR.Str("name", C.Name, true);
			CR.Str("role", C.Role, true);
			CR.Str("department", C.Department, false);
			CR.Str("faction", C.Faction, false);
			CR.Text("bio", C.Bio, false);
			CR.Str("location", C.Location, false);
			CR.Bool("executive", C.bExecutive);
			CR.Bool("personal", C.bPersonal);
			CR.Int("startRelation", C.StartRelation, false);
			P.People.push_back(C);
		}
	}
	Out = std::move(P);
	return OutErrors.size() == Before;
}

bool StoryFromJson(const std::string& Text, const std::string& SourceName, StoryCatalogue& Out, std::vector<std::string>& OutErrors)
{
	const size_t Before = OutErrors.size();
	JsonValue Doc;
	if (!ParseDoc(Text, SourceName, Doc, OutErrors)) return false;
	StoryCatalogue S;
	{
		Reader R(Doc, SourceName, OutErrors);
		Schema(R);
		R.Str("title", S.Title, true);
		for (const JsonValue& Item : Items(R, "clues", true))
		{
			ClueDef C;
			Reader CR(Item, SourceName + " clue", OutErrors);
			CR.Str("id", C.Id, true);
			CR.Str("title", C.Title, true);
			CR.Text("summary", C.Summary, true);
			CR.Int("chapter", C.Chapter, true);
			S.Clues.push_back(C);
		}
	}
	Out = std::move(S);
	return OutErrors.size() == Before;
}

bool EventsFromJson(const std::string& Text, const std::string& SourceName, EventCatalogue& OutEvents, std::vector<FalloutTemplate>& OutFallout,
	std::vector<std::string>& OutErrors)
{
	const size_t Before = OutErrors.size();
	JsonValue Doc;
	if (!ParseDoc(Text, SourceName, Doc, OutErrors)) return false;
	EventCatalogue Events;
	std::vector<FalloutTemplate> Fallout;
	{
		Reader R(Doc, SourceName, OutErrors);
		Schema(R);
		for (const JsonValue& Item : Items(R, "events", true))
		{
			EventDef E;
			Reader ER(Item, SourceName + " event", OutErrors);
			ER.Str("id", E.Id, true);
			std::string Cat;
			ER.Str("category", Cat, true);
			bool bKnown = false;
			for (int i = 0; i < static_cast<int>(EMessageCategory::Count); ++i)
			{
				if (Cat == CategoryNames[i])
				{
					E.Category = static_cast<EMessageCategory>(i);
					bKnown = true;
				}
			}
			if (!Cat.empty() && !bKnown) ER.Error(Item.Line, "unknown category \"" + Cat + "\"");
			ER.Str("from", E.From, false);
			ER.Str("subject", E.Subject, true);
			ER.Text("body", E.Body, true);
			if (const JsonValue* W = ER.Get("when", false)) ReadCondition(*W, SourceName + " event '" + E.Id + "' when", E.When, OutErrors);
			ER.Dbl("probability", E.Probability, false);
			ER.Int("deadlineWeeks", E.DeadlineWeeks, false);
			ER.Str("defaultChoice", E.DefaultChoice, false);
			ER.Str("clue", E.Clue, false);
			ER.Str("headline", E.Headline, false);
			ER.Int("headlineTone", E.HeadlineTone, false);
			for (const JsonValue& C : Items(ER, "choices", false))
			{
				EventChoiceDef Def;
				ReadChoice(C, SourceName + " event '" + E.Id + "' choice", Def, OutErrors);
				E.Choices.push_back(Def);
			}
			Events.Events.push_back(E);
		}
		for (const JsonValue& Item : Items(R, "fallout", true))
		{
			FalloutTemplate T;
			Reader FR(Item, SourceName + " fallout", OutErrors);
			FR.Str("department", T.Department, true);
			FR.Text("description", T.Description, true);
			FR.Str("headline", T.Headline, false);
			FR.Effects("effects", T.Effects);
			Fallout.push_back(T);
		}
	}
	OutEvents = std::move(Events);
	OutFallout = std::move(Fallout);
	return OutErrors.size() == Before;
}

std::vector<std::string> ValidateWorld(const WorldContent& W, const MissionCatalogue* Missions)
{
	std::vector<std::string> Out = ValidatePeople(W.People);
	auto Person = [&](const std::string& Id) { return Id == "monitoring" || W.People.Find(Id) != nullptr; };

	std::set<std::string> ClueIds;
	if (W.Story.Title.empty()) Out.push_back("Story.json: missing title");
	for (const ClueDef& C : W.Story.Clues)
	{
		const std::string P = "Story.json '" + C.Id + "'";
		if (!IsLowerId(C.Id) || !ClueIds.insert(C.Id).second) Out.push_back(P + ": id missing, not lower-case or duplicated");
		if (C.Chapter < 1 || C.Chapter > 4) Out.push_back(P + ": chapter outside 1..4");
	}
	std::set<std::string> ObtainableClues;
	auto Clue = [&](const std::string& Id, const std::string& Where) {
		if (Id.empty()) return;
		if (!ClueIds.count(Id)) Out.push_back(Where + ": unknown clue '" + Id + "'");
		ObtainableClues.insert(Id);
	};
	auto Relations = [&](const std::vector<RelationEffect>& Rs, const std::string& Where) {
		for (const RelationEffect& R : Rs)
		{
			if (!W.People.Find(R.CharacterId)) Out.push_back(Where + ": unknown character '" + R.CharacterId + "'");
			if (R.Delta < -40 || R.Delta > 40) Out.push_back(Where + ": relation change above 40");
		}
	};

	std::set<std::string> EventIds;
	for (const EventDef& E : W.Events.Events)
	{
		const std::string P = "Events.json '" + E.Id + "'";
		if (!IsLowerId(E.Id) || !EventIds.insert(E.Id).second) Out.push_back(P + ": id missing, not lower-case or duplicated");
		if (!E.From.empty() && !Person(E.From)) Out.push_back(P + ": unknown sender '" + E.From + "'");
		if (!(E.Probability > 0.0 && E.Probability <= 1.0)) Out.push_back(P + ": probability outside (0, 1]");
		if (E.DeadlineWeeks < 0 || E.DeadlineWeeks > 12) Out.push_back(P + ": deadlineWeeks outside 0..12");
		if (E.HeadlineTone < -1 || E.HeadlineTone > 1) Out.push_back(P + ": headlineTone must be -1, 0 or 1");
		if (E.Choices.empty() != (E.DeadlineWeeks == 0)) Out.push_back(P + ": choices and deadlineWeeks go together");
		if (!E.Choices.empty())
		{
			if (E.Choices.size() < 2) Out.push_back(P + ": needs at least two choices");
			bool bDefault = false;
			std::set<std::string> ChoiceIds;
			for (const EventChoiceDef& C : E.Choices)
			{
				const std::string CP = P + " choice '" + C.Id + "'";
				if (C.Id.empty() || !ChoiceIds.insert(C.Id).second) Out.push_back(CP + ": id missing or duplicated");
				if (C.Label.empty()) Out.push_back(CP + ": missing label");
				if (C.Stress < -30 || C.Stress > 30) Out.push_back(CP + ": stress change outside -30..30");
				bDefault = bDefault || C.Id == E.DefaultChoice;
				CheckEffects(C.Effects, CP, Out);
				Relations(C.Relations, CP);
				Clue(C.Clue, CP);
				for (const ConsequenceDef& Cq : C.Schedules)
				{
					if (Cq.Id.empty() || Cq.Description.empty() || Cq.DelayWeeks < 1 || !(Cq.Probability >= 0.0 && Cq.Probability <= 1.0))
					{
						Out.push_back(CP + ": consequence needs id, description, delay >= 1 and probability 0..1");
					}
					CheckEffects(Cq.Effects, CP, Out);
				}
			}
			if (!bDefault) Out.push_back(P + ": defaultChoice must name one of its choices");
		}
		else if (!E.DefaultChoice.empty())
		{
			Out.push_back(P + ": defaultChoice without choices");
		}
		Clue(E.Clue, P);
		for (const WorldCondition::RelationTest& T : E.When.Relations)
		{
			if (!W.People.Find(T.CharacterId)) Out.push_back(P + ": condition on unknown character '" + T.CharacterId + "'");
		}
		if (Missions)
		{
			for (const std::string& M : E.When.Passed)
			{
				if (M != "M001" && !Missions->Find(M)) Out.push_back(P + ": condition on unknown mission '" + M + "'");
			}
		}
	}

	bool bDefaultFallout = false;
	for (const FalloutTemplate& T : W.Fallout)
	{
		const std::string P = "Events.json fallout '" + T.Department + "'";
		bDefaultFallout = bDefaultFallout || T.Department == "default";
		if (T.Description.empty() || T.Effects.empty()) Out.push_back(P + ": needs a description and effects");
		CheckEffects(T.Effects, P, Out);
	}
	if (!bDefaultFallout) Out.push_back("Events.json: fallout needs a \"default\" template");

	if (Missions)
	{
		for (const MissionDef& M : Missions->Missions)
		{
			for (const MissionEvidenceDef& E : M.Evidence) Clue(E.Clue, M.Id + " evidence '" + E.Id + "'");
			for (const ContextNoteDef& C : M.Context)
			{
				if (!C.RelationId.empty() && !W.People.Find(C.RelationId)) Out.push_back(M.Id + ": context on unknown character '" + C.RelationId + "'");
			}
			for (const MissionQuestionDef& Q : M.Questions)
			{
				for (const MissionOptionDef& O : Q.Options) Relations(O.Relations, M.Id + " option '" + O.Id + "'");
			}
		}
		for (const MissionDef& M : Missions->Missions)
		{
			if (M.RequiresClues > static_cast<int>(ObtainableClues.size()))
			{
				Out.push_back(M.Id + ": requires " + std::to_string(M.RequiresClues) + " clues but only " + std::to_string(ObtainableClues.size()) + " can be found");
			}
		}
	}
	for (const std::string& Id : ClueIds)
	{
		if (Missions && !ObtainableClues.count(Id)) Out.push_back("Story.json '" + Id + "': no evidence, event or reply gives this clue");
	}
	return Out;
}
} // namespace rb
