#include "RbCampaign.h"

#include "RbGameState.h"
#include "RbQuant.h"
#include "RbRng.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace rb
{
const char* ToString(EMissionKind K)
{
	switch (K)
	{
	case EMissionKind::Mission: return "Mission";
	case EMissionKind::Crisis: return "Crisis";
	case EMissionKind::Capstone: return "Capstone";
	}
	return "?";
}

const char* ToString(EMissionError E)
{
	switch (E)
	{
	case EMissionError::None: return "None";
	case EMissionError::Unavailable: return "This mission is not available yet.";
	case EMissionError::AnotherMissionActive: return "Finish or set aside the mission in progress first.";
	case EMissionError::NotActive: return "Open the mission first.";
	case EMissionError::UnknownEvidence: return "Unknown evidence.";
	case EMissionError::RequiredEvidenceNotViewed: return "Review every required piece of evidence first.";
	case EMissionError::UnansweredQuestion: return "Answer every question.";
	case EMissionError::UnknownOption: return "Unknown answer.";
	case EMissionError::SingleNeedsOneOption: return "Choose exactly one option where asked.";
	}
	return "?";
}

const MissionDef* MissionCatalogue::Find(const std::string& Id) const
{
	for (const MissionDef& M : Missions)
	{
		if (M.Id == Id) return &M;
	}
	return nullptr;
}

// ---------------------------------------------------------------- validation

static bool Contains(const std::vector<std::string>& V, const std::string& S)
{
	return std::find(V.begin(), V.end(), S) != V.end();
}

static void ValidateEffects(const std::vector<MetricEffect>& Effects, const std::string& Where, std::vector<std::string>& Out)
{
	for (const MetricEffect& E : Effects)
	{
		const int M = static_cast<int>(E.Metric);
		if (M < 0 || M >= OrgMetricCount) Out.push_back(Where + ": unknown metric");
		else if (!std::isfinite(E.Delta)) Out.push_back(Where + ": effect is not a number");
		else if (IsIndexMetric(E.Metric) && std::fabs(E.Delta) > 50.0) Out.push_back(Where + ": index effect above 50 points");
	}
}

std::vector<std::string> ValidateMission(const MissionDef& M)
{
	std::vector<std::string> Out;
	const std::string P = (M.Id.empty() ? std::string("(no id)") : M.Id) + ": ";
	auto Err = [&](const std::string& Msg) { Out.push_back(P + Msg); };
	if (M.Id.empty()) Err("missing id");
	for (char Ch : M.Id)
	{
		if (!(std::isalnum(static_cast<unsigned char>(Ch)) || Ch == '_' || Ch == '-')) { Err("id has invalid characters"); break; }
	}
	if (M.Title.empty()) Err("missing title");
	if (M.Briefing.empty()) Err("missing briefing");
	if (M.MinLevel < 1 || M.MinLevel > static_cast<int>(CareerLadder().size())) Err("minLevel outside 1..10");
	if (M.PassMark < 1 || M.PassMark > 100) Err("passMark outside 1..100");
	if (M.WeeksToComplete < 0 || M.WeeksToComplete > 52) Err("weeks outside 0..52");
	if (M.Xp < 0) Err("negative xp");
	if (Contains(M.RequiresPassed, M.Id)) Err("requires itself");
	for (const ContextNoteDef& C : M.Context)
	{
		if (C.Text.empty()) Err("context note without text");
		if (C.Flag.empty() && !C.bUseMetric) Err("context note without a condition");
	}

	std::set<std::string> EvidenceIds;
	if (M.Evidence.empty()) Err("no evidence");
	for (const MissionEvidenceDef& E : M.Evidence)
	{
		if (E.Id.empty() || !EvidenceIds.insert(E.Id).second) Err("evidence id missing or duplicated: '" + E.Id + "'");
		if (E.Title.empty() || E.Body.empty()) Err("evidence '" + E.Id + "' needs a title and body");
	}

	std::set<std::string> QuestionIds;
	std::set<std::string> ConsequenceIds;
	int WeightSum = 0;
	if (M.Questions.empty()) Err("no questions");
	for (const MissionQuestionDef& Q : M.Questions)
	{
		const std::string QP = "question '" + Q.Id + "'";
		if (Q.Id.empty() || !QuestionIds.insert(Q.Id).second) Err(QP + ": id missing or duplicated");
		if (Q.Prompt.empty()) Err(QP + ": missing prompt");
		if (Q.Weight <= 0) Err(QP + ": weight must be positive");
		const int C = static_cast<int>(Q.Competency);
		if (C < 0 || C >= CompetencyCount) Err(QP + ": unknown competency");
		WeightSum += Q.Weight;
		if (Q.Options.size() < 2) Err(QP + ": needs at least two options");
		std::set<std::string> OptionIds;
		int Best = -1;
		int CorrectCount = 0;
		for (const MissionOptionDef& O : Q.Options)
		{
			const std::string OP = QP + " option '" + O.Id + "'";
			if (O.Id.empty() || !OptionIds.insert(O.Id).second) Err(OP + ": id missing or duplicated");
			if (O.Label.empty()) Err(OP + ": missing label");
			if (O.Score < 0 || O.Score > 100) Err(OP + ": score outside 0..100");
			if (O.Cost < 0.0 || !std::isfinite(O.Cost)) Err(OP + ": invalid cost");
			if (Q.Kind == EQuestionKind::Multi && O.Cost > 0.0) Err(OP + ": costs belong on single-choice questions");
			Best = std::max(Best, O.Score);
			CorrectCount += O.bCorrect ? 1 : 0;
			ValidateEffects(O.Effects, P + OP, Out);
			for (const std::string& F : O.SetsFlags)
			{
				if (F.empty()) Err(OP + ": empty flag");
			}
			for (const ConsequenceDef& Cq : O.Schedules)
			{
				const std::string CP = OP + " consequence '" + Cq.Id + "'";
				if (Cq.Id.empty() || !ConsequenceIds.insert(Cq.Id).second) Err(CP + ": id missing or duplicated");
				if (Cq.Description.empty()) Err(CP + ": missing description");
				if (Cq.DelayWeeks < 1) Err(CP + ": delay must be at least one week");
				if (!(Cq.Probability >= 0.0 && Cq.Probability <= 1.0)) Err(CP + ": probability outside 0..1");
				if (Cq.MitigatedProbabilityFactor < 0.0 || Cq.MitigatedSeverityFactor < 0.0) Err(CP + ": negative mitigation factor");
				ValidateEffects(Cq.Effects, P + CP, Out);
			}
		}
		if (Q.Kind == EQuestionKind::Single && Best < 80) Err(QP + ": no option scores 80 or more");
		if (Q.Kind == EQuestionKind::Multi && (CorrectCount == 0 || CorrectCount == static_cast<int>(Q.Options.size())))
		{
			Err(QP + ": multi-select needs at least one correct and one incorrect option");
		}
	}
	if (!M.Questions.empty() && WeightSum != 100) Err("question weights sum to " + std::to_string(WeightSum) + ", not 100");
	return Out;
}

std::vector<std::string> ValidateCatalogue(const MissionCatalogue& C)
{
	std::vector<std::string> Out;
	std::set<std::string> Ids;
	for (const MissionDef& M : C.Missions)
	{
		if (!Ids.insert(M.Id).second) Out.push_back(M.Id + ": duplicate mission id");
		if (M.Id == "M001") Out.push_back("M001: reserved for the hand-built vertical slice");
	}
	for (const MissionDef& M : C.Missions)
	{
		for (const std::string& E : ValidateMission(M)) Out.push_back(E);
		for (const std::string& R : M.RequiresPassed)
		{
			if (R != "M001" && !Ids.count(R)) Out.push_back(M.Id + ": requires unknown mission '" + R + "'");
		}
	}
	return Out;
}

// ---------------------------------------------------------------- availability

MissionAvailability CheckAvailability(const GameState& State, const MissionDef& M)
{
	MissionAvailability A;
	if (State.Career.HasPassed(M.Id)) A.Reasons.push_back("Already passed");
	if (State.Career.Level < M.MinLevel)
	{
		A.Reasons.push_back("Requires " + LevelDef(M.MinLevel).Title + " (level " + std::to_string(M.MinLevel) + ")");
	}
	for (const std::string& R : M.RequiresPassed)
	{
		if (!State.Career.HasPassed(R)) A.Reasons.push_back("Pass " + R + " first");
	}
	for (const std::string& F : M.RequiresFlags)
	{
		if (!State.Org.HasFlag(F)) A.Reasons.push_back("Not triggered yet");
	}
	for (const std::string& F : M.ExcludedByFlags)
	{
		if (State.Org.HasFlag(F)) A.Reasons.push_back("No longer relevant");
	}
	A.bAvailable = A.Reasons.empty();
	return A;
}

std::vector<const MissionDef*> AvailableMissions(const GameState& State, const MissionCatalogue& C)
{
	std::vector<const MissionDef*> Out;
	for (const MissionDef& M : C.Missions)
	{
		if (CheckAvailability(State, M).bAvailable) Out.push_back(&M);
	}
	return Out;
}

std::vector<int> OptionDisplayOrder(uint64_t GameSeed, const std::string& MissionId, const std::string& QuestionId, int OptionCount)
{
	// FNV-1a over the ids, mixed with the game seed: independent per question, fixed per save.
	uint64_t H = 1469598103934665603ULL;
	for (const std::string* S : {&MissionId, &QuestionId})
	{
		for (unsigned char C : *S)
		{
			H ^= C;
			H *= 1099511628211ULL;
		}
		H ^= 0xFF;
		H *= 1099511628211ULL;
	}
	Rng R(GameSeed ^ H);
	std::vector<int> Order;
	for (int i = 0; i < OptionCount; ++i) Order.push_back(i);
	for (int i = OptionCount - 1; i > 0; --i)
	{
		const int j = R.UniformInt(0, i);
		std::swap(Order[static_cast<size_t>(i)], Order[static_cast<size_t>(j)]);
	}
	return Order;
}

std::vector<std::string> BriefingFor(const GameState& State, const MissionDef& M)
{
	std::vector<std::string> Out = {M.Briefing};
	for (const ContextNoteDef& C : M.Context)
	{
		bool bApplies = true;
		if (!C.Flag.empty()) bApplies = State.Org.HasFlag(C.Flag) != C.bWhenAbsent;
		if (bApplies && C.bUseMetric)
		{
			const double V = State.Org.Get(C.Metric);
			bApplies = C.bBelow ? V < C.Threshold : V >= C.Threshold;
		}
		if (bApplies) Out.push_back(C.Text);
	}
	return Out;
}

// ---------------------------------------------------------------- play

EMissionError StartMission(GameState& State, const MissionDef& M)
{
	CampaignState& C = State.Campaign;
	if (C.ActiveMissionId == M.Id) return EMissionError::None; // resume
	if (!C.ActiveMissionId.empty()) return EMissionError::AnotherMissionActive;
	if (!CheckAvailability(State, M).bAvailable) return EMissionError::Unavailable;
	C.ActiveMissionId = M.Id;
	C.ViewedEvidence.clear();
	State.Checkpoint = M.Id + ":start";
	return EMissionError::None;
}

EMissionError ViewMissionEvidence(GameState& State, const MissionDef& M, const std::string& EvidenceId)
{
	CampaignState& C = State.Campaign;
	if (C.ActiveMissionId != M.Id) return EMissionError::NotActive;
	const bool bKnown = std::any_of(M.Evidence.begin(), M.Evidence.end(), [&](const MissionEvidenceDef& E) { return E.Id == EvidenceId; });
	if (!bKnown) return EMissionError::UnknownEvidence;
	if (!Contains(C.ViewedEvidence, EvidenceId)) C.ViewedEvidence.push_back(EvidenceId);
	return EMissionError::None;
}

EMissionError AbandonMission(GameState& State)
{
	if (State.Campaign.ActiveMissionId.empty()) return EMissionError::NotActive;
	State.Campaign.ActiveMissionId.clear();
	State.Campaign.ViewedEvidence.clear();
	return EMissionError::None;
}

static const MissionAnswer* FindAnswer(const std::vector<MissionAnswer>& Answers, const std::string& QuestionId)
{
	for (const MissionAnswer& A : Answers)
	{
		if (A.QuestionId == QuestionId) return &A;
	}
	return nullptr;
}

static const MissionOptionDef* FindOption(const MissionQuestionDef& Q, const std::string& OptionId)
{
	for (const MissionOptionDef& O : Q.Options)
	{
		if (O.Id == OptionId) return &O;
	}
	return nullptr;
}

EMissionError ValidateAnswers(const GameState& State, const MissionDef& M, const std::vector<MissionAnswer>& Answers)
{
	if (State.Campaign.ActiveMissionId != M.Id) return EMissionError::NotActive;
	for (const MissionEvidenceDef& E : M.Evidence)
	{
		if (E.bRequired && !Contains(State.Campaign.ViewedEvidence, E.Id)) return EMissionError::RequiredEvidenceNotViewed;
	}
	for (const MissionAnswer& A : Answers)
	{
		const bool bKnown = std::any_of(M.Questions.begin(), M.Questions.end(), [&](const MissionQuestionDef& Q) { return Q.Id == A.QuestionId; });
		if (!bKnown) return EMissionError::UnknownOption;
	}
	for (const MissionQuestionDef& Q : M.Questions)
	{
		const MissionAnswer* A = FindAnswer(Answers, Q.Id);
		if (!A || A->OptionIds.empty()) return EMissionError::UnansweredQuestion;
		std::set<std::string> Unique;
		for (const std::string& O : A->OptionIds)
		{
			if (!FindOption(Q, O) || !Unique.insert(O).second) return EMissionError::UnknownOption;
		}
		if (Q.Kind == EQuestionKind::Single && A->OptionIds.size() != 1) return EMissionError::SingleNeedsOneOption;
	}
	return EMissionError::None;
}

MissionDebrief ScoreMission(const MissionDef& M, const std::vector<MissionAnswer>& Answers, int Level)
{
	MissionDebrief D;
	D.MissionId = M.Id;
	D.Lesson = M.Lesson;
	const double Authority = LevelDef(Level).DecisionAuthority;
	double Weighted = 0.0;
	double CompSum[CompetencyCount] = {};
	int CompWeight[CompetencyCount] = {};
	for (const MissionQuestionDef& Q : M.Questions)
	{
		MissionCriterion C;
		C.QuestionId = Q.Id;
		C.Prompt = Q.Prompt;
		C.Competency = Q.Competency;
		C.Weight = Q.Weight;
		const MissionAnswer* A = FindAnswer(Answers, Q.Id);
		const std::vector<std::string> Picked = A ? A->OptionIds : std::vector<std::string>();
		if (Q.Kind == EQuestionKind::Single)
		{
			const MissionOptionDef* O = Picked.empty() ? nullptr : FindOption(Q, Picked.front());
			if (O)
			{
				C.Score = O->Score;
				if (!O->Feedback.empty()) C.Feedback.push_back(O->Feedback);
				if (O->Cost > Authority && !O->bEscalates)
				{
					C.bAuthorityExceeded = true;
					C.Score = std::min(C.Score, AuthorityBreachScoreCap);
					C.Feedback.push_back("This commits " + FormatMoney(O->Cost) + ", above your approval authority of " + FormatMoney(Authority) +
						". It needed escalating to someone who can approve it.");
				}
			}
		}
		else
		{
			int Correct = 0, Hits = 0, False = 0;
			for (const MissionOptionDef& O : Q.Options)
			{
				const bool bPicked = Contains(Picked, O.Id);
				Correct += O.bCorrect ? 1 : 0;
				if (bPicked && O.bCorrect) ++Hits;
				if (bPicked && !O.bCorrect) ++False;
				if (bPicked && !O.Feedback.empty()) C.Feedback.push_back(O.Feedback);
				if (!bPicked && O.bCorrect) C.Feedback.push_back("Missed: " + O.Label);
			}
			const int Raw = Correct ? static_cast<int>(std::lround(100.0 * Hits / Correct)) : 0;
			C.Score = std::max(0, std::min(100, Raw - MultiFalsePickPenalty * False));
		}
		Weighted += C.Score * C.Weight;
		const int Ci = static_cast<int>(Q.Competency);
		CompSum[Ci] += static_cast<double>(C.Score) * C.Weight;
		CompWeight[Ci] += C.Weight;
		D.Criteria.push_back(C);
	}
	D.Score = static_cast<int>(std::lround(Weighted / 100.0));
	D.bPassed = D.Score >= M.PassMark;
	for (int i = 0; i < CompetencyCount; ++i)
	{
		if (CompWeight[i] > 0)
		{
			D.CompetencyEvidence.push_back({static_cast<ECompetency>(i), static_cast<int>(std::lround(CompSum[i] / CompWeight[i]))});
		}
	}
	D.XpAwarded = M.Xp * D.Score / 100;
	return D;
}

EMissionError SubmitMission(GameState& State, const MissionDef& M, const std::vector<MissionAnswer>& Answers, MissionDebrief* OutDebrief)
{
	const EMissionError Err = ValidateAnswers(State, M, Answers);
	if (Err != EMissionError::None) return Err;

	MissionDebrief D = ScoreMission(M, Answers, State.Career.Level);
	for (const AssessmentResult& A : D.CompetencyEvidence) State.Career.ApplyAssessment(A);
	State.Career.RecordMission(M.Id, D.bPassed, D.XpAwarded);

	int Attempt = 1;
	for (const MissionRecord& R : State.Campaign.Records) Attempt += R.MissionId == M.Id ? 1 : 0;

	// Organisational decision: every chosen option's immediate effects, flags and scheduled consequences.
	DecisionRecord Rec;
	Rec.Id = M.Id + "-" + std::to_string(Attempt);
	Rec.MissionId = M.Id;
	Rec.Summary = M.Title;
	Rec.QualityScore = D.Score;
	std::vector<const MissionOptionDef*> Chosen;
	for (const MissionQuestionDef& Q : M.Questions)
	{
		const MissionAnswer* A = FindAnswer(Answers, Q.Id);
		for (const std::string& Id : A->OptionIds)
		{
			const MissionOptionDef* O = FindOption(Q, Id);
			Chosen.push_back(O);
			if (Q.Kind == EQuestionKind::Single) Rec.Summary += "; " + O->Label;
		}
	}
	for (const MissionOptionDef* O : Chosen)
	{
		Rec.Effects.insert(Rec.Effects.end(), O->Effects.begin(), O->Effects.end());
	}
	State.Org.RecordDecision(Rec);
	for (const MissionOptionDef* O : Chosen)
	{
		for (const std::string& F : O->SetsFlags) State.Org.SetFlag(F);
		for (const ConsequenceDef& Cq : O->Schedules)
		{
			PendingConsequence P;
			P.Id = M.Id + "-" + Cq.Id;
			P.SourceDecisionId = Rec.Id;
			P.Description = Cq.Description;
			P.TriggerWeek = State.Org.Week + std::max(1, Cq.DelayWeeks);
			P.Probability = std::min(1.0, Cq.Probability * IncidentProbabilityMultiplier(State.Difficulty));
			P.EffectsIfOccurs = Cq.Effects;
			P.MitigatingFlag = Cq.MitigatingFlag;
			P.MitigatedProbabilityFactor = Cq.MitigatedProbabilityFactor;
			P.MitigatedSeverityFactor = Cq.MitigatedSeverityFactor;
			P.FlagIfOccurs = Cq.FlagIfOccurs;
			State.Org.Schedule(P);
		}
	}
	State.Org.SetFlag(M.Id + (D.bPassed ? "_passed" : "_failed"));

	MissionRecord Record;
	Record.MissionId = M.Id;
	Record.Week = State.Org.Week;
	Record.Level = State.Career.Level;
	Record.Score = D.Score;
	Record.bPassed = D.bPassed;
	Record.Answers = Answers;
	State.Campaign.Records.push_back(Record);
	State.Campaign.ActiveMissionId.clear();
	State.Campaign.ViewedEvidence.clear();
	if (!M.Lesson.empty()) State.LearningHistory.push_back(M.Id + ": " + M.Lesson);
	State.Checkpoint = M.Id + ":debriefed";

	// Time passes while the work lands; anything due (from this or an earlier mission) resolves now.
	D.Resolved = AdvanceGameWeeks(State, M.WeeksToComplete);

	// Promotion review: competence evidence and passed missions only (Docs/CareerModel.md).
	D.bPromoted = State.Career.TryPromote();
	D.Level = State.Career.Level;
	D.PromotionGaps = State.Career.CheckPromotion().UnmetGates;
	if (OutDebrief) *OutDebrief = D;
	return EMissionError::None;
}
} // namespace rb
