#include "RbRiskModel.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace rb
{
const char* ToString(ERating Rating)
{
	switch (Rating)
	{
	case ERating::Low: return "Low";
	case ERating::Medium: return "Medium";
	case ERating::High: return "High";
	case ERating::Critical: return "Critical";
	}
	return "?";
}

const char* ToString(EControlEffectiveness E)
{
	switch (E)
	{
	case EControlEffectiveness::Ineffective: return "Ineffective";
	case EControlEffectiveness::PartiallyEffective: return "Partially effective";
	case EControlEffectiveness::Effective: return "Effective";
	}
	return "?";
}

const char* ToString(EAppetiteStatus S)
{
	switch (S)
	{
	case EAppetiteStatus::Within: return "Within appetite";
	case EAppetiteStatus::AtLimit: return "At limit";
	case EAppetiteStatus::Exceeds: return "Exceeds appetite";
	}
	return "?";
}

const char* ToString(ETreatment T)
{
	switch (T)
	{
	case ETreatment::Undecided: return "Undecided";
	case ETreatment::Avoid: return "Avoid";
	case ETreatment::Reduce: return "Reduce";
	case ETreatment::Transfer: return "Transfer";
	case ETreatment::Accept: return "Accept";
	}
	return "?";
}

const char* ToString(ERiskStatus S)
{
	switch (S)
	{
	case ERiskStatus::Open: return "Open";
	case ERiskStatus::InTreatment: return "In treatment";
	case ERiskStatus::Escalated: return "Escalated";
	case ERiskStatus::Closed: return "Closed";
	}
	return "?";
}

bool RiskMatrix::IsValid() const
{
	const int Max = Size * Size;
	const bool bBandsOk = Size >= 3 && Size <= 10 && MediumFrom > 1 && MediumFrom < HighFrom && HighFrom < CriticalFrom && CriticalFrom <= Max;
	const bool bOverridesOk = CellOverrides.empty() || static_cast<int>(CellOverrides.size()) == Max;
	return bBandsOk && bOverridesOk;
}

int RiskMatrix::Clamp(int Level) const
{
	return std::max(1, std::min(Size, Level));
}

int RiskMatrix::Score(int Likelihood, int Impact) const
{
	return Clamp(Likelihood) * Clamp(Impact);
}

ERating RiskMatrix::Rate(int Likelihood, int Impact) const
{
	const int L = Clamp(Likelihood);
	const int I = Clamp(Impact);
	if (!CellOverrides.empty() && static_cast<int>(CellOverrides.size()) == Size * Size)
	{
		return CellOverrides[(L - 1) * Size + (I - 1)];
	}
	const int S = L * I;
	if (S >= CriticalFrom) return ERating::Critical;
	if (S >= HighFrom) return ERating::High;
	if (S >= MediumFrom) return ERating::Medium;
	return ERating::Low;
}

EControlEffectiveness AssessControl(const Control& C)
{
	const int Weakest = std::min(std::max(0, std::min(3, C.DesignScore)), std::max(0, std::min(3, C.OperatingScore)));
	if (Weakest >= 3) return EControlEffectiveness::Effective;
	if (Weakest == 2) return EControlEffectiveness::PartiallyEffective;
	return EControlEffectiveness::Ineffective;
}

ResidualResult ComputeResidual(const RiskMatrix& M, int InherentL, int InherentI, const std::vector<Control>& Controls)
{
	int LReduction = 0;
	int IReduction = 0;
	for (const Control& C : Controls)
	{
		const int Steps = static_cast<int>(AssessControl(C));
		if (C.Type == EControlType::Preventive)
		{
			LReduction = std::max(LReduction, Steps);
		}
		else
		{
			IReduction = std::max(IReduction, Steps);
		}
	}
	ResidualResult R;
	R.Likelihood = M.Clamp(M.Clamp(InherentL) - LReduction);
	R.Impact = M.Clamp(M.Clamp(InherentI) - IReduction);
	return R;
}

int RiskAppetite::LimitFor(const std::string& Category) const
{
	for (const AppetiteRule& R : Rules)
	{
		if (R.Category == Category)
		{
			return R.MaxResidualScore;
		}
	}
	return DefaultMaxResidualScore;
}

EAppetiteStatus RiskAppetite::Check(const std::string& Category, int ResidualScore) const
{
	const int Limit = LimitFor(Category);
	if (ResidualScore > Limit) return EAppetiteStatus::Exceeds;
	if (ResidualScore == Limit) return EAppetiteStatus::AtLimit;
	return EAppetiteStatus::Within;
}

RiskRegister::RiskRegister(RiskMatrix InMatrix, RiskAppetite InAppetite)
	: MatrixCfg(std::move(InMatrix))
	, AppetiteCfg(std::move(InAppetite))
{
}

void RiskRegister::Recompute(RiskEntry& E) const
{
	E.InherentLikelihood = MatrixCfg.Clamp(E.InherentLikelihood);
	E.InherentImpact = MatrixCfg.Clamp(E.InherentImpact);
	const ResidualResult R = ComputeResidual(MatrixCfg, E.InherentLikelihood, E.InherentImpact, E.Controls);
	E.ResidualLikelihood = R.Likelihood;
	E.ResidualImpact = R.Impact;
	E.Appetite = AppetiteCfg.Check(E.Category, MatrixCfg.Score(R.Likelihood, R.Impact));
}

std::string RiskRegister::Add(RiskEntry Entry)
{
	if (Entry.Id.empty())
	{
		char Buf[16];
		do
		{
			std::snprintf(Buf, sizeof(Buf), "R-%03d", NextId++);
		} while (Find(Buf) != nullptr);
		Entry.Id = Buf;
	}
	else if (Find(Entry.Id) != nullptr)
	{
		return std::string();
	}
	Recompute(Entry);
	Items.push_back(std::move(Entry));
	return Items.back().Id;
}

void RiskRegister::RestoreEntry(RiskEntry Entry)
{
	Recompute(Entry);
	Items.push_back(std::move(Entry));
}

bool RiskRegister::Update(const RiskEntry& Entry)
{
	for (RiskEntry& E : Items)
	{
		if (E.Id == Entry.Id)
		{
			E = Entry;
			Recompute(E);
			return true;
		}
	}
	return false;
}

bool RiskRegister::Remove(const std::string& Id)
{
	const auto It = std::find_if(Items.begin(), Items.end(), [&](const RiskEntry& E) { return E.Id == Id; });
	if (It == Items.end())
	{
		return false;
	}
	Items.erase(It);
	return true;
}

const RiskEntry* RiskRegister::Find(const std::string& Id) const
{
	for (const RiskEntry& E : Items)
	{
		if (E.Id == Id)
		{
			return &E;
		}
	}
	return nullptr;
}

std::vector<const RiskEntry*> RiskRegister::Filter(const RiskFilter& F) const
{
	std::vector<const RiskEntry*> Out;
	for (const RiskEntry& E : Items)
	{
		if (!F.Category.empty() && E.Category != F.Category) continue;
		if (!F.Owner.empty() && E.Owner != F.Owner) continue;
		if (F.bFilterStatus && E.Status != F.Status) continue;
		if (F.bFilterMinRating && MatrixCfg.Rate(E.ResidualLikelihood, E.ResidualImpact) < F.MinResidualRating) continue;
		if (F.bOnlyOutsideAppetite && E.Appetite != EAppetiteStatus::Exceeds) continue;
		Out.push_back(&E);
	}
	return Out;
}

static std::string Lower(const std::string& S)
{
	std::string R = S;
	std::transform(R.begin(), R.end(), R.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
	return R;
}

std::vector<const RiskEntry*> RiskRegister::Search(const std::string& Query) const
{
	std::vector<const RiskEntry*> Out;
	const std::string Q = Lower(Query);
	for (const RiskEntry& E : Items)
	{
		const std::string Hay = Lower(E.Id + "\n" + E.Title + "\n" + E.Cause + "\n" + E.Event + "\n" + E.Consequence + "\n" + E.Owner);
		if (Q.empty() || Hay.find(Q) != std::string::npos)
		{
			Out.push_back(&E);
		}
	}
	return Out;
}
} // namespace rb
