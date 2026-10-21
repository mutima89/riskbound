#include "RbCampaign.h"

#include "RbGameState.h"
#include "RbQuant.h"
#include "RbRng.h"
#include "RbWorld.h"
#include "RbGrowth.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
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

const char* ToString(EQuestionKind K)
{
	switch (K)
	{
	case EQuestionKind::Single: return "single";
	case EQuestionKind::Multi: return "multi";
	case EQuestionKind::Numeric: return "numeric";
	case EQuestionKind::Rating: return "rating";
	}
	return "?";
}

bool ParseNumericAnswer(const std::string& Text, double& Out)
{
	std::string Clean;
	for (char Ch : Text)
	{
		if (Ch == ',' || Ch == ' ') continue; // thousands separators as players type them
		Clean += Ch;
	}
	if (Clean.empty() || Clean.size() > 32) return false;
	char* End = nullptr;
	const double V = std::strtod(Clean.c_str(), &End);
	if (End == Clean.c_str() || *End != '\0' || !std::isfinite(V)) return false;
	Out = V;
	return true;
}

static std::string FormatFigure(double V)
{
	char Buf[64];
	if (std::fabs(V - std::round(V)) < 1e-9) return FormatMoney(V);
	std::snprintf(Buf, sizeof(Buf), "%.2f", V);
	return Buf;
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
	case EMissionError::EvidenceElsewhere: return "This has to be seen in person: go to where it is.";
	case EMissionError::PresentInPerson: return "Present this in person at the right place in the building.";
	}
	return "?";
}

const std::vector<std::pair<std::string, std::string>>& KnownLocations()
{
	static const std::vector<std::pair<std::string, std::string>> L = {
		{"reception", "Reception"}, {"workspace", "Junior analyst workspace"}, {"manager", "Manager's office"}, {"meeting", "Meeting room"},
		{"training", "Training area"}, {"monitoring", "Risk monitoring room"}, {"boardroom", "Executive boardroom"}};
	return L;
}

std::string LocationName(const std::string& Id)
{
	for (const auto& P : KnownLocations())
	{
		if (P.first == Id) return P.second;
	}
	return std::string();
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

MissionAnswer BestAnswer(const MissionQuestionDef& Q)
{
	MissionAnswer A{Q.Id, {}};
	switch (Q.Kind)
	{
	case EQuestionKind::Multi:
		for (const MissionOptionDef& O : Q.Options)
		{
			if (O.bCorrect) A.OptionIds.push_back(O.Id);
		}
		break;
	case EQuestionKind::Numeric:
	{
		char Buf[64];
		std::snprintf(Buf, sizeof(Buf), "%.10g", Q.Answer);
		A.OptionIds.push_back(Buf);
		break;
	}
	case EQuestionKind::Rating:
		A.OptionIds = {std::to_string(Q.TargetLikelihood), std::to_string(Q.TargetImpact)};
		break;
	case EQuestionKind::Single:
		if (!Q.Options.empty())
		{
			A.OptionIds.push_back(std::max_element(Q.Options.begin(), Q.Options.end(),
				[](const MissionOptionDef& L, const MissionOptionDef& R) { return L.Score < R.Score; })->Id);
		}
		break;
	}
	return A;
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
		if (C.Flag.empty() && !C.bUseMetric && C.RelationId.empty() && C.MinClues <= 0) Err("context note without a condition");
	}
	if (M.RequiresClues < 0) Err("negative requiresClues");
	if (!M.PresentAt.empty() && LocationName(M.PresentAt).empty()) Err("presentAt is an unknown location '" + M.PresentAt + "'");
	if (M.DeadlineWeeks < 0 || M.DeadlineWeeks > 26) Err("deadlineWeeks outside 0..26");
	if (M.DeadlineWeeks > 0 && (M.Kind != EMissionKind::Crisis || M.RequiresFlags.empty()))
	{
		Err("only incident-triggered crises (with requiresFlags) can have a deadline");
	}
	if (M.DeadlineWeeks == 0 && (!M.EscalationEffects.empty() || !M.EscalationText.empty())) Err("escalation needs deadlineWeeks");
	ValidateEffects(M.EscalationEffects, P + "escalation", Out);

	std::set<std::string> EvidenceIds;
	if (M.Evidence.empty()) Err("no evidence");
	for (const MissionEvidenceDef& E : M.Evidence)
	{
		if (E.Id.empty() || !EvidenceIds.insert(E.Id).second) Err("evidence id missing or duplicated: '" + E.Id + "'");
		if (E.Title.empty() || E.Body.empty()) Err("evidence '" + E.Id + "' needs a title and body");
		if (!E.At.empty() && LocationName(E.At).empty()) Err("evidence '" + E.Id + "' is at an unknown location '" + E.At + "'");
		if (!E.Person.empty() && E.At.empty()) Err("evidence '" + E.Id + "': an interview needs a location");
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
		if (Q.Kind == EQuestionKind::Numeric || Q.Kind == EQuestionKind::Rating)
		{
			const int C = static_cast<int>(Q.Competency);
			if (C < 0 || C >= CompetencyCount) Err(QP + ": unknown competency");
			WeightSum += Q.Weight;
			if (!Q.Options.empty()) Err(QP + ": numeric and rating questions have no options");
			if (Q.Explanation.empty()) Err(QP + ": needs an explanation for the debrief");
			if (Q.Kind == EQuestionKind::Numeric)
			{
				if (!std::isfinite(Q.Answer)) Err(QP + ": answer is not a number");
				if (!(Q.Tolerance > 0.0 && Q.Tolerance <= 0.5)) Err(QP + ": tolerance outside (0, 0.5]");
				for (const NumericMistake& Mk : Q.Mistakes)
				{
					if (!std::isfinite(Mk.Value) || Mk.Feedback.empty()) Err(QP + ": a mistake needs a value and feedback");
					else if (std::fabs(Mk.Value - Q.Answer) <= Q.Tolerance * std::max(std::fabs(Q.Answer), 1.0)) Err(QP + ": a mistake is within tolerance of the answer");
				}
			}
			else if (Q.TargetLikelihood < 1 || Q.TargetLikelihood > 5 || Q.TargetImpact < 1 || Q.TargetImpact > 5)
			{
				Err(QP + ": rating targets must be 1..5");
			}
			continue;
		}
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
			if (O.Stress < -30 || O.Stress > 30) Err(OP + ": stress change outside -30..30");
			for (const RelationEffect& R : O.Relations)
			{
				if (R.CharacterId.empty() || R.Delta < -40 || R.Delta > 40) Err(OP + ": relation effect needs a character and a change within 40");
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
			const MissionDef* Req = C.Find(R);
			if (Req && Req->DeadlineWeeks > 0) Out.push_back(M.Id + ": requires " + R + ", which can escalate and close");
		}
	}
	return Out;
}

// ---------------------------------------------------------------- availability

static MissionAvailability CheckNew(const GameState& State, const MissionDef& M)
{
	MissionAvailability A;
	// Challenge modes (RbLearning.h): only the current case is open, whatever the career rules say.
	if (State.Challenge.Mode != EGameMode::Career)
	{
		A.bAvailable = ChallengeUnlocks(State, M.Id);
		if (!A.bAvailable) A.Reasons.push_back(State.Challenge.bOver ? "The challenge is over" : "Not the current case in this challenge");
		return A;
	}
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
	if (M.DeadlineWeeks > 0 && State.Org.HasFlag(M.Id + "_escalated")) A.Reasons.push_back("Escalated without you");
	if (static_cast<int>(State.World.Clues.size()) < M.RequiresClues)
	{
		A.Reasons.push_back("Needs more evidence (" + std::to_string(State.World.Clues.size()) + " of " + std::to_string(M.RequiresClues) + " clues)");
	}
	A.bAvailable = A.Reasons.empty();
	return A;
}

bool IsCampaignStuck(const GameState& State, const MissionCatalogue& C)
{
	if (State.Career.Level >= static_cast<int>(CareerLadder().size())) return false;
	if (State.Career.CheckPromotion().bEligible) return false;
	for (const MissionDef& M : C.Missions)
	{
		if (CheckNew(State, M).bAvailable) return false;
	}
	return true;
}

MissionAvailability CheckAvailability(const GameState& State, const MissionDef& M, const MissionCatalogue* Catalogue)
{
	MissionAvailability A = CheckNew(State, M);
	if (!A.bAvailable && Catalogue && State.Career.HasPassed(M.Id) && M.Kind == EMissionKind::Mission && M.MinLevel == State.Career.Level &&
		IsCampaignStuck(State, *Catalogue))
	{
		A.bAvailable = true;
		A.bReview = true;
		A.Reasons.clear();
	}
	return A;
}

std::vector<const MissionDef*> AvailableMissions(const GameState& State, const MissionCatalogue& C)
{
	std::vector<const MissionDef*> Out;
	for (const MissionDef& M : C.Missions)
	{
		if (CheckAvailability(State, M, &C).bAvailable) Out.push_back(&M);
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
		if (bApplies && !C.RelationId.empty())
		{
			const int V = GetRelation(State, C.RelationId);
			bApplies = C.bRelationBelow ? V < C.RelationThreshold : V >= C.RelationThreshold;
		}
		if (bApplies && C.MinClues > 0) bApplies = static_cast<int>(State.World.Clues.size()) >= C.MinClues;
		if (bApplies) Out.push_back(C.Text);
	}
	const std::string Cause = CrisisCause(State, M);
	if (!Cause.empty()) Out.push_back(Cause);
	return Out;
}

// ---------------------------------------------------------------- play

EMissionError StartMission(GameState& State, const MissionDef& M, const MissionCatalogue* Catalogue)
{
	CampaignState& C = State.Campaign;
	if (C.ActiveMissionId == M.Id) return EMissionError::None; // resume
	if (!C.ActiveMissionId.empty()) return EMissionError::AnotherMissionActive;
	if (!CheckAvailability(State, M, Catalogue).bAvailable) return EMissionError::Unavailable;
	C.ActiveMissionId = M.Id;
	C.ViewedEvidence.clear();
	State.Checkpoint = M.Id + ":start";
	return EMissionError::None;
}

EMissionError ViewMissionEvidence(GameState& State, const MissionDef& M, const std::string& EvidenceId, const std::string& Where)
{
	CampaignState& C = State.Campaign;
	if (C.ActiveMissionId != M.Id) return EMissionError::NotActive;
	const auto It = std::find_if(M.Evidence.begin(), M.Evidence.end(), [&](const MissionEvidenceDef& E) { return E.Id == EvidenceId; });
	if (It == M.Evidence.end()) return EMissionError::UnknownEvidence;
	if (!It->At.empty() && It->At != Where) return EMissionError::EvidenceElsewhere;
	if (!Contains(C.ViewedEvidence, EvidenceId)) C.ViewedEvidence.push_back(EvidenceId);
	for (const MissionEvidenceDef& E : M.Evidence)
	{
		if (E.Id == EvidenceId) AddClue(State, E.Clue);
	}
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
		if (Q.Kind == EQuestionKind::Single || Q.Kind == EQuestionKind::Multi)
		{
			std::set<std::string> Unique;
			for (const std::string& O : A->OptionIds)
			{
				if (!FindOption(Q, O) || !Unique.insert(O).second) return EMissionError::UnknownOption;
			}
		}
		if (Q.Kind == EQuestionKind::Numeric)
		{
			double V = 0.0;
			if (A->OptionIds.size() != 1 || !ParseNumericAnswer(A->OptionIds[0], V)) return EMissionError::UnknownOption;
			continue;
		}
		if (Q.Kind == EQuestionKind::Rating)
		{
			if (A->OptionIds.size() != 2) return EMissionError::UnknownOption;
			for (const std::string& S : A->OptionIds)
			{
				if (S.size() != 1 || S[0] < '1' || S[0] > '5') return EMissionError::UnknownOption;
			}
			continue;
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
		else if (Q.Kind == EQuestionKind::Numeric)
		{
			double V = 0.0;
			ParseNumericAnswer(Picked.empty() ? std::string() : Picked.front(), V);
			const double Scale = std::max(std::fabs(Q.Answer), 1.0);
			const double Rel = std::fabs(V - Q.Answer) / Scale;
			C.Score = Rel <= Q.Tolerance ? NumericScoreExact : (Rel <= 3 * Q.Tolerance ? NumericScoreClose : (Rel <= 10 * Q.Tolerance ? NumericScoreNear : 0));
			C.Feedback.push_back("You entered " + FormatFigure(V) + (Q.Unit.empty() ? std::string() : " " + Q.Unit) + ".");
			for (const NumericMistake& Mk : Q.Mistakes)
			{
				if (C.Score < NumericScoreExact && std::fabs(V - Mk.Value) <= Q.Tolerance * std::max(std::fabs(Mk.Value), 1.0))
				{
					C.Score = std::min(C.Score, NumericScoreMistake);
					C.Feedback.push_back(Mk.Feedback);
				}
			}
			C.Feedback.push_back("Answer: " + FormatFigure(Q.Answer) + (Q.Unit.empty() ? std::string() : " " + Q.Unit) + ". " + Q.Explanation);
		}
		else if (Q.Kind == EQuestionKind::Rating)
		{
			const int L = Picked.size() == 2 ? Picked[0][0] - '0' : 0;
			const int I = Picked.size() == 2 ? Picked[1][0] - '0' : 0;
			C.Score = std::max(0, 100 - RatingStepPenalty * (std::abs(L - Q.TargetLikelihood) + std::abs(I - Q.TargetImpact)));
			C.Feedback.push_back("You rated likelihood " + std::to_string(L) + ", impact " + std::to_string(I) + ". The evidence supports likelihood " +
				std::to_string(Q.TargetLikelihood) + ", impact " + std::to_string(Q.TargetImpact) + ". " + Q.Explanation);
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

// Latent risk from weak answers: one possible incident per weak question (FalloutScoreThreshold).
static void ScheduleFallout(GameState& State, const MissionDef& M, const std::vector<MissionAnswer>& Answers, const std::string& DecisionId,
	const WorldContent& World, MissionDebrief& D)
{
	const FalloutTemplate* T = World.FalloutFor(M.Department);
	if (!T) return;
	for (const MissionQuestionDef& Q : M.Questions)
	{
		const MissionAnswer* A = FindAnswer(Answers, Q.Id);
		if (!A || A->OptionIds.empty()) continue;
		double P = 0.0;
		std::string Choice;
		if (Q.Kind == EQuestionKind::Single)
		{
			const MissionOptionDef* O = FindOption(Q, A->OptionIds.front());
			if (!O || O->Score >= FalloutScoreThreshold || !O->Schedules.empty()) continue;
			P = FalloutMaxProbability * (FalloutScoreThreshold - O->Score) / FalloutScoreThreshold;
			Choice = O->Label;
		}
		else if (Q.Kind == EQuestionKind::Multi)
		{
			for (const std::string& Id : A->OptionIds)
			{
				const MissionOptionDef* O = FindOption(Q, Id);
				if (O && !O->bCorrect && Choice.empty()) Choice = O->Label;
			}
			if (Choice.empty()) continue;
			P = FalloutMultiProbability;
		}
		else
		{
			const MissionDebrief Scored = ScoreMission(M, Answers, State.Career.Level);
			int Score = 100;
			for (const MissionCriterion& Cr : Scored.Criteria)
			{
				if (Cr.QuestionId == Q.Id) Score = Cr.Score;
			}
			if (Score >= FalloutScoreThreshold) continue;
			P = FalloutMaxProbability * (FalloutScoreThreshold - Score) / FalloutScoreThreshold;
			Choice = Q.Kind == EQuestionKind::Numeric ? "your figure for " + Q.Prompt : "your rating for " + Q.Prompt;
		}
		std::string Text = T->Description;
		const std::pair<std::string, std::string> Subs[] = {{"{mission}", M.Title}, {"{choice}", Choice}};
		for (const auto& Sub : Subs)
		{
			for (size_t i = Text.find(Sub.first); i != std::string::npos; i = Text.find(Sub.first, i + Sub.second.size())) Text.replace(i, Sub.first.size(), Sub.second);
		}
		uint64_t H = 1469598103934665603ULL;
		for (unsigned char Ch : M.Id + "/" + Q.Id)
		{
			H ^= Ch;
			H *= 1099511628211ULL;
		}
		PendingConsequence C;
		C.Id = "FALLOUT-" + DecisionId + "-" + Q.Id;
		C.SourceDecisionId = DecisionId;
		C.Description = Text;
		C.TriggerWeek = State.Org.Week + 2 + static_cast<int>(H % 5);
		C.Probability = std::min(1.0, P * IncidentProbabilityMultiplier(State.Difficulty) * FalloutFactor(State));
		C.FlagIfOccurs = "fallout_incident";
		for (const MetricEffect& E : T->Effects)
		{
			const bool bMoney = E.Metric == EOrgMetric::Cash || E.Metric == EOrgMetric::Costs || E.Metric == EOrgMetric::Revenue;
			C.EffectsIfOccurs.push_back({E.Metric, bMoney ? E.Delta * std::max(1, M.MinLevel) : E.Delta});
		}
		State.Org.Schedule(C);
		D.Fallout.push_back(Text);
	}
}

EMissionError SubmitMission(GameState& State, const MissionDef& M, const std::vector<MissionAnswer>& Answers, MissionDebrief* OutDebrief,
	const MissionCatalogue* Catalogue, const WorldContent* World, const std::string& Where)
{
	const EMissionError Err = ValidateAnswers(State, M, Answers);
	if (Err != EMissionError::None) return Err;
	if (!M.PresentAt.empty() && Where != M.PresentAt) return EMissionError::PresentInPerson;

	MissionDebrief D = ScoreMission(M, Answers, State.Career.Level);
	D.bReview = State.Career.HasPassed(M.Id);
	for (const AssessmentResult& A : D.CompetencyEvidence) State.Career.ApplyAssessment(A);
	State.Career.RecordMission(M.Id, D.bPassed, D.bReview ? 0 : D.XpAwarded);
	if (D.bReview)
	{
		// Practice: the organisation already lived with this decision once.
		D.XpAwarded = 0;
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
		State.Checkpoint = M.Id + ":reviewed";
		D.bPromoted = State.Career.TryPromote();
		D.Level = State.Career.Level;
		D.PromotionGaps = State.Career.CheckPromotion().UnmetGates;
		if (OutDebrief) *OutDebrief = D;
		return EMissionError::None;
	}

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
		if (Q.Kind != EQuestionKind::Single && Q.Kind != EQuestionKind::Multi) continue; // figures and ratings carry no option effects
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
		for (const RelationEffect& R : O->Relations) ChangeRelation(State, R.CharacterId, R.Delta);
		ChangeStress(State, O->Stress);
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
	ChangeStress(State, M.Kind == EMissionKind::Crisis ? StressPerCrisis : (M.Kind == EMissionKind::Capstone ? StressPerCapstone : StressPerMission));
	if (World) ScheduleFallout(State, M, Answers, Rec.Id, *World, D);

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
	if (World)
	{
		const WorldUpdate U = UpdateWorld(State, Catalogue, *World, D.Resolved, M.WeeksToComplete);
		D.NewMessages = U.NewMessages;
		D.Escalated = U.Escalated;
		D.bBurnout = U.bBurnout;
	}

	// Promotion review: competence evidence and passed missions only (Docs/CareerModel.md).
	if (State.Challenge.Mode != EGameMode::Career)
	{
		AdvanceChallenge(State, D);
		D.Level = State.Career.Level;
		if (OutDebrief) *OutDebrief = D;
		return EMissionError::None;
	}
	D.bPromoted = State.Career.TryPromote();
	D.Level = State.Career.Level;
	D.PromotionGaps = State.Career.CheckPromotion().UnmetGates;
	if (OutDebrief) *OutDebrief = D;
	return EMissionError::None;
}
} // namespace rb
