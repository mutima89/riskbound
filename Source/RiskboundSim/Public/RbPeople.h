// People at Valtren and how they feel about the player. Content: Data/People.json.
// Relationships are memory, not a score to grind: they change briefings, open or close optional
// work and inbox events, and never gate promotion (Docs/GameDesign.md "Living world").
#pragma once

#include "RbApi.h"

#include <string>
#include <vector>

namespace rb
{
constexpr int RelationMin = -100;
constexpr int RelationMax = 100;
// Label bands (inclusive lower bounds): Ally >= 50, Friendly >= 20, Neutral > -20, Wary > -50, else Hostile.
constexpr int RelationAlly = 50;
constexpr int RelationFriendly = 20;
constexpr int RelationWary = -20;
constexpr int RelationHostile = -50;

RB_API const char* RelationLabel(int Value);

struct CharacterDef
{
	std::string Id;          // lower-case key used by content ("priya")
	std::string Name;
	std::string Role;
	std::string Department;
	std::string Faction;     // "growth", "prudence" or empty; executives pull in these directions
	std::string Bio;
	std::string Location;    // where they are usually found in the HQ (room name), may be empty
	bool bExecutive = false;
	bool bPersonal = false;  // outside work (family); never part of office politics
	int StartRelation = 0;
};

struct PeopleCatalogue
{
	std::vector<CharacterDef> People;
	RB_API const CharacterDef* Find(const std::string& Id) const;
};

struct RelationEffect
{
	std::string CharacterId;
	int Delta = 0;
};

struct Relation
{
	std::string CharacterId;
	int Value = 0;
};

// Unique lower-case ids, names and roles present, start relation in range, known faction.
RB_API std::vector<std::string> ValidatePeople(const PeopleCatalogue& P);
} // namespace rb
