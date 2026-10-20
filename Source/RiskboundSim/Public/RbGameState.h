// Everything that persists between sessions. Serialised by RbSave. Format: Docs/SaveFormat.md.
#pragma once

#include "RbApi.h"

#include "RbCareer.h"
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
};

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
