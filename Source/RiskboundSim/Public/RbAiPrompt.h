// Optional AI colleagues (idea 29): builds the in-character system prompt and the request body for
// a free-typed conversation with a colleague. Pure string work, no network: the UE layer
// (URbAiColleaguesSubsystem) sends it only when the player has switched AI colleagues on and
// supplied their own API key. The game is fully playable without it.
#pragma once

#include "RbApi.h"

#include <string>
#include <vector>

namespace rb
{
struct GameState;
struct CharacterDef;
struct MissionDef;

struct AiTurn
{
	bool bPlayer = true;  // false: the colleague
	std::string Text;
};

// Model and limits for colleague chat (Docs/AiColleagues.md).
RB_API const char* AiColleagueModel();      // "claude-opus-5-5"
constexpr int AiMaxTokens = 1024;
constexpr int AiMaxTurnsKept = 12;          // older turns are dropped from the request
constexpr size_t AiMaxPlayerChars = 600;    // longer player messages are cut before sending

// The system prompt: who the colleague is, how they feel about the player, what is going on at
// Valtren right now, and the rules (stay in character, fictional company, never give away case
// answers or rubric scores, short spoken replies).
RB_API std::string ColleagueSystemPrompt(const GameState& State, const CharacterDef& Person, const MissionDef* ActiveMission);

// The JSON body for POST /v1/messages (effort low, refusal fallbacks on).
RB_API std::string ColleagueRequestJson(const std::string& SystemPrompt, const std::vector<AiTurn>& Turns);

// Extracts the reply text from a Messages API response body. False with OutError on an error body,
// a refusal or unparseable JSON.
RB_API bool ParseColleagueResponse(const std::string& Body, std::string& OutText, std::string& OutError);

RB_API std::string JsonEscape(const std::string& S);
} // namespace rb
