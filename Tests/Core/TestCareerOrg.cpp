#include "RbTest.h"

#include "RbCareer.h"
#include "RbOrganization.h"

using namespace rb;

TEST(Career_LadderHasTenLevelsAndAuthorityGrows)
{
	const auto& L = CareerLadder();
	CHECK_EQ(L.size(), size_t(10));
	CHECK(L.front().Title.find("Intern") != std::string::npos);
	CHECK(L.back().Title == "Chief Risk Officer");
	for (size_t i = 1; i < L.size(); ++i)
	{
		CHECK(L[i].DecisionAuthority > L[i - 1].DecisionAuthority);
		CHECK_EQ(L[i].Level, static_cast<int>(i) + 1);
	}
	CHECK_EQ(AdvancedPathways().size(), size_t(5));
}

TEST(Career_XpAloneNeverPromotes)
{
	CareerState C;
	C.Level = 2;
	C.Xp = 10000000;
	const PromotionCheck P = C.CheckPromotion();
	CHECK(!P.bEligible);
	CHECK(!P.UnmetGates.empty());
	CHECK(!C.TryPromote());
	CHECK_EQ(C.Level, 2);
}

TEST(Career_PromotionRequiresCompetenceEvidenceAndMissions)
{
	CareerState C;
	C.Level = 2;
	const CareerLevelDef& Def = LevelDef(2);
	for (const auto& Req : Def.PromotionRequirements)
	{
		C.ApplyAssessment({Req.Competency, 90}); // one piece of evidence only
	}
	for (const auto& M : Def.RequiredMissions) C.RecordMission(M, true, 0);
	CHECK(!C.CheckPromotion().bEligible); // needs 2 assessments per competency
	for (const auto& Req : Def.PromotionRequirements) C.ApplyAssessment({Req.Competency, 90});
	CHECK(C.CheckPromotion().bEligible);
	CHECK(C.TryPromote());
	CHECK_EQ(C.Level, 3);
}

TEST(Career_AssessmentBlendingAndMissionRecords)
{
	CareerState C;
	C.ApplyAssessment({ECompetency::Judgement, 100});
	CHECK_EQ(C.Get(ECompetency::Judgement).Score, 100);
	C.ApplyAssessment({ECompetency::Judgement, 0});
	CHECK_EQ(C.Get(ECompetency::Judgement).Score, 60); // 100*0.6 + 0*0.4
	C.ApplyAssessment({ECompetency::Judgement, 500});  // clamped
	CHECK_EQ(C.Get(ECompetency::Judgement).Score, 76);
	C.RecordMission("M001", false, 50);
	C.RecordMission("M001", true, 50);
	C.RecordMission("M001", true, 50);
	CHECK_EQ(C.PassedMissions.size(), size_t(1));
	CHECK_EQ(C.AttemptedMissions.size(), size_t(1));
	CHECK_EQ(C.Xp, 150);
}

TEST(Org_ProfilesAndClamping)
{
	CHECK_EQ(OrganizationProfiles().size(), size_t(3));
	OrganizationState O;
	O.Apply({EOrgMetric::Reputation, 500});
	CHECK_NEAR(O.Get(EOrgMetric::Reputation), 100.0, 0.0);
	const double Cash = O.Get(EOrgMetric::Cash);
	O.Apply({EOrgMetric::Cash, -1e9});
	CHECK_NEAR(O.Get(EOrgMetric::Cash), Cash - 1e9, 1e-3); // money is not clamped
}

static PendingConsequence MakeIncident(double P)
{
	PendingConsequence C;
	C.Id = "inc";
	C.SourceDecisionId = "d1";
	C.Description = "failure";
	C.TriggerWeek = 4;
	C.Probability = P;
	C.EffectsIfOccurs = {{EOrgMetric::Reputation, -20}};
	C.MitigatingFlag = "fixed";
	C.MitigatedProbabilityFactor = 1.0;
	C.MitigatedSeverityFactor = 0.5;
	return C;
}

TEST(Org_DeferredConsequenceFiresOnScheduleAndMitigates)
{
	OrganizationState A;
	A.RecordDecision({"d1", "M001", "ignored", 0, 20, "", {}});
	A.Schedule(MakeIncident(1.0));
	CHECK(A.AdvanceWeeks(3).empty());
	const auto R = A.AdvanceWeeks(1);
	CHECK_EQ(R.size(), size_t(1));
	CHECK(R[0].bOccurred);
	CHECK(!R[0].bMitigated);
	CHECK_NEAR(A.Get(EOrgMetric::Reputation), 62 - 20, 1e-9);
	CHECK(A.Decisions[0].Outcome.find("occurred") != std::string::npos);

	OrganizationState B;
	B.SetFlag("fixed");
	B.Schedule(MakeIncident(1.0));
	B.AdvanceWeeks(10);
	CHECK(B.History[0].bMitigated);
	CHECK_NEAR(B.Get(EOrgMetric::Reputation), 62 - 10, 1e-9);
	CHECK(B.Pending.empty());
}

TEST(Org_RandomEventsReproducibleFromSeed)
{
	int Occurred1 = 0, Occurred2 = 0;
	for (int Run = 0; Run < 2; ++Run)
	{
		OrganizationState O;
		O.EventSeed = 1234;
		for (int i = 0; i < 200; ++i)
		{
			PendingConsequence C = MakeIncident(0.5);
			C.TriggerWeek = i;
			O.Schedule(C);
		}
		O.AdvanceWeeks(300);
		int N = 0;
		for (const auto& H : O.History) N += H.bOccurred ? 1 : 0;
		(Run == 0 ? Occurred1 : Occurred2) = N;
	}
	CHECK_EQ(Occurred1, Occurred2);
	CHECK(Occurred1 > 70 && Occurred1 < 130); // ~Binomial(200, 0.5)
}
