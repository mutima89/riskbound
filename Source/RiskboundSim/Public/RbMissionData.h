// Mission content files (Data/Missions/*.json) -> MissionDef. Format: Docs/MissionData.md.
// Strict: unknown keys, wrong types and unknown enum names are errors, reported with file and line.
#pragma once

#include "RbApi.h"

#include "RbCampaign.h"

#include <string>
#include <utility>
#include <vector>

namespace rb
{
constexpr int MissionSchemaVersion = 1;

// Parses one mission file. SourceName (usually the file name) prefixes every error.
RB_API bool MissionFromJson(const std::string& Text, const std::string& SourceName, MissionDef& Out, std::vector<std::string>& OutErrors);

// Parses every (file name, text) pair, checks that "<id>.json" holds mission <id>, then runs
// ValidateCatalogue. Returns true only if there are no errors; Out holds what parsed either way.
RB_API bool LoadMissionCatalogue(const std::vector<std::pair<std::string, std::string>>& Files, MissionCatalogue& Out, std::vector<std::string>& OutErrors);

// Name <-> enum used by the files (exposed for tests and tools).
RB_API bool CompetencyFromName(const std::string& Name, ECompetency& Out);
RB_API bool MetricFromName(const std::string& Name, EOrgMetric& Out);
RB_API const char* MetricName(EOrgMetric M);
RB_API const char* CompetencyName(ECompetency C);
} // namespace rb
