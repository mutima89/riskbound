#include "RbTest.h"

#include "RbActions.h"
#include "RbGameState.h"
#include "RbMission001.h"

using namespace rb;

static CorrectiveAction MakeAction(const std::string& Title, int Created, int Due)
{
	CorrectiveAction A;
	A.RiskId = "R-014";
	A.Title = Title;
	A.Owner = "Daniel Reyes";
	A.CreatedWeek = Created;
	A.DueWeek = Due;
	return A;
}

TEST(Actions_StatusDerivedFromWeek)
{
	ActionTracker T;
	const std::string Id = T.Create(MakeAction("Service compressor", 0, 4));
	CHECK(Id == "A-001");
	const CorrectiveAction* A = T.Find(Id);
	CHECK(ActionStatus(*A, 0) == EActionStatus::Open);
	CHECK(ActionStatus(*A, 4) == EActionStatus::Open);    // due at the end of week 4
	CHECK(ActionStatus(*A, 5) == EActionStatus::Overdue); // first week past due
	CHECK(T.Start(Id));
	CHECK(ActionStatus(*T.Find(Id), 3) == EActionStatus::InProgress);
	CHECK(ActionStatus(*T.Find(Id), 5) == EActionStatus::Overdue); // started but late is still overdue
	CHECK(!T.Close(Id, 6, ""));                                     // evidence is mandatory
	CHECK(T.Close(Id, 6, "Service certificate CS-2291"));
	CHECK(ActionStatus(*T.Find(Id), 9) == EActionStatus::Done);
	CHECK(IsCompletedLate(*T.Find(Id)));                            // closed in week 6, due week 4
	CHECK(!T.Close(Id, 7, "again"));
}

TEST(Actions_CreateUpdateValidate)
{
	ActionTracker T;
	CorrectiveAction NoOwner = MakeAction("x", 0, 2);
	NoOwner.Owner.clear();
	CHECK(T.Create(NoOwner).empty());
	CHECK(T.Create(MakeAction("due before created", 5, 4)).empty());
	CHECK(T.Create(MakeAction("first", 0, 2)) == "A-001");
	CHECK(T.Create(MakeAction("second", 0, 8)) == "A-002");
	CorrectiveAction Explicit = MakeAction("explicit", 0, 3);
	Explicit.Id = "A-002";
	CHECK(T.Create(Explicit).empty()); // duplicate id

	CorrectiveAction Edit = *T.Find("A-002");
	Edit.DueWeek = 1;
	CHECK(T.Update(Edit));
	CHECK_EQ(T.Find("A-002")->DueWeek, 1);
	REQUIRE(T.Overdue(2).size() == size_t(1)); // A-002 (due 1); A-001 due 2 is not yet overdue
	CHECK(T.Overdue(2)[0]->Id == "A-002");
	CHECK_EQ(T.Overdue(3).size(), size_t(2));
	CHECK(T.Close("A-001", 2, "Photo of service log"));
	Edit.Id = "A-001";
	CHECK(!T.Update(Edit)); // done actions are frozen
	CHECK_EQ(T.Outstanding().size(), size_t(1));
	CHECK_EQ(T.ForRisk("R-014").size(), size_t(2));
	CHECK(T.ForRisk("R-999").empty());
}

TEST(Actions_Mission001TreatmentCreatesActions)
{
	GameState G = NewGame("Test", EDifficulty::Professional, 7);
	G.Mission001.MeetManager();
	for (const m001::EvidenceItem& E : m001::Evidence()) G.Mission001.ViewEvidence(E.Id);

	m001::Submission S;
	S.MissingInfo = {"compressor_failure_history"};
	S.InherentLikelihood = 4;
	S.InherentImpact = 4;
	S.ControlRatings = {EControlEffectiveness::Ineffective, EControlEffectiveness::PartiallyEffective, EControlEffectiveness::Ineffective};
	S.ResidualLikelihood = 4;
	S.ResidualImpact = 3;
	S.TreatmentId = "repair_and_escalation";
	S.ActionOwnerId = "ops_site_manager";
	S.RationaleTagIds = {"alarm_ignored"};

	m001::Submission Incomplete = S;
	Incomplete.TreatmentId.clear();
	CHECK(SubmitMission001(G, Incomplete) != m001::ESubmitError::None);
	CHECK(G.Actions.All().empty()); // no actions without a valid submission

	CHECK(SubmitMission001(G, S) == m001::ESubmitError::None);
	REQUIRE(G.Actions.All().size() == size_t(3));
	const CorrectiveAction& First = G.Actions.All()[0];
	CHECK(First.RiskId == "R-014");
	CHECK(First.Owner == "Daniel Reyes, Northgate Site Manager (Operations)");
	CHECK_EQ(First.DueWeek, 2);
	CHECK_EQ(G.Actions.All()[1].DueWeek, 4);
	CHECK_EQ(G.Actions.All()[2].DueWeek, 4);

	AdvanceGameWeeks(G, 3);
	CHECK_EQ(G.Actions.Overdue(G.Org.Week).size(), size_t(1)); // only the 2-week service

	S.TreatmentId = "accept_monitor";
	const std::vector<CorrectiveAction> Accept = m001::TreatmentActions(S, 10);
	REQUIRE(Accept.size() == size_t(1));
	CHECK_EQ(Accept[0].DueWeek, 23);
	S.TreatmentId = "no_such_treatment";
	CHECK(m001::TreatmentActions(S, 0).empty());
}
