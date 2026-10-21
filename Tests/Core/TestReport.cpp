#include "RbTest.h"

#include "RbGameState.h"
#include "RbMission001.h"
#include "RbReport.h"

using namespace rb;

static RiskEntry Risk(const std::string& Title, const std::string& Category, int L, int I)
{
	RiskEntry E;
	E.Title = Title;
	E.Category = Category;
	E.Owner = "Owner | with pipe";
	E.InherentLikelihood = L;
	E.InherentImpact = I;
	return E; // no controls: residual = inherent
}

static m001::Submission Best()
{
	m001::Submission S;
	S.MissingInfo = {"compressor_failure_history", "after_hours_escalation", "alarm_threshold_config"};
	S.InherentLikelihood = 4;
	S.InherentImpact = 4;
	S.ControlRatings = {EControlEffectiveness::Ineffective, EControlEffectiveness::PartiallyEffective, EControlEffectiveness::Ineffective};
	S.ResidualLikelihood = 4;
	S.ResidualImpact = 3;
	S.TreatmentId = "repair_and_escalation";
	S.ActionOwnerId = "ops_site_manager";
	S.RationaleTagIds = {"alarm_ignored", "service_overdue", "penalty_exposure", "outside_appetite"};
	return S;
}

TEST(Report_OrderingAndSections)
{
	GameState G = NewGame("Test", EDifficulty::Professional, 5);
	CHECK(G.Register.Add(Risk("Fuel price spike", "Financial", 4, 4)) == "R-015"); // 16, limit 10: exceeds
	CHECK(G.Register.Add(Risk("Licence lapse", "Compliance", 1, 3)) == "R-016");    // 3, limit 4: within
	CHECK(G.Register.Add(Risk("Driver shortage", "Strategic", 4, 3)) == "R-017");   // 12, limit 12: at limit
	CHECK(G.Register.Add(Risk("Depot flood", "Operational", 3, 4)) == "R-018");     // 12, limit 8: exceeds
	RiskEntry Closed = Risk("Old system", "Cyber", 5, 5);
	Closed.Status = ERiskStatus::Closed;
	G.Register.Add(Closed);

	const ExecutiveReport R = BuildExecutiveReport(G, 3);
	CHECK_EQ(R.OpenRisks, 5); // R-014 + four open; the closed one is excluded
	REQUIRE(R.TopRisks.size() == size_t(3));
	CHECK(R.TopRisks[0].Id == "R-015");
	CHECK_EQ(R.TopRisks[0].ResidualScore, 16);
	CHECK(R.TopRisks[0].Rating == ERating::Critical);
	CHECK(R.TopRisks[1].Id == "R-017"); // tie at 12 broken by id
	CHECK(R.TopRisks[2].Id == "R-018");
	REQUIRE(R.AppetiteBreaches.size() == size_t(2)); // R-015 and R-018 (R-014 legacy residual is within)
	CHECK(R.AppetiteBreaches[0].Id == "R-015");
	CHECK(R.AppetiteBreaches[1].Id == "R-018");
	CHECK_EQ(R.AppetiteBreaches[1].AppetiteLimit, 8);
	CHECK_EQ(R.Kris.size(), size_t(5));
	CHECK(R.Decisions.empty());
	CHECK(R.Quantified.empty());

	const std::string Md = ReportToMarkdown(R);
	CHECK(Md.find("Owner / with pipe") != std::string::npos); // cells cannot break the table
	CHECK(Md.find("No decisions recorded yet.") != std::string::npos);
}

TEST(Report_ActionsQuantDecisions)
{
	GameState G = NewGame("Test", EDifficulty::Professional, 5);
	G.Mission001.MeetManager();
	for (const m001::EvidenceItem& E : m001::Evidence()) G.Mission001.ViewEvidence(E.Id);
	CHECK(SubmitMission001(G, Best()) == m001::ESubmitError::None);
	CHECK(G.Actions.Close("A-002", 0, "Rota published"));

	std::vector<LossScenario> Bad = m001::BaselineScenarios();
	Bad[0].AnnualProbability = 1.5;
	CHECK(!SetQuantification(G, "R-014", Bad));
	CHECK(!SetQuantification(G, "R-999", m001::BaselineScenarios()));
	CHECK(SetQuantification(G, "R-014", m001::BaselineScenarios()));

	ExecutiveReport R = BuildExecutiveReport(G);
	REQUIRE(R.OpenActions.size() == size_t(2)); // A-001 (due 2), A-003 (due 4); A-002 done
	CHECK(R.OpenActions[0].Id == "A-001");
	CHECK(R.OpenActions[1].Id == "A-003");
	CHECK(R.OverdueActions.empty());
	REQUIRE(R.Quantified.size() == size_t(1));
	CHECK_NEAR(R.Quantified[0].ExpectedLoss, 117500.0, 1e-6); // 0.5x40k + 0.15x290k + 0.03x1.8m
	CHECK_NEAR(R.Quantified[0].WorstCase, 1800000.0, 1e-6);
	REQUIRE(R.Decisions.size() == size_t(1));
	CHECK_EQ(R.Decisions[0].QualityScore, 100);
	CHECK(R.Decisions[0].Outcome == "Pending");

	std::string Md = ReportToMarkdown(R);
	CHECK(Md.find("| R-014 | 117,500 | 1,800,000 | 3 |") != std::string::npos);
	CHECK(Md.find("100/100 | Pending") != std::string::npos);

	AdvanceGameWeeks(G, 7); // consequence fires in week 6; actions due 2 and 4 are now late
	R = BuildExecutiveReport(G);
	CHECK_EQ(R.OverdueActions.size(), size_t(2));
	CHECK(R.OpenActions.empty());
	CHECK(R.Decisions[0].Outcome != "Pending");
	CHECK_EQ(R.Week, 7);
}
