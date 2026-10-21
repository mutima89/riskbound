// Versioned, checksummed, crash-safe local saves with backup recovery and migration.
// Format and recovery rules: Docs/SaveFormat.md.
#pragma once

#include "RbApi.h"

#include "RbGameState.h"

#include <string>

namespace rb
{
constexpr int CurrentSaveVersion = 9;

enum class ELoadStatus
{
	Ok,
	RecoveredFromBackup, // primary invalid, backup loaded
	NotFound,
	Corrupt,             // primary and backup both missing or invalid
	UnsupportedVersion   // newer than this build understands
};

RB_API const char* ToString(ELoadStatus S);

struct LoadResult
{
	ELoadStatus Status = ELoadStatus::NotFound;
	int LoadedVersion = 0;  // version found on disk, before migration
	bool bMigrated = false;
	std::string Message;
};

// In-memory (de)serialisation. Deserialize verifies header, checksum and version.
RB_API std::string SerializeGame(const GameState& State);
RB_API bool DeserializeGame(const std::string& Text, GameState& Out, LoadResult& Info);

// Safe write: temp file -> verify -> rotate valid primary to .bak -> rename temp over primary.
// A corrupt primary is moved to .corrupt and never rotated into the backup.
RB_API bool SaveGameToFile(const std::string& Path, const GameState& State, std::string* OutError = nullptr);
// Loads primary; on failure falls back to .bak. Never writes to disk.
RB_API LoadResult LoadGameFromFile(const std::string& Path, GameState& Out);

RB_API std::string BackupPath(const std::string& Path);
} // namespace rb
