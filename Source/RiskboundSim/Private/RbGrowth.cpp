#include "RbGrowth.h"

#include "RbGameState.h"
#include "RbJsonReader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace rb
{
// ---------------------------------------------------------------- skills

const std::vector<SkillDef>& SkillDefs()
{
	static const std::vector<SkillDef> D = {
		{"composure", "Wellbeing", "Composure", "Pressure lands softer: stress gains are a quarter smaller.", ""},
		{"self_care", "Wellbeing", "Self-care", "You switch off properly: stress falls 2 more points a week.", "composure"},
		{"networker", "People", "Networker", "People notice when you help: relationship gains are a quarter larger.", ""},
		{"diplomat", "People", "Diplomat", "You disagree without making enemies: relationship losses are a quarter smaller.", "networker"},
		{"crisis_commander", "Crisis", "Crisis command", "Crises wait two more weeks for you before they escalate.", ""},
		{"controls_engineer", "Crisis", "Controls engineer", "Your safety nets catch more: weak calls are a quarter less likely to turn into incidents.", "crisis_commander"},
		{"quant_modeller", "Analysis", "Quant modeller", "Opens analytical replies that settle arguments with numbers.", ""},
		{"negotiator", "Influence", "Negotiator", "Opens replies that find the deal both sides can live with.", ""},
		{"regulatory_insight", "Influence", "Regulatory insight", "Opens replies that work with the regulator rather than around it.", "negotiator"},
		{"board_presence", "Influence", "Board presence", "Opens replies that hold the room with executives and the board.", "regulatory_insight"},
	};
	return D;
}

const SkillDef* FindSkill(const std::string& Id)
{
	for (const SkillDef& S : SkillDefs())
	{
		if (Id == S.Id) return &S;
	}
	return nullptr;
}

bool HasSkill(const GameState& State, const std::string& Id)
{
	return std::find(State.World.Skills.begin(), State.World.Skills.end(), Id) != State.World.Skills.end();
}

int SkillPointsEarned(const GameState& State)
{
	const int Passed = static_cast<int>(State.Career.PassedMissions.size());
	const BackgroundDef* B = FindBackground(State.World.Background);
	return std::max(0, State.Career.Level - 2) + Passed / 4 + (B ? B->BonusPoints : 0);
}

int SkillPointsAvailable(const GameState& State)
{
	const BackgroundDef* B = FindBackground(State.World.Background);
	const int Free = (B && B->StartingSkill[0] != '\0') ? 1 : 0; // the background's skill is a gift, not a spend
	return std::max(0, SkillPointsEarned(State) - (static_cast<int>(State.World.Skills.size()) - Free));
}

const char* ToString(ESkillError E)
{
	switch (E)
	{
	case ESkillError::None: return "None";
	case ESkillError::Unknown: return "Unknown skill.";
	case ESkillError::AlreadyLearned: return "You already have this skill.";
	case ESkillError::NeedsPrerequisite: return "Learn the skill before it in this branch first.";
	case ESkillError::NoPoints: return "No skill points left: they come with promotions and passed missions.";
	}
	return "?";
}

ESkillError LearnSkill(GameState& State, const std::string& Id)
{
	const SkillDef* S = FindSkill(Id);
	if (!S) return ESkillError::Unknown;
	if (HasSkill(State, Id)) return ESkillError::AlreadyLearned;
	if (S->Requires[0] != '\0' && !HasSkill(State, S->Requires)) return ESkillError::NeedsPrerequisite;
	if (SkillPointsAvailable(State) <= 0) return ESkillError::NoPoints;
	State.World.Skills.push_back(Id);
	return ESkillError::None;
}

double StressGainFactor(const GameState& State) { return HasSkill(State, "composure") ? 0.75 : 1.0; }
int ExtraRecoveryPerWeek(const GameState& State) { return HasSkill(State, "self_care") ? 2 : 0; }
double RelationGainFactor(const GameState& State) { return HasSkill(State, "networker") ? 1.25 : 1.0; }
double RelationLossFactor(const GameState& State) { return HasSkill(State, "diplomat") ? 0.75 : 1.0; }
int ExtraCrisisWeeks(const GameState& State) { return HasSkill(State, "crisis_commander") ? 2 : 0; }
double FalloutFactor(const GameState& State) { return HasSkill(State, "controls_engineer") ? 0.75 : 1.0; }

// ---------------------------------------------------------------- backgrounds

const std::vector<BackgroundDef>& BackgroundDefs()
{
	static const std::vector<BackgroundDef> D = {
		{"graduate", "Graduate", "Straight from a finance degree and the graduate scheme. No baggage, no habits to unlearn, and one extra skill point to spend.", "", 1, {{"priya", 5}}},
		{"auditor", "Ex-auditor", "Five years in internal audit. You test controls in your sleep; Compliance trusts you, Operations is wary.", "controls_engineer", 0,
			{{"ruth", 15}, {"amara", -5}, {"tomasz", -5}}},
		{"trader", "Ex-trader", "A commodities desk taught you to price risk fast. Finance and Commercial like your numbers; Health and Safety thinks you are a cowboy.", "quant_modeller", 0,
			{{"hollis", 10}, {"rafe", 10}, {"grace", -10}}},
		{"operations", "Ex-operations", "You ran a depot night shift before moving into risk. The yard trusts you; the boardroom has yet to.", "crisis_commander", 0,
			{{"amara", 15}, {"grace", 10}, {"adrian", -5}}},
	};
	return D;
}

const BackgroundDef* FindBackground(const std::string& Id)
{
	for (const BackgroundDef& B : BackgroundDefs())
	{
		if (Id == B.Id) return &B;
	}
	return nullptr;
}

bool ApplyBackground(GameState& State, const std::string& Id)
{
	const BackgroundDef* B = FindBackground(Id);
	if (!B) return false;
	State.World.Background = Id;
	if (B->StartingSkill[0] != '\0' && !HasSkill(State, B->StartingSkill)) State.World.Skills.push_back(B->StartingSkill);
	for (const RelationEffect& R : B->Relations)
	{
		bool bFound = false;
		for (Relation& Rel : State.World.Relations)
		{
			if (Rel.CharacterId == R.CharacterId)
			{
				Rel.Value = std::max(RelationMin, std::min(RelationMax, Rel.Value + R.Delta));
				bFound = true;
			}
		}
		if (!bFound) State.World.Relations.push_back({R.CharacterId, std::max(RelationMin, std::min(RelationMax, R.Delta))});
	}
	return true;
}

// ---------------------------------------------------------------- conversations

const char* ToString(ETone T)
{
	switch (T)
	{
	case ETone::Diplomatic: return "Diplomatic";
	case ETone::Direct: return "Direct";
	case ETone::Analytical: return "Analytical";
	case ETone::Count: break;
	}
	return "?";
}

const ConversationDef* DialogueCatalogue::Find(const std::string& Id) const
{
	for (const ConversationDef& C : Conversations)
	{
		if (C.Id == Id) return &C;
	}
	return nullptr;
}

const ConversationDef* ConversationFor(const GameState& State, const DialogueCatalogue& C, const std::string& Person, const std::string& Trigger)
{
	for (const ConversationDef& Conv : C.Conversations)
	{
		if (Conv.Person != Person || Conv.Trigger != Trigger) continue;
		if (std::find(State.World.ConversationsHad.begin(), State.World.ConversationsHad.end(), Conv.Id) != State.World.ConversationsHad.end()) continue;
		if (!ConditionHolds(State, Conv.When)) continue;
		return &Conv;
	}
	return nullptr;
}

std::vector<const DialogueResponseDef*> AvailableResponses(const GameState& State, const DialogueNodeDef& Node)
{
	std::vector<const DialogueResponseDef*> Out;
	for (const DialogueResponseDef& R : Node.Responses)
	{
		if (R.RequiresSkill.empty() || HasSkill(State, R.RequiresSkill)) Out.push_back(&R);
	}
	return Out;
}

const char* ToString(EDialogueError E)
{
	switch (E)
	{
	case EDialogueError::None: return "None";
	case EDialogueError::UnknownConversation: return "Unknown conversation.";
	case EDialogueError::UnknownNode: return "Unknown point in the conversation.";
	case EDialogueError::UnknownResponse: return "Unknown reply.";
	case EDialogueError::NeedsSkill: return "That reply needs a skill you do not have.";
	}
	return "?";
}

EDialogueError ChooseResponse(GameState& State, const DialogueCatalogue& C, const std::string& ConversationId, const std::string& NodeId,
	const std::string& ResponseId, std::string& OutNext)
{
	const ConversationDef* Conv = C.Find(ConversationId);
	if (!Conv) return EDialogueError::UnknownConversation;
	const auto Node = std::find_if(Conv->Nodes.begin(), Conv->Nodes.end(), [&](const DialogueNodeDef& N) { return N.Id == NodeId; });
	if (Node == Conv->Nodes.end()) return EDialogueError::UnknownNode;
	const auto R = std::find_if(Node->Responses.begin(), Node->Responses.end(), [&](const DialogueResponseDef& X) { return X.Id == ResponseId; });
	if (R == Node->Responses.end()) return EDialogueError::UnknownResponse;
	if (!R->RequiresSkill.empty() && !HasSkill(State, R->RequiresSkill)) return EDialogueError::NeedsSkill;
	State.Org.Apply(R->Effects);
	for (const RelationEffect& Rel : R->Relations) ChangeRelation(State, Rel.CharacterId, Rel.Delta);
	ChangeStress(State, R->Stress);
	for (const std::string& F : R->SetsFlags) State.Org.SetFlag(F);
	AddClue(State, R->Clue);
	OutNext = R->Next;
	if (OutNext.empty() && std::find(State.World.ConversationsHad.begin(), State.World.ConversationsHad.end(), Conv->Id) == State.World.ConversationsHad.end())
	{
		State.World.ConversationsHad.push_back(Conv->Id);
	}
	return EDialogueError::None;
}

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
} // namespace

bool DialogueFromJson(const std::string& Text, const std::string& SourceName, DialogueCatalogue& Out, std::vector<std::string>& OutErrors)
{
	const size_t Before = OutErrors.size();
	JsonValue Doc;
	std::string JsonError;
	if (!ParseJson(Text, Doc, JsonError))
	{
		OutErrors.push_back(SourceName + ": " + JsonError);
		return false;
	}
	DialogueCatalogue Cat;
	{
		Reader R(Doc, SourceName, OutErrors);
		int Schema = 0;
		R.Int("schema", Schema, true);
		if (Schema != 1) R.Error(Doc.Line, "schema must be 1");
		for (const JsonValue& CV : Items(R, "conversations", true))
		{
			ConversationDef Conv;
			Reader CR(CV, SourceName + " conversation", OutErrors);
			CR.Str("id", Conv.Id, true);
			CR.Str("person", Conv.Person, true);
			CR.Str("title", Conv.Title, true);
			CR.Str("trigger", Conv.Trigger, true);
			if (const JsonValue* W = CR.Get("when", false))
			{
				// Same condition keys as Data/Events.json (flags, passed, minLevel, relations...).
				Reader WR(*W, SourceName + " conversation '" + Conv.Id + "' when", OutErrors);
				WR.Strings("flags", Conv.When.Flags);
				WR.Strings("notFlags", Conv.When.NotFlags);
				WR.Strings("passed", Conv.When.Passed);
				WR.Int("minLevel", Conv.When.MinLevel, false);
				WR.Int("maxLevel", Conv.When.MaxLevel, false);
				WR.Int("minClues", Conv.When.MinClues, false);
				WR.Int("minStress", Conv.When.MinStress, false);
			}
			for (const JsonValue& NV : Items(CR, "nodes", true))
			{
				DialogueNodeDef N;
				Reader NR(NV, SourceName + " conversation '" + Conv.Id + "' node", OutErrors);
				NR.Str("id", N.Id, true);
				NR.Text("line", N.Line, true);
				for (const JsonValue& RV : Items(NR, "responses", true))
				{
					DialogueResponseDef Resp;
					Reader RR(RV, SourceName + " conversation '" + Conv.Id + "' response", OutErrors);
					RR.Str("id", Resp.Id, true);
					RR.Str("label", Resp.Label, true);
					std::string Tone = "diplomatic";
					RR.Str("tone", Tone, false);
					if (Tone == "diplomatic") Resp.Tone = ETone::Diplomatic;
					else if (Tone == "direct") Resp.Tone = ETone::Direct;
					else if (Tone == "analytical") Resp.Tone = ETone::Analytical;
					else RR.Error(RV.Line, "tone must be diplomatic, direct or analytical");
					RR.Str("next", Resp.Next, false);
					RR.Str("requiresSkill", Resp.RequiresSkill, false);
					RR.Effects("effects", Resp.Effects);
					RR.Relations("relations", Resp.Relations);
					RR.Int("stress", Resp.Stress, false);
					RR.Strings("setsFlags", Resp.SetsFlags);
					RR.Str("clue", Resp.Clue, false);
					N.Responses.push_back(Resp);
				}
				Conv.Nodes.push_back(N);
			}
			Cat.Conversations.push_back(Conv);
		}
	}
	Out = std::move(Cat);
	return OutErrors.size() == Before;
}

std::vector<std::string> ValidateDialogue(const DialogueCatalogue& C, const PeopleCatalogue& People, const StoryCatalogue& Story)
{
	std::vector<std::string> Out;
	std::set<std::string> Ids;
	for (const ConversationDef& Conv : C.Conversations)
	{
		const std::string P = "Dialogue.json '" + Conv.Id + "'";
		if (Conv.Id.empty() || !Ids.insert(Conv.Id).second) Out.push_back(P + ": id missing or duplicated");
		if (!People.Find(Conv.Person)) Out.push_back(P + ": unknown person '" + Conv.Person + "'");
		if (Conv.Trigger != "talk" && Conv.Trigger.rfind("evidence:", 0) != 0) Out.push_back(P + ": trigger must be \"talk\" or \"evidence:<mission>/<evidence>\"");
		if (Conv.Nodes.empty()) Out.push_back(P + ": no nodes");
		std::set<std::string> NodeIds;
		for (const DialogueNodeDef& N : Conv.Nodes)
		{
			if (N.Id.empty() || !NodeIds.insert(N.Id).second) Out.push_back(P + ": node id missing or duplicated");
		}
		for (const DialogueNodeDef& N : Conv.Nodes)
		{
			int Ungated = 0;
			for (const DialogueResponseDef& R : N.Responses)
			{
				const std::string RP = P + " node '" + N.Id + "' response '" + R.Id + "'";
				if (R.Label.empty()) Out.push_back(RP + ": missing label");
				if (!R.Next.empty() && !NodeIds.count(R.Next)) Out.push_back(RP + ": next node '" + R.Next + "' does not exist");
				if (!R.RequiresSkill.empty() && !FindSkill(R.RequiresSkill)) Out.push_back(RP + ": unknown skill '" + R.RequiresSkill + "'");
				if (!R.Clue.empty() && !Story.Find(R.Clue)) Out.push_back(RP + ": unknown clue '" + R.Clue + "'");
				for (const RelationEffect& Rel : R.Relations)
				{
					if (!People.Find(Rel.CharacterId)) Out.push_back(RP + ": unknown character '" + Rel.CharacterId + "'");
				}
				Ungated += R.RequiresSkill.empty() ? 1 : 0;
			}
			if (Ungated == 0) Out.push_back(P + " node '" + N.Id + "': every player needs at least one reply without a skill");
		}
	}
	return Out;
}
} // namespace rb
