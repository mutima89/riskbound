#include "RbActions.h"

#include <cstdio>

namespace rb
{
const char* ToString(EActionStatus S)
{
	switch (S)
	{
	case EActionStatus::Open: return "Open";
	case EActionStatus::InProgress: return "In progress";
	case EActionStatus::Done: return "Done";
	case EActionStatus::Overdue: return "Overdue";
	}
	return "?";
}

EActionStatus ActionStatus(const CorrectiveAction& A, int CurrentWeek)
{
	if (A.bDone) return EActionStatus::Done;
	if (CurrentWeek > A.DueWeek) return EActionStatus::Overdue;
	return A.bStarted ? EActionStatus::InProgress : EActionStatus::Open;
}

bool IsCompletedLate(const CorrectiveAction& A)
{
	return A.bDone && A.DoneWeek > A.DueWeek;
}

std::string ActionTracker::Create(CorrectiveAction A)
{
	if (A.RiskId.empty() || A.Title.empty() || A.Owner.empty() || A.DueWeek < A.CreatedWeek) return std::string();
	if (A.Id.empty())
	{
		char Buf[16];
		do
		{
			std::snprintf(Buf, sizeof(Buf), "A-%03d", NextId++);
		} while (Find(Buf) != nullptr);
		A.Id = Buf;
	}
	else if (Find(A.Id) != nullptr)
	{
		return std::string();
	}
	A.bDone = false;
	A.DoneWeek = -1;
	A.CompletionEvidence.clear();
	Items.push_back(A);
	return A.Id;
}

CorrectiveAction* ActionTracker::FindMutable(const std::string& Id)
{
	for (CorrectiveAction& A : Items)
	{
		if (A.Id == Id) return &A;
	}
	return nullptr;
}

const CorrectiveAction* ActionTracker::Find(const std::string& Id) const
{
	for (const CorrectiveAction& A : Items)
	{
		if (A.Id == Id) return &A;
	}
	return nullptr;
}

bool ActionTracker::Update(const CorrectiveAction& A)
{
	CorrectiveAction* Existing = FindMutable(A.Id);
	if (!Existing || Existing->bDone) return false;
	if (A.RiskId.empty() || A.Title.empty() || A.Owner.empty() || A.DueWeek < A.CreatedWeek) return false;
	CorrectiveAction Copy = A;
	Copy.bDone = false; // closing goes through Close() so evidence is always recorded
	Copy.DoneWeek = -1;
	Copy.CompletionEvidence.clear();
	*Existing = Copy;
	return true;
}

bool ActionTracker::Start(const std::string& Id)
{
	CorrectiveAction* A = FindMutable(Id);
	if (!A || A->bDone) return false;
	A->bStarted = true;
	return true;
}

bool ActionTracker::Close(const std::string& Id, int Week, const std::string& Evidence)
{
	CorrectiveAction* A = FindMutable(Id);
	if (!A || A->bDone || Evidence.empty()) return false;
	A->bStarted = true;
	A->bDone = true;
	A->DoneWeek = Week;
	A->CompletionEvidence = Evidence;
	return true;
}

std::vector<const CorrectiveAction*> ActionTracker::ForRisk(const std::string& RiskId) const
{
	std::vector<const CorrectiveAction*> Out;
	for (const CorrectiveAction& A : Items)
	{
		if (A.RiskId == RiskId) Out.push_back(&A);
	}
	return Out;
}

std::vector<const CorrectiveAction*> ActionTracker::Overdue(int CurrentWeek) const
{
	std::vector<const CorrectiveAction*> Out;
	for (const CorrectiveAction& A : Items)
	{
		if (ActionStatus(A, CurrentWeek) == EActionStatus::Overdue) Out.push_back(&A);
	}
	return Out;
}

std::vector<const CorrectiveAction*> ActionTracker::Outstanding() const
{
	std::vector<const CorrectiveAction*> Out;
	for (const CorrectiveAction& A : Items)
	{
		if (!A.bDone) Out.push_back(&A);
	}
	return Out;
}
} // namespace rb
