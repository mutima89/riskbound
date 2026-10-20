// Versioned, checksummed, crash-safe local saves with backup recovery and migration.
// Format and recovery rules: Docs/SaveFormat.md.
#pragma once

#include "RbGameState.h"

#include <string>

namespace rb
{
constexpr int CurrentSaveVersion = 2;

enum class ELoadStatus
{
	Ok,
	RecoveredFromBackup, // primary invalid, backup loaded
	NotFound,
	Corrupt,             // primary and backup both missing or invalid
	UnsupportedVersion   // newer than this build understands
};

const char* ToString(ELoadStatus S);

struct LoadResult
{
	ELoadStatus Status = ELoadStatus::NotFound;
	int LoadedVersion = 0;  // version found on disk, before migration
	bool bMigrated = false;
	std::string Message;
};

// In-memory (de)serialisation. Deserialize verifies header, checksum and version.
std::string SerializeGame(const GameState& State);
bool DeserializeGame(const std::string& Text, GameState& Out, LoadResult& Info);

// Safe write: temp file -> verify -> rotate valid primary to .bak -> rename temp over primary.
// A corrupt primary is moved to .corrupt and never rotated into the backup.
bool SaveGameToFile(const std::string& Path, const GameState& State, std::string* OutError = nullptr);
// Loads primary; on failure falls back to .bak. Never writes to disk.
LoadResult LoadGameFromFile(const std::string& Path, GameState& Out);

std::string BackupPath(const std::string& Path);
} // namespace rb
