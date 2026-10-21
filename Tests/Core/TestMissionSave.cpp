#include "RbTest.h"

#include "RbGameState.h"
#include "RbMission001.h"
#include "RbSave.h"

#include <cinttypes>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace rb;
using namespace rb::m001;

static void PlayInvestigation(MissionState& M, bool bAllEvidence)
{
	M.MeetManager();
	M.ViewEvidence("incident_report");
	M.ViewEvidence("risk_register_R014");
	if (bAllEvidence)
	{
		for (const EvidenceItem& E : Evidence()) M.ViewEvidence(E.Id);
	}
}

static Submission BestSubmission()
{
	Submission S;
	S.MissingInfo = {"compressor_failure_history", "after_hours_escalation", "alarm_threshold_config"};
	S.InherentLikelihood = 4;
	S.InherentImpact = 4;
	S.ControlRatings = {EControlEffectiveness::Ineffective, EControlEffectiveness::PartiallyEffective, EControlEffectiveness::Ineffective};
	S.ResidualLikelihood = 4;
	S.ResidualImpact = 3;
	S.TreatmentId = "repair_and_escalation";
	S.ActionOwnerId = "ops_site_manager";
	S.RationaleTagIds = {"alarm_ignored", "service_overdue", "penalty_exposure", "outside_appetite"};
	S.RationaleText = "Controls exist on paper but are not operating.";
	return S;
}

static Submission PoorSubmission()
{
	Submission S;
	S.MissingInfo = {"client_penalty_terms", "fleet_fuel_costs"};
	S.InherentLikelihood = 2;
	S.InherentImpact = 3;
	S.ControlRatings = {EControlEffectiveness::Effective, EControlEffectiveness::Effective, EControlEffectiveness::Effective};
	S.ResidualLikelihood = 1;
	S.ResidualImpact = 2;
	S.TreatmentId = "accept_monitor";
	S.ActionOwnerId = "risk_team";
	S.RationaleTagIds = {"site_is_new", "noise_complaints"};
	return S;
}

TEST(M001_ContentIsConsistent)
{
	CHECK_EQ(Evidence().size(), size_t(7));
	CHECK_EQ(TreatmentOptions().size(), size_t(4));
	CHECK(ValidateScenarios(BaselineScenarios(), true));
	CHECK_NEAR(ExpectedLoss(BaselineScenarios()), 117500.0, 1e-6);
	// The engine's view of the evidence must match the rubric's answer key.
	const ResidualResult R = ComputeResidual(RiskMatrix{}, 4, 4, ExistingControls());
	CHECK_EQ(R.Likelihood, 4);
	CHECK_EQ(R.Impact, 3);
	// Recommended option is the only one with positive NPV; plant replacement over-spends.
	CHECK(AppraiseTreatment(AppraisalInputFor("repair_and_escalation")).bCostEffective);
	CHECK(!AppraiseTreatment(AppraisalInputFor("replace_plant")).bCostEffective);
	CHECK(!AppraiseTreatment(AppraisalInputFor("increase_insurance")).bCostEffective);
	CHECK(AppraiseTreatment(AppraisalInputFor("replace_plant")).AnnualRiskReduction >
		AppraiseTreatment(AppraisalInputFor("repair_and_escalation")).AnnualRiskReduction);
}

TEST(M001_SubmissionGating)
{
	MissionState M;
	CHECK(M.Validate(BestSubmission()) == ESubmitError::ManagerNotMet);
	M.MeetManager();
	CHECK(M.Validate(BestSubmission()) == ESubmitError::IncidentReportNotReviewed);
	CHECK(M.ViewEvidence("incident_report"));
	CHECK(!M.ViewEvidence("no_such_doc"));
	CHECK(M.Validate(BestSubmission()) == ESubmitError::RegisterNotInspected);
	M.ViewEvidence("risk_register_R014");
	Submission Partial = BestSubmission();
	Partial.TreatmentId.clear();
	CHECK(M.Validate(Partial) == ESubmitError::IncompleteSubmission);
	CHECK(M.Validate(BestSubmission()) == ESubmitError::None);
	CHECK_EQ(M.CurrentObjective(), static_cast<int>(EObjective::ReviewControlEvidence));
}

TEST(M001_BestPlayScoresFullAndPasses)
{
	MissionState M;
	PlayInvestigation(M, true);
	const Debrief D = Score(BestSubmission(), M.ViewedEvidence);
	CHECK_EQ(D.OverallScore, 100);
	CHECK(D.bPassed);
	CHECK(D.Improvements.empty());
	CHECK(D.ChosenAppraisal.bCostEffective);
	CHECK(D.BaselineExpectedLoss < D.BaselineWorstCase);
}

TEST(M001_PoorPlayFailsWithImprovementAreas)
{
	MissionState M;
	PlayInvestigation(M, false);
	const Debrief D = Score(PoorSubmission(), M.ViewedEvidence);
	CHECK(!D.bPassed);
	CHECK(D.OverallScore < 30);
	CHECK(D.Improvements.size() >= 5);
}

TEST(M001_SubmitUpdatesCareerOrgRegister)
{
	GameState G = NewGame("Test", EDifficulty::Professional, 7);
	PlayInvestigation(G.Mission001, true);
	const double CashBefore = G.Org.Get(EOrgMetric::Cash);
	CHECK(G.Mission001.Submit(BestSubmission(), G.Difficulty, G.Career, G.Org, G.Register) == ESubmitError::None);
	CHECK(G.Mission001.Submit(BestSubmission(), G.Difficulty, G.Career, G.Org, G.Register) == ESubmitError::AlreadySubmitted);
	CHECK(G.Career.HasPassed(MissionId));
	CHECK_EQ(G.Career.Get(ECompetency::RiskAssessment).Score, 100);
	CHECK(G.Career.Level == 2); // one mission is never enough to promote
	CHECK_NEAR(G.Org.Get(EOrgMetric::Cash), CashBefore - 18000, 1e-6);
	CHECK(G.Org.HasFlag("northgate_controls_fixed"));
	CHECK_EQ(G.Org.Pending.size(), size_t(1));
	const RiskEntry* R = G.Register.Find("R-014");
	CHECK(R != nullptr);
	CHECK(R->Treatment == ETreatment::Reduce);
	CHECK(R->Appetite == EAppetiteStatus::Exceeds); // still above appetite until controls are fixed
	CHECK_EQ(G.Mission001.CurrentObjective(), ObjectiveCount);
	const auto New = EvaluateAchievements(G);
	CHECK(New.size() >= 5);
	CHECK(EvaluateAchievements(G).empty());
}

TEST(M001_GoodControlReducesLaterIncident)
{
	double LossGood = 0, LossBad = 0;
	int HitsGood = 0, HitsBad = 0;
	for (uint64_t Seed = 1; Seed <= 200; ++Seed)
	{
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			GameState G = NewGame("T", EDifficulty::Professional, Seed);
			PlayInvestigation(G.Mission001, true);
			G.Mission001.Submit(Pass == 0 ? BestSubmission() : PoorSubmission(), G.Difficulty, G.Career, G.Org, G.Register);
			const double Before = G.Org.Get(EOrgMetric::Cash);
			const auto R = G.Org.AdvanceWeeks(6);
			REQUIRE(R.size() == size_t(1));
			(Pass == 0 ? LossGood : LossBad) += Before - G.Org.Get(EOrgMetric::Cash);
			(Pass == 0 ? HitsGood : HitsBad) += R[0].bOccurred ? 1 : 0;
		}
	}
	CHECK(HitsGood < HitsBad);
	CHECK(LossGood < LossBad * 0.5);
	CHECK(HitsGood > 0); // good decisions can still have bad outcomes
}

// ---------------- persistence ----------------
static std::string TempDir()
{
	const auto P = std::filesystem::temp_directory_path() / "riskbound_tests";
	std::filesystem::remove_all(P);
	std::filesystem::create_directories(P);
	return P.string();
}

static std::string Slurp(const std::string& Path)
{
	std::ifstream F(Path, std::ios::binary);
	std::ostringstream S;
	S << F.rdbuf();
	return S.str();
}

static void Spit(const std::string& Path, const std::string& Text)
{
	std::ofstream F(Path, std::ios::binary | std::ios::trunc);
	F << Text;
}

static GameState MidGame()
{
	GameState G = NewGame("Ada \"Tab\"\tNew\nline", EDifficulty::Executive, 2026);
	PlayInvestigation(G.Mission001, true);
	G.Mission001.Submit(BestSubmission(), G.Difficulty, G.Career, G.Org, G.Register);
	EvaluateAchievements(G);
	G.LearningHistory = {"worked_example:residual_risk", "retrieval:control_design"};
	G.Settings.TextScale = 1.25;
	G.Checkpoint = "M001:debrief";
	return G;
}

TEST(Save_RoundTripIsExact)
{
	const GameState G = MidGame();
	const std::string Text = SerializeGame(G);
	GameState L;
	LoadResult Info;
	CHECK(DeserializeGame(Text, L, Info));
	CHECK(Info.Status == ELoadStatus::Ok);
	CHECK(SerializeGame(L) == Text);
	CHECK(L.ProfileName == G.ProfileName);
	CHECK_EQ(L.Mission001.Result.OverallScore, 100);
	CHECK(L.Register.Find("R-014") != nullptr);
}

TEST(Save_ResumeContinuesSameRandomSequence)
{
	GameState A = MidGame();
	GameState B;
	LoadResult Info;
	CHECK(DeserializeGame(SerializeGame(A), B, Info));
	A.Org.AdvanceWeeks(6);
	B.Org.AdvanceWeeks(6);
	CHECK(SerializeGame(A) == SerializeGame(B));
}

TEST(Save_FileWriteBackupAndRecovery)
{
	const std::string Dir = TempDir();
	const std::string Path = Dir + "/slot1.rbsave";
	GameState G = MidGame();
	GameState L;
	CHECK(LoadGameFromFile(Path, L).Status == ELoadStatus::NotFound);

	std::string Err;
	CHECK(SaveGameToFile(Path, G, &Err));
	CHECK(!std::filesystem::exists(Path + ".tmp"));
	CHECK(!std::filesystem::exists(BackupPath(Path))); // nothing to rotate on first save
	G.Checkpoint = "second";
	CHECK(SaveGameToFile(Path, G, &Err));
	CHECK(std::filesystem::exists(BackupPath(Path)));

	// Corrupt the primary by flipping one byte: checksum must catch it, backup must load.
	std::string Text = Slurp(Path);
	Text[Text.size() / 2] ^= 0x01;
	Spit(Path, Text);
	LoadResult R = LoadGameFromFile(Path, L);
	CHECK(R.Status == ELoadStatus::RecoveredFromBackup);
	CHECK(L.Checkpoint == "M001:debrief");

	// Saving over a corrupt primary must not replace the good backup with the corrupt file.
	const std::string GoodBackup = Slurp(BackupPath(Path));
	G.Checkpoint = "third";
	CHECK(SaveGameToFile(Path, G, &Err));
	CHECK(Slurp(BackupPath(Path)) == GoodBackup);
	CHECK(std::filesystem::exists(Path + ".corrupt"));
	CHECK(LoadGameFromFile(Path, L).Status == ELoadStatus::Ok);
	CHECK(L.Checkpoint == "third");

	// Truncated primary and garbage backup: reported as corrupt, not silently replaced.
	Spit(Path, Slurp(Path).substr(0, 100));
	Spit(BackupPath(Path), "not a save");
	CHECK(LoadGameFromFile(Path, L).Status == ELoadStatus::Corrupt);
	std::filesystem::remove_all(Dir);
}

static std::string Resign(std::string Body)
{
	const size_t Pos = Body.rfind("checksum\t");
	Body = Body.substr(0, Pos);
	uint64_t H = 0xCBF29CE484222325ULL;
	for (unsigned char C : Body)
	{
		H ^= C;
		H *= 0x100000001B3ULL;
	}
	char Sum[32];
	std::snprintf(Sum, sizeof(Sum), "%016" PRIx64, H);
	return Body + "checksum\t" + Sum + "\n";
}

static void ReplaceLine(std::string& Text, const std::string& Prefix, const std::string& NewLine)
{
	const size_t P = Text.find("\n" + Prefix);
	const size_t E = Text.find('\n', P + 1);
	Text.replace(P + 1, E - P - 1, NewLine);
}

TEST(Save_MigratesV1AndRejectsFutureVersions)
{
	const GameState G = MidGame();
	std::string V1 = SerializeGame(G);
	ReplaceLine(V1, "version\t", "version\t1");
	ReplaceLine(V1, "profile.name\t", "player\tLegacy Player");
	ReplaceLine(V1, "profile.difficulty\t", "difficulty\thard");
	ReplaceLine(V1, "org.draws\t", "settings.gfx\t2");
	V1 = Resign(V1);
	GameState L;
	LoadResult Info;
	CHECK(DeserializeGame(V1, L, Info));
	CHECK(Info.bMigrated);
	CHECK_EQ(Info.LoadedVersion, 1);
	CHECK(L.ProfileName == "Legacy Player");
	CHECK(L.Difficulty == EDifficulty::Executive);
	CHECK(L.Org.EventDraws == 0);

	std::string Future = SerializeGame(G);
	ReplaceLine(Future, "version\t", "version\t99");
	Future = Resign(Future);
	CHECK(!DeserializeGame(Future, L, Info));
	CHECK(Info.Status == ELoadStatus::UnsupportedVersion);

	std::string BadEnum = SerializeGame(G);
	ReplaceLine(BadEnum, "profile.difficulty\t", "profile.difficulty\t7");
	CHECK(!DeserializeGame(Resign(BadEnum), L, Info));
}

TEST(Save_V3RoundTripsActionsKrisAndQuantification)
{
	GameState G = NewGame("V3", EDifficulty::Professional, 31);
	PlayInvestigation(G.Mission001, true);
	CHECK(SubmitMission001(G, BestSubmission()) == ESubmitError::None);
	CHECK(G.Actions.Close("A-001", 1, "Service certificate\twith tab"));
	CHECK(SetQuantification(G, "R-014", BaselineScenarios()));
	AdvanceGameWeeks(G, 3);
	const std::string Text = SerializeGame(G);
	CHECK(Text.find("version\t" + std::to_string(CurrentSaveVersion) + "\n") != std::string::npos);
	GameState L;
	LoadResult Info;
	CHECK(DeserializeGame(Text, L, Info));
	CHECK(!Info.bMigrated);
	CHECK(SerializeGame(L) == Text);
	CHECK_EQ(L.Actions.All().size(), size_t(3));
	CHECK(L.Actions.Find("A-001")->bDone);
	CHECK_EQ(L.Actions.NextIdNumber(), G.Actions.NextIdNumber());
	REQUIRE(L.Kris.Series[0].History.size() == size_t(KriHistoryWeeksAtStart + 3));
	CHECK(L.Kris.Series[0].History.back().Status == G.Kris.Series[0].History.back().Status);
	CHECK_EQ(L.Kris.Series[0].History.back().AmberStreak, G.Kris.Series[0].History.back().AmberStreak);
	CHECK_EQ(L.Quantified.size(), size_t(1));

	std::string Done = Text; // a done action without evidence is invalid
	ReplaceLine(Done, "act.0.evidence\t", "act.0.evidence\t");
	CHECK(!DeserializeGame(Resign(Done), L, Info));
}

// Removes every line whose key starts with Prefix.
static void DropKeys(std::string& Text, const std::string& Prefix)
{
	size_t P = 0;
	while ((P = Text.find("\n" + Prefix, P)) != std::string::npos)
	{
		const size_t E = Text.find('\n', P + 1);
		Text.erase(P, E - P);
	}
}

TEST(Save_MigratesHandcraftedV2)
{
	GameState G = NewGame("V2", EDifficulty::Analyst, 77);
	PlayInvestigation(G.Mission001, true);
	G.Org.AdvanceWeeks(2); // decision taken in week 2
	CHECK(G.Mission001.Submit(BestSubmission(), G.Difficulty, G.Career, G.Org, G.Register) == ESubmitError::None);
	std::string V2 = SerializeGame(G);
	DropKeys(V2, "act.");
	DropKeys(V2, "kri.");
	DropKeys(V2, "quant.");
	ReplaceLine(V2, "version\t", "version\t2");
	CHECK(V2.find("\nact.") == std::string::npos);
	GameState L;
	LoadResult Info;
	CHECK(DeserializeGame(Resign(V2), L, Info));
	CHECK(Info.bMigrated);
	CHECK_EQ(Info.LoadedVersion, 2);
	REQUIRE(L.Actions.All().size() == size_t(3)); // recreated from the stored submission
	CHECK_EQ(L.Actions.All()[0].CreatedWeek, 2);
	CHECK_EQ(L.Actions.All()[0].DueWeek, 4);
	REQUIRE(L.Kris.Series.size() == size_t(5));
	CHECK_EQ(L.Kris.Series[0].History.back().Week, 2);
	CHECK(L.Quantified.empty());
	// Saving the migrated game writes v3, which loads without migration.
	GameState L3;
	CHECK(DeserializeGame(SerializeGame(L), L3, Info));
	CHECK(!Info.bMigrated);
}

// The only place the pre-v4 organisation and contractor names may appear: they must be migrated.
TEST(Save_MigratesV3OrganisationNamesToFictionalNames)
{
	GameState G = NewGame("V3", EDifficulty::Professional, 12);
	PlayInvestigation(G.Mission001, true);
	Submission S = BestSubmission();
	S.ActionOwnerId = "facilities_contractor";
	CHECK(SubmitMission001(G, S) == ESubmitError::None);
	CHECK(G.Org.ProfileId == "valtren");
	CHECK(G.Org.Name == "Valtren Coldline Logistics");
	REQUIRE(!G.Actions.All().empty());
	CHECK(G.Actions.All()[0].Owner == "Kelvarin Refrigeration Services (external maintenance contractor)");

	std::string V3 = SerializeGame(G);
	ReplaceLine(V3, "version\t", "version\t3");
	ReplaceLine(V3, "org.profile\t", "org.profile\tmeridian");
	ReplaceLine(V3, "org.name\t", "org.name\tMeridian Cold Chain Ltd");
	size_t P = 0;
	while ((P = V3.find("Kelvarin Refrigeration Services", P)) != std::string::npos) V3.replace(P, 31, "CoolServ Ltd");
	CHECK(V3.find("Kelvarin") == std::string::npos);

	GameState L;
	LoadResult Info;
	CHECK(DeserializeGame(Resign(V3), L, Info));
	CHECK(Info.bMigrated);
	CHECK_EQ(Info.LoadedVersion, 3);
	CHECK(L.Org.ProfileId == "valtren");
	CHECK(L.Org.Name == "Valtren Coldline Logistics");
	REQUIRE(!L.Actions.All().empty());
	CHECK(L.Actions.All()[0].Owner == "Kelvarin Refrigeration Services (external maintenance contractor)");
	const std::string Again = SerializeGame(L);
	CHECK(Again.find("Meridian") == std::string::npos);
	CHECK(Again.find("CoolServ") == std::string::npos);

	for (const auto& Old : {std::pair<const char*, const char*>{"aldmoor", "orrinbrook"}, {"kestrel", "quellard"}})
	{
		std::string Other = SerializeGame(NewGame("X", EDifficulty::Professional, 1, Old.second));
		ReplaceLine(Other, "version\t", "version\t3");
		ReplaceLine(Other, "org.profile\t", std::string("org.profile\t") + Old.first);
		CHECK(DeserializeGame(Resign(Other), L, Info));
		CHECK(L.Org.ProfileId == Old.second);
	}
}

// v4 had no campaign block and no consequence flags: loads with no campaign progress.
TEST(Save_MigratesV4WithoutCampaign)
{
	GameState G = NewGame("V4", EDifficulty::Professional, 9);
	PlayInvestigation(G.Mission001, true);
	CHECK(SubmitMission001(G, BestSubmission()) == ESubmitError::None);
	std::string V4 = SerializeGame(G);
	DropKeys(V4, "camp.");
	size_t P = 0;
	while ((P = V4.find(".setflag\t", P)) != std::string::npos)
	{
		const size_t Start = V4.rfind('\n', P);
		V4.erase(Start, V4.find('\n', P) - Start);
	}
	ReplaceLine(V4, "version\t", "version\t4");
	CHECK(V4.find("camp.") == std::string::npos);
	GameState L;
	LoadResult Info;
	CHECK(DeserializeGame(Resign(V4), L, Info));
	CHECK(Info.bMigrated);
	CHECK_EQ(Info.LoadedVersion, 4);
	CHECK(L.Campaign.Records.empty());
	CHECK(L.Campaign.ActiveMissionId.empty());
	REQUIRE(!L.Org.Pending.empty());
	CHECK(L.Org.Pending[0].FlagIfOccurs.empty());
	CHECK(L.Career.HasPassed("M001"));
	GameState L5;
	CHECK(DeserializeGame(SerializeGame(L), L5, Info));
	CHECK(!Info.bMigrated);
	CHECK_EQ(Info.LoadedVersion, CurrentSaveVersion);
}
