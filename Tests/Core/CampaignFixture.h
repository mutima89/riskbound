// Small hand-built campaign used by the core tests (independent of the shipped Data/ missions).
#pragma once

#include "RbCampaign.h"
#include "RbGameState.h"

namespace rbtest
{
inline rb::MissionOptionDef Opt(const char* Id, int Score, bool bCorrect = false)
{
	rb::MissionOptionDef O;
	O.Id = Id;
	O.Label = std::string("Option ") + Id;
	O.Feedback = std::string("Feedback ") + Id;
	O.Score = Score;
	O.bCorrect = bCorrect;
	return O;
}

inline rb::MissionQuestionDef Single(const char* Id, rb::ECompetency C, int Weight, std::vector<rb::MissionOptionDef> Options)
{
	rb::MissionQuestionDef Q;
	Q.Id = Id;
	Q.Prompt = std::string("Prompt ") + Id;
	Q.Kind = rb::EQuestionKind::Single;
	Q.Competency = C;
	Q.Weight = Weight;
	Q.Options = std::move(Options);
	return Q;
}

// Four questions, one per level-2 promotion competency, so a mission gives evidence for each.
inline rb::MissionDef BasicMission(const char* Id, std::vector<std::string> Requires)
{
	using rb::ECompetency;
	rb::MissionDef M;
	M.Id = Id;
	M.Title = std::string("Title ") + Id;
	M.Department = "Operations";
	M.Briefing = std::string("Briefing ") + Id;
	M.MinLevel = 2;
	M.RequiresPassed = std::move(Requires);
	M.Evidence = {{"ev1", "Evidence one", "Body one", true}, {"ev2", "Evidence two", "Body two", false}};
	M.Questions = {
		Single("identify", ECompetency::RiskIdentification, 25, {Opt("good", 90), Opt("weak", 30)}),
		Single("assess", ECompetency::RiskAssessment, 25, {Opt("good", 85), Opt("weak", 35)}),
		Single("controls", ECompetency::ControlEvaluation, 25, {Opt("good", 80), Opt("weak", 30)}),
		Single("explain", ECompetency::Communication, 25, {Opt("good", 80), Opt("weak", 20)}),
	};
	M.PassMark = 60;
	M.Xp = 120;
	M.WeeksToComplete = 2;
	M.Lesson = std::string("Lesson ") + Id;
	return M;
}

inline std::vector<rb::MissionAnswer> AllAnswers(const rb::MissionDef& M, const char* Pick)
{
	std::vector<rb::MissionAnswer> A;
	for (const rb::MissionQuestionDef& Q : M.Questions) A.push_back({Q.Id, {Pick}});
	return A;
}

// M002 -> M003 chain, a crisis opened by an M002 consequence, a level-3 mission.
inline rb::MissionCatalogue TestCatalogue()
{
	rb::MissionCatalogue C;
	rb::MissionDef M2 = BasicMission("M002", {"M001"});
	// The deferral choice: scheduled breakdown that opens the crisis if it happens.
	rb::MissionOptionDef Defer = Opt("defer", 20);
	Defer.SetsFlags = {"m002_deferred"};
	rb::ConsequenceDef Fire;
	Fire.Id = "depot_fire";
	Fire.Description = "Depot fire after the deferred electrical inspection";
	Fire.DelayWeeks = 3;
	Fire.Probability = 1.0;
	Fire.Effects = {{rb::EOrgMetric::Reputation, -6}, {rb::EOrgMetric::Cash, -150000}};
	Fire.FlagIfOccurs = "depot_fire";
	Fire.MitigatingFlag = "fire_drill";
	Fire.MitigatedProbabilityFactor = 0.0;
	Defer.Schedules = {Fire};
	M2.Questions[2].Options.push_back(Defer);
	C.Missions.push_back(M2);

	rb::MissionDef M3 = BasicMission("M003", {"M002"});
	M3.Context = {{"m002_deferred", false, false, rb::EOrgMetric::StakeholderTrust, true, 0.0, "Operations still has not booked the inspection you deferred."}};
	C.Missions.push_back(M3);

	rb::MissionDef Crisis = BasicMission("C901", {});
	Crisis.Kind = rb::EMissionKind::Crisis;
	Crisis.RequiresFlags = {"depot_fire"};
	C.Missions.push_back(Crisis);

	rb::MissionDef M4 = BasicMission("M004", {"M003"});
	M4.MinLevel = 3;
	C.Missions.push_back(M4);
	return C;
}

// A new game where Mission 001 has been passed with solid evidence for the four level-2 competencies.
inline rb::GameState AfterM001()
{
	using rb::ECompetency;
	rb::GameState G = rb::NewGame("Test", rb::EDifficulty::Professional, 5);
	for (ECompetency C : {ECompetency::RiskIdentification, ECompetency::RiskAssessment, ECompetency::ControlEvaluation, ECompetency::Communication})
	{
		G.Career.ApplyAssessment({C, 80});
	}
	G.Career.RecordMission("M001", true, 100);
	return G;
}

inline rb::EMissionError Play(rb::GameState& G, const rb::MissionDef& M, const std::vector<rb::MissionAnswer>& A, rb::MissionDebrief* D = nullptr)
{
	const rb::EMissionError E = rb::StartMission(G, M);
	if (E != rb::EMissionError::None) return E;
	for (const rb::MissionEvidenceDef& Ev : M.Evidence) rb::ViewMissionEvidence(G, M, Ev.Id);
	return rb::SubmitMission(G, M, A, D);
}
} // namespace rbtest
