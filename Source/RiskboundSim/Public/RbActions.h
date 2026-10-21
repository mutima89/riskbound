// Corrective action tracker: actions linked to a register risk, with an owner, a due week and
// evidence of completion. Status is derived from the organisation's current week, never stored,
// so a reload or a time skip always shows the right status. Rules: Docs/RiskMethodology.md section 12.
#pragma once

#include "RbApi.h"

#include <string>
#include <vector>

namespace rb
{
enum class EActionStatus
{
	Open,       // not started, not yet due
	InProgress, // started, not yet due
	Done,       // closed with evidence
	Overdue     // not done and the current week is past the due week
};

RB_API const char* ToString(EActionStatus S);

struct CorrectiveAction
{
	std::string Id;     // "A-001"; assigned by the tracker when empty
	std::string RiskId; // register risk this action treats
	std::string Title;
	std::string Owner;
	int CreatedWeek = 0;
	int DueWeek = 0;    // due at the end of this week
	bool bStarted = false;
	bool bDone = false;
	int DoneWeek = -1;
	std::string CompletionEvidence;
};

// Done beats everything; otherwise Overdue when CurrentWeek > DueWeek; otherwise InProgress or Open.
RB_API EActionStatus ActionStatus(const CorrectiveAction& A, int CurrentWeek);
// True for a done action closed after its due week.
RB_API bool IsCompletedLate(const CorrectiveAction& A);

class RB_API ActionTracker
{
public:
	// Validates (risk, title and owner present, DueWeek >= CreatedWeek), assigns an id if empty.
	// Returns the id, or "" if invalid or the id already exists.
	std::string Create(CorrectiveAction A);
	// Replaces an action by id. Done actions are frozen; returns false for them or unknown ids.
	bool Update(const CorrectiveAction& A);
	bool Start(const std::string& Id);
	// Requires non-empty evidence. Returns false if unknown, already done, or no evidence.
	bool Close(const std::string& Id, int Week, const std::string& Evidence);

	const CorrectiveAction* Find(const std::string& Id) const;
	std::vector<const CorrectiveAction*> ForRisk(const std::string& RiskId) const;
	std::vector<const CorrectiveAction*> Overdue(int CurrentWeek) const;
	std::vector<const CorrectiveAction*> Outstanding() const; // everything not done

	const std::vector<CorrectiveAction>& All() const { return Items; }
	int NextIdNumber() const { return NextId; }
	void SetNextIdNumber(int N) { NextId = N; }
	// Persistence: restores an action verbatim.
	void Restore(CorrectiveAction A) { Items.push_back(std::move(A)); }

private:
	CorrectiveAction* FindMutable(const std::string& Id);

	std::vector<CorrectiveAction> Items;
	int NextId = 1;
};
} // namespace rb
