// Phase 4: learning content, endings, performance reviews, certificates, decision replays,
// challenge modes and a local leaderboard. Content: Data/Glossary.json, Data/Endings.json.
// Nothing here changes a rubric score or a promotion gate.
#pragma once

#include "RbApi.h"

#include "RbCampaign.h"
#include "RbPeople.h"
#include "RbWorld.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rb
{
struct GameState;

// ---------------------------------------------------------------- glossary
struct GlossaryTerm
{
	std::string Id;
	std::string Term;
	std::string Category;
	std::string Definition;
	std::string Example;
	std::vector<std::string> Missions; // where the concept is used (for "learn more" links)
};

RB_API bool GlossaryFromJson(const std::string& Text, const std::string& SourceName, std::vector<GlossaryTerm>& Out, std::vector<std::string>& OutErrors);
RB_API std::vector<std::string> ValidateGlossary(const std::vector<GlossaryTerm>& Terms, const MissionCatalogue* Missions);
// Terms linked to a mission (shown in the case file as "Concepts in this case").
RB_API std::vector<const GlossaryTerm*> TermsForMission(const std::vector<GlossaryTerm>& Terms, const std::string& MissionId);

// ---------------------------------------------------------------- endings
struct EndingDef
{
	std::string Id;
	std::string Title;
	WorldCondition When;          // empty: always (the last ending is the default)
	std::string Epilogue;
};

RB_API bool EndingsFromJson(const std::string& Text, const std::string& SourceName, std::vector<EndingDef>& Out, std::vector<std::string>& OutErrors);
RB_API std::vector<std::string> ValidateEndings(const std::vector<EndingDef>& Endings);
// The first ending whose condition holds (content lists them in priority order).
RB_API const EndingDef* ChooseEnding(const GameState& State, const std::vector<EndingDef>& Endings);

// ---------------------------------------------------------------- performance review
struct PerformanceReview
{
	int Level = 0;
	std::string Title;
	int MissionsPassed = 0;
	int AverageScore = 0;            // over every submitted attempt
	int IncidentsOccurred = 0;
	int IncidentsAvoided = 0;
	std::vector<std::string> Strengths; // competencies at or above 75
	std::vector<std::string> Growth;    // competencies below 60 (assessed ones only)
	std::string Ally;                   // best relationship (work)
	std::string Strained;               // worst relationship (work), if Wary or worse
	std::string Summary;                // one paragraph for the review screen
};

RB_API PerformanceReview MakeReview(const GameState& State, const PeopleCatalogue& People);

// ---------------------------------------------------------------- certificate
// A plain-text career certificate (Markdown) the player can export and share.
RB_API std::string CertificateMarkdown(const GameState& State, const PerformanceReview& Review, const EndingDef* Ending, const std::string& Date);

// ---------------------------------------------------------------- decision replay ("what if")
struct WhatIfOption
{
	std::string Label;
	int Score = 0;          // what it would have scored (Single) or whether it was correct (Multi: 100 / 0)
	bool bChosen = false;
	std::vector<std::string> Consequences; // what it would have scheduled
};

struct WhatIfQuestion
{
	std::string Prompt;
	std::string Kind;
	std::vector<WhatIfOption> Options; // empty for numeric and rating
	std::string YourAnswer;            // for numeric and rating
	std::string Explanation;
};

// For an attempt already submitted (the rubric is no longer hidden), every alternative and its effect.
RB_API std::vector<WhatIfQuestion> WhatIf(const MissionDef& M, const MissionRecord& Record);

// ---------------------------------------------------------------- challenge modes
enum class EGameMode
{
	Career,   // the normal game
	Survival, // one crisis after another until three are failed
	Daily     // three cases chosen by the date, one attempt each
};

RB_API const char* ToString(EGameMode M);

struct ChallengeState
{
	EGameMode Mode = EGameMode::Career;
	std::vector<std::string> Sequence; // mission ids in play order
	int Index = 0;                     // next in the sequence
	int Lives = 3;                     // survival: failures left
	int Points = 0;                    // sum of scores (daily) or crises passed (survival)
	bool bOver = false;
	std::string Date;                  // daily: YYYY-MM-DD
};

// Sets up a challenge on a fresh game. Survival: every crisis, shuffled by the seed. Daily: three
// case studies and crises chosen by the date (everyone on the same day gets the same three).
RB_API void StartChallenge(GameState& State, const MissionCatalogue& C, EGameMode Mode, const std::string& Date = std::string());
// A fixed sequence (classroom: everyone plays the same case), scored like Daily.
RB_API void StartCustomChallenge(GameState& State, const std::vector<std::string>& Sequence);
RB_API std::string CurrentChallengeMission(const GameState& State); // "" when over
// Records a submitted attempt in the challenge and moves on. Returns true if the challenge just ended.
RB_API bool AdvanceChallenge(GameState& State, const MissionDebrief& D);
// In a challenge, the current mission opens regardless of level, prerequisites and triggers.
RB_API bool ChallengeUnlocks(const GameState& State, const std::string& MissionId);

// ---------------------------------------------------------------- local leaderboard
struct LeaderboardEntry
{
	std::string Mode;       // career, survival, daily
	std::string Profile;
	std::string Detail;     // ending, or the date
	int Points = 0;         // career: average score; survival: crises; daily: total
	int Weeks = 0;
	std::string When;       // date recorded
};

RB_API std::string SerializeLeaderboard(const std::vector<LeaderboardEntry>& Entries);
RB_API std::vector<LeaderboardEntry> DeserializeLeaderboard(const std::string& Text); // tolerant: skips bad lines
// Adds an entry and keeps the best 20 per mode, best first.
RB_API void AddToLeaderboard(std::vector<LeaderboardEntry>& Entries, const LeaderboardEntry& E);
} // namespace rb
