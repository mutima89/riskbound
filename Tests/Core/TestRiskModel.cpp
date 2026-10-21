#include "RbTest.h"

#include "RbRiskModel.h"

using namespace rb;

TEST(Matrix_DefaultBandsAndClamping)
{
	RiskMatrix M;
	CHECK(M.IsValid());
	CHECK_EQ(M.Score(4, 4), 16);
	CHECK(M.Rate(4, 4) == ERating::Critical);
	CHECK(M.Rate(4, 3) == ERating::High);
	CHECK(M.Rate(2, 3) == ERating::Medium);
	CHECK(M.Rate(1, 4) == ERating::Low);
	CHECK_EQ(M.Score(9, -3), 5); // clamped to 5 x 1
}

TEST(Matrix_CellOverridesAndValidation)
{
	RiskMatrix M;
	M.Size = 3;
	M.MediumFrom = 3;
	M.HighFrom = 6;
	M.CriticalFrom = 9;
	CHECK(M.IsValid());
	M.CellOverrides.assign(9, ERating::Low);
	M.CellOverrides[0 * 3 + 2] = ERating::High; // rare but severe weighted up
	CHECK(M.Rate(1, 3) == ERating::High);
	M.CellOverrides.resize(4);
	CHECK(!M.IsValid());
	RiskMatrix Bad;
	Bad.HighFrom = 4; // below MediumFrom
	CHECK(!Bad.IsValid());
}

TEST(Control_EffectivenessIsLimitedByWeakerDimension)
{
	CHECK(AssessControl({"a", EControlType::Preventive, 3, 3}) == EControlEffectiveness::Effective);
	CHECK(AssessControl({"b", EControlType::Preventive, 3, 1}) == EControlEffectiveness::Ineffective);
	CHECK(AssessControl({"c", EControlType::Preventive, 2, 3}) == EControlEffectiveness::PartiallyEffective);
	CHECK(AssessControl({"d", EControlType::Preventive, 9, 9}) == EControlEffectiveness::Effective);
}

TEST(Residual_StrongestControlPerDimensionNoStacking)
{
	RiskMatrix M;
	const std::vector<Control> Cs = {
		{"p1", EControlType::Preventive, 3, 3}, // -2 L
		{"p2", EControlType::Preventive, 3, 3}, // would stack to -4 if additive
		{"d1", EControlType::Detective, 2, 2},  // -1 I
	};
	const ResidualResult R = ComputeResidual(M, 5, 5, Cs);
	CHECK_EQ(R.Likelihood, 3);
	CHECK_EQ(R.Impact, 4);
	const ResidualResult Floor = ComputeResidual(M, 1, 1, Cs);
	CHECK_EQ(Floor.Likelihood, 1);
	CHECK_EQ(Floor.Impact, 1);
}

TEST(Appetite_WithinAtLimitExceeds)
{
	RiskAppetite A;
	A.DefaultMaxResidualScore = 9;
	A.Rules = {{"Compliance", 4}};
	CHECK(A.Check("Compliance", 3) == EAppetiteStatus::Within);
	CHECK(A.Check("Compliance", 4) == EAppetiteStatus::AtLimit);
	CHECK(A.Check("Compliance", 5) == EAppetiteStatus::Exceeds);
	CHECK(A.Check("Other", 9) == EAppetiteStatus::AtLimit);
}

static RiskEntry MakeRisk(const char* Title, const char* Cat, int L, int I)
{
	RiskEntry E;
	E.Title = Title;
	E.Category = Cat;
	E.Owner = "Finance";
	E.InherentLikelihood = L;
	E.InherentImpact = I;
	return E;
}

TEST(Register_CrudFilterSearch)
{
	RiskAppetite A;
	A.Rules = {{"Operational", 8}};
	RiskRegister Reg(RiskMatrix{}, A);
	const std::string Id1 = Reg.Add(MakeRisk("Supplier insolvency", "Operational", 4, 4));
	const std::string Id2 = Reg.Add(MakeRisk("FX exposure", "Financial", 2, 2));
	CHECK(Id1 == "R-001");
	CHECK(Id2 == "R-002");
	CHECK(Reg.Add([] { RiskEntry E; E.Id = "R-001"; return E; }()).empty()); // duplicate id rejected

	const RiskEntry* E1 = Reg.Find(Id1);
	CHECK(E1 != nullptr);
	CHECK(E1->Appetite == EAppetiteStatus::Exceeds);

	RiskEntry Edit = *E1;
	Edit.Controls = {{"Dual sourcing", EControlType::Preventive, 3, 3}};
	CHECK(Reg.Update(Edit));
	CHECK_EQ(Reg.Find(Id1)->ResidualLikelihood, 2);
	CHECK(Reg.Find(Id1)->Appetite == EAppetiteStatus::AtLimit);

	RiskFilter F;
	F.Category = "Financial";
	CHECK_EQ(Reg.Filter(F).size(), size_t(1));
	RiskFilter F2;
	F2.bFilterMinRating = true;
	F2.MinResidualRating = ERating::Medium;
	CHECK_EQ(Reg.Filter(F2).size(), size_t(1));
	CHECK_EQ(Reg.Search("SUPPLIER").size(), size_t(1));
	CHECK_EQ(Reg.Search("").size(), size_t(2));
	CHECK(Reg.Remove(Id2));
	CHECK(!Reg.Remove(Id2));
	CHECK_EQ(Reg.Entries().size(), size_t(1));
}
