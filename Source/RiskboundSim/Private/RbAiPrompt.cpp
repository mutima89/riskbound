#include "RbAiPrompt.h"

#include "RbGameState.h"
#include "RbJson.h"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace rb
{
const char* AiColleagueModel()
{
	return "claude-opus-5-5";
}

std::string JsonEscape(const std::string& S)
{
	std::string Out;
	Out.reserve(S.size() + 8);
	for (unsigned char C : S)
	{
		switch (C)
		{
		case '"': Out += "\\\""; break;
		case '\\': Out += "\\\\"; break;
		case '\n': Out += "\\n"; break;
		case '\r': Out += "\\r"; break;
		case '\t': Out += "\\t"; break;
		default:
			if (C < 0x20)
			{
				char Buf[8];
				std::snprintf(Buf, sizeof(Buf), "\\u%04x", C);
				Out += Buf;
			}
			else
			{
				Out += static_cast<char>(C);
			}
		}
	}
	return Out;
}

std::string ColleagueSystemPrompt(const GameState& State, const CharacterDef& Person, const MissionDef* ActiveMission)
{
	const int Rel = GetRelation(State, Person.Id);
	std::ostringstream S;
	S << "You are " << Person.Name << ", " << Person.Role << " at Valtren Coldline Logistics, a fictional temperature-controlled logistics company. "
	  << "This is a conversation inside Riskbound, a risk-management career game. The player is a colleague: "
	  << LevelDef(State.Career.Level).Title << " in the risk team.\n\n";
	S << "Who you are: " << Person.Bio << "\n";
	S << "How you feel about the player: " << RelationLabel(Rel) << " (" << Rel << " on a scale of -100 to 100). Let that colour your warmth, not your honesty.\n\n";
	S << "What is going on at Valtren now: week " << State.Org.Week << "; reputation " << static_cast<int>(State.Org.Get(EOrgMetric::Reputation))
	  << "/100; staff morale " << static_cast<int>(State.Org.Get(EOrgMetric::EmployeeMorale)) << "/100.";
	if (ActiveMission)
	{
		S << " The player is working on the case \"" << ActiveMission->Title << "\" (" << ActiveMission->Department << "). Its briefing: "
		  << ActiveMission->Briefing.substr(0, 700);
	}
	S << "\n\nRules:\n"
	  << "- Stay in character and speak as you would in an office conversation: two to five sentences, plain words, no lists or markdown.\n"
	  << "- You may share your opinion, what you have seen and what you worry about. Do not give the player the answer to a case question, a score, "
	  << "a number they are asked to work out, or which option is right; nudge them to the evidence instead.\n"
	  << "- Everything at Valtren is fictional. Never name real companies, banks, regulators or people.\n"
	  << "- If asked about things outside work at Valtren, steer back politely in character.\n"
	  << "- If you do not know something, say so in character rather than inventing facts about the case.";
	return S.str();
}

std::string ColleagueRequestJson(const std::string& SystemPrompt, const std::vector<AiTurn>& Turns)
{
	const size_t First = Turns.size() > static_cast<size_t>(AiMaxTurnsKept) ? Turns.size() - AiMaxTurnsKept : 0;
	std::ostringstream S;
	S << "{\"model\":\"" << AiColleagueModel() << "\",\"max_tokens\":" << AiMaxTokens << ",";
	S << "\"output_config\":{\"effort\":\"low\"},";
	S << "\"fallbacks\":\"default\",";
	S << "\"system\":\"" << JsonEscape(SystemPrompt) << "\",\"messages\":[";
	bool bFirst = true;
	for (size_t i = First; i < Turns.size(); ++i)
	{
		// The conversation must start with the player.
		if (bFirst && !Turns[i].bPlayer) continue;
		std::string Text = Turns[i].Text;
		if (Turns[i].bPlayer && Text.size() > AiMaxPlayerChars) Text.resize(AiMaxPlayerChars);
		S << (bFirst ? "" : ",") << "{\"role\":\"" << (Turns[i].bPlayer ? "user" : "assistant") << "\",\"content\":\"" << JsonEscape(Text) << "\"}";
		bFirst = false;
	}
	S << "]}";
	return S.str();
}

bool ParseColleagueResponse(const std::string& Body, std::string& OutText, std::string& OutError)
{
	OutText.clear();
	JsonValue Doc;
	std::string JsonError;
	if (!ParseJson(Body, Doc, JsonError) || !Doc.IsObject())
	{
		OutError = "The reply could not be read.";
		return false;
	}
	if (const JsonValue* Err = Doc.Find("error"))
	{
		const JsonValue* Msg = Err->Find("message");
		OutError = Msg && Msg->IsString() ? Msg->String : "The AI service returned an error.";
		return false;
	}
	const JsonValue* Stop = Doc.Find("stop_reason");
	if (Stop && Stop->IsString() && Stop->String == "refusal")
	{
		OutError = "They would rather not talk about that.";
		return false;
	}
	const JsonValue* Content = Doc.Find("content");
	if (!Content || !Content->IsArray())
	{
		OutError = "The reply was empty.";
		return false;
	}
	for (const JsonValue& Block : Content->Items)
	{
		const JsonValue* Type = Block.Find("type");
		const JsonValue* Text = Block.Find("text");
		if (Type && Type->IsString() && Type->String == "text" && Text && Text->IsString()) OutText += Text->String;
	}
	if (OutText.empty())
	{
		OutError = "The reply was empty.";
		return false;
	}
	return true;
}
} // namespace rb
