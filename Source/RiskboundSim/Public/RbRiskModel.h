// Qualitative risk model: configurable matrix, controls, residual risk, appetite, register.
// Scores here are ORDINAL. They rank risks; they are not money. See Docs/RiskMethodology.md.
#pragma once

#include <string>
#include <vector>

namespace rb
{
enum class ERating
{
	Low,
	Medium,
	High,
	Critical
};

const char* ToString(ERating Rating);

// N x N likelihood/impact matrix. Score = L * I. Rating bands by score thresholds,
// optionally overridden cell-by-cell (some organisations weight impact more heavily).
struct RiskMatrix
{
	int Size = 5;
	int MediumFrom = 5;
	int HighFrom = 10;
	int CriticalFrom = 16;
	// Optional Size*Size overrides, row = likelihood-1, col = impact-1. Empty = use bands.
	std::vector<ERating> CellOverrides;

	bool IsValid() const;
	int Clamp(int Level) const;
	int Score(int Likelihood, int Impact) const;
	ERating Rate(int Likelihood, int Impact) const;
};

enum class EControlType
{
	Preventive, // reduces likelihood
	Detective,  // reduces impact (earlier detection limits damage)
	Corrective  // reduces impact
};

enum class EControlEffectiveness
{
	Ineffective = 0,
	PartiallyEffective = 1,
	Effective = 2
};

const char* ToString(EControlEffectiveness E);

struct Control
{
	std::string Name;
	EControlType Type = EControlType::Preventive;
	int DesignScore = 0;    // 0..3 : is the control designed to address the cause?
	int OperatingScore = 0; // 0..3 : evidence it operates as designed?
};

// Effectiveness is limited by the weaker of design and operation (a well-designed
// control that never runs is ineffective). 0-1 Ineffective, 2 Partial, 3 Effective.
EControlEffectiveness AssessControl(const Control& C);

struct ResidualResult
{
	int Likelihood = 1;
	int Impact = 1;
};

// Residual = inherent reduced by the single strongest control per dimension
// (reductions do not stack), floored at 1.
ResidualResult ComputeResidual(const RiskMatrix& M, int InherentL, int InherentI, const std::vector<Control>& Controls);

enum class EAppetiteStatus
{
	Within,
	AtLimit,
	Exceeds
};

const char* ToString(EAppetiteStatus S);

struct AppetiteRule
{
	std::string Category;
	int MaxResidualScore = 9;
};

struct RiskAppetite
{
	std::vector<AppetiteRule> Rules;
	int DefaultMaxResidualScore = 9;

	int LimitFor(const std::string& Category) const;
	EAppetiteStatus Check(const std::string& Category, int ResidualScore) const;
};

enum class ETreatment
{
	Undecided,
	Avoid,
	Reduce,
	Transfer,
	Accept
};

enum class ERiskStatus
{
	Open,
	InTreatment,
	Escalated,
	Closed
};

const char* ToString(ETreatment T);
const char* ToString(ERiskStatus S);

struct RiskEntry
{
	std::string Id;
	std::string Title;
	std::string Cause;
	std::string Event;
	std::string Consequence;
	std::string Category;
	std::string Owner;
	int InherentLikelihood = 1;
	int InherentImpact = 1;
	std::vector<Control> Controls;
	int ResidualLikelihood = 1;
	int ResidualImpact = 1;
	EAppetiteStatus Appetite = EAppetiteStatus::Within;
	ETreatment Treatment = ETreatment::Undecided;
	std::string ActionOwner;
	std::string ActionDeadline; // ISO date, in-game calendar
	std::vector<std::string> Evidence;
	ERiskStatus Status = ERiskStatus::Open;
};

struct RiskFilter
{
	std::string Category; // empty = any
	std::string Owner;    // empty = any
	bool bFilterStatus = false;
	ERiskStatus Status = ERiskStatus::Open;
	bool bFilterMinRating = false;
	ERating MinResidualRating = ERating::Low;
	bool bOnlyOutsideAppetite = false;
};

class RiskRegister
{
public:
	RiskRegister() = default;
	RiskRegister(RiskMatrix InMatrix, RiskAppetite InAppetite);

	// Assigns an Id if empty, recomputes residual + appetite. Returns the Id, or "" if the Id clashes.
	std::string Add(RiskEntry Entry);
	bool Update(const RiskEntry& Entry); // false if Id not found
	bool Remove(const std::string& Id);
	const RiskEntry* Find(const std::string& Id) const;

	std::vector<const RiskEntry*> Filter(const RiskFilter& F) const;
	// Case-insensitive substring search over id, title, cause, event, consequence, owner.
	std::vector<const RiskEntry*> Search(const std::string& Query) const;

	const std::vector<RiskEntry>& Entries() const { return Items; }
	const RiskMatrix& Matrix() const { return MatrixCfg; }
	const RiskAppetite& Appetite() const { return AppetiteCfg; }
	int NextIdNumber() const { return NextId; }
	void SetNextIdNumber(int N) { NextId = N; }

	// Used by persistence to restore entries verbatim (still re-derives computed fields).
	void RestoreEntry(RiskEntry Entry);

private:
	void Recompute(RiskEntry& E) const;

	RiskMatrix MatrixCfg;
	RiskAppetite AppetiteCfg;
	std::vector<RiskEntry> Items;
	int NextId = 1;
};
} // namespace rb
