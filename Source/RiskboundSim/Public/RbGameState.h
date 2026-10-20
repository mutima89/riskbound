// Everything that persists between sessions. Serialised by RbSave. Format: Docs/SaveFormat.md.
#pragma once

#include "RbApi.h"

#include "RbActions.h"
#include "RbCareer.h"
#include "RbKri.h"
#include "RbMission001.h"
#include "RbOrganization.h"
#include "RbRiskModel.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rb
{
struct PlayerSettings
{
	double MouseSensitivity = 1.0; // 0.1..5
	double TextScale = 1.0;        // 0.8..1.6
	double MasterVolume = 0.8;     // 0..1
	bool bReducedMotion = false;
	bool bHints = true;
	bool bFullscreen = true;
	int GraphicsQuality = 2;       // 0 low .. 3 epic
};

// A register risk the player has quantified with loss scenarios (Loss Scenarios tool).
struct QuantifiedRisk
{
	std::string RiskId;
	std::vector<LossScenario> Scenarios;
};

struct GameState
{
	std::string ProfileName = "Analyst";
	EDifficulty Difficulty = EDifficulty::Professional;
	uint64_t Seed = 20261020;
	CareerState Career;
	OrganizationState Org;
	RiskRegister Register;
	m001::MissionState Mission001;
	std::vector<std::string> Achievements;
	std::vector<std::string> LearningHistory;
	std::string Checkpoint = "M001:start";
	PlayerSettings Settings;
	ActionTracker Actions;                  // corrective actions (save v3)
	KriMonitor Kris;                        // KRI history (save v3)
	std::vector<QuantifiedRisk> Quantified; // player-entered loss scenarios (save v3)
};

constexpr int KriHistoryWeeksAtStart = 12;

// Mission 001 submission at game level: MissionState::Submit plus the corrective actions
// for the chosen treatment. Returns the submit error; actions are only created on success.
RB_API m001::ESubmitError SubmitMission001(GameState& State, const m001::Submission& S);

// Advances organisation time: resolves due consequences and records a KRI reading per week.
RB_API std::vector<ResolvedConsequence> AdvanceGameWeeks(GameState& State, int Weeks);

// Stores (or replaces) the player's loss scenarios for a risk. False if the risk is unknown
// or the scenarios are invalid (ValidateScenarios, mutually exclusive).
RB_API bool SetQuantification(GameState& State, const std::string& RiskId, const std::vector<LossScenario>& Scenarios);

// Default operational-risk register for an organisation profile: 5x5 matrix,
// appetite limits by category, seeded with the stale R-014 entry Mission 001 investigates.
RB_API RiskRegister MakeDefaultRegister();

// New game for the given profile and difficulty, with event seed derived from Seed.
RB_API GameState NewGame(const std::string& ProfileName, EDifficulty Difficulty, uint64_t Seed, const std::string& OrgProfileId = "meridian");

// ---- Achievements (Docs/GameDesign.md) ----
struct AchievementDef
{
	std::string Id;
	std::string Title;
	std::string Description;
};

RB_API const std::vector<AchievementDef>& AchievementDefs();
// Unlocks achievements whose conditions now hold. Returns the ids newly unlocked.
RB_API std::vector<std::string> EvaluateAchievements(GameState& State);
} // namespace rb
