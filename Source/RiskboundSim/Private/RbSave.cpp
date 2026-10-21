#include "RbSave.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

namespace rb
{
// Paths are UTF-8 throughout the simulation layer.
static fs::path ToPath(const std::string& Utf8)
{
#if defined(__cpp_char8_t)
	return fs::path(std::u8string(Utf8.begin(), Utf8.end()));
#else
	return fs::u8path(Utf8);
#endif
}

const char* ToString(ELoadStatus S)
{
	switch (S)
	{
	case ELoadStatus::Ok: return "Ok";
	case ELoadStatus::RecoveredFromBackup: return "Recovered from backup";
	case ELoadStatus::NotFound: return "Not found";
	case ELoadStatus::Corrupt: return "Corrupt";
	case ELoadStatus::UnsupportedVersion: return "Unsupported version";
	}
	return "?";
}

static const char* const Magic = "RISKBOUND-SAVE";

static uint64_t Fnv1a64(const std::string& S)
{
	uint64_t H = 0xCBF29CE484222325ULL;
	for (unsigned char C : S)
	{
		H ^= C;
		H *= 0x100000001B3ULL;
	}
	return H;
}

static std::string Escape(const std::string& V)
{
	std::string O;
	O.reserve(V.size());
	for (char C : V)
	{
		switch (C)
		{
		case '\\': O += "\\\\"; break;
		case '\t': O += "\\t"; break;
		case '\n': O += "\\n"; break;
		case '\r': O += "\\r"; break;
		default: O += C;
		}
	}
	return O;
}

static std::string Unescape(const std::string& V)
{
	std::string O;
	O.reserve(V.size());
	for (size_t i = 0; i < V.size(); ++i)
	{
		if (V[i] == '\\' && i + 1 < V.size())
		{
			const char N = V[++i];
			O += N == 't' ? '\t' : N == 'n' ? '\n' : N == 'r' ? '\r' : N;
		}
		else
		{
			O += V[i];
		}
	}
	return O;
}

// ---------------- writer ----------------
class KvWriter
{
public:
	std::string Body;

	void Put(const std::string& K, const std::string& V) { Body += K + "\t" + Escape(V) + "\n"; }
	void Int(const std::string& K, long long V) { Put(K, std::to_string(V)); }
	void U64(const std::string& K, uint64_t V)
	{
		char Buf[32];
		std::snprintf(Buf, sizeof(Buf), "%" PRIu64, V);
		Put(K, Buf);
	}
	void Dbl(const std::string& K, double V)
	{
		char Buf[64];
		std::snprintf(Buf, sizeof(Buf), "%.17g", V);
		Put(K, Buf);
	}
	void Bool(const std::string& K, bool V) { Put(K, V ? "1" : "0"); }
	void List(const std::string& K, const std::vector<std::string>& V)
	{
		Int(K + ".n", static_cast<long long>(V.size()));
		for (size_t i = 0; i < V.size(); ++i) Put(K + "." + std::to_string(i), V[i]);
	}
};

// ---------------- reader ----------------
class KvReader
{
public:
	std::map<std::string, std::string> Map;
	bool bOk = true;

	bool Has(const std::string& K) const { return Map.count(K) != 0; }
	std::string Str(const std::string& K, const std::string& Def = std::string()) const
	{
		const auto It = Map.find(K);
		return It == Map.end() ? Def : It->second;
	}
	long long Int(const std::string& K, long long Def = 0)
	{
		const auto It = Map.find(K);
		if (It == Map.end()) return Def;
		char* End = nullptr;
		const long long V = std::strtoll(It->second.c_str(), &End, 10);
		if (End == It->second.c_str() || *End != '\0') bOk = false;
		return V;
	}
	uint64_t U64(const std::string& K, uint64_t Def = 0)
	{
		const auto It = Map.find(K);
		if (It == Map.end()) return Def;
		char* End = nullptr;
		const unsigned long long V = std::strtoull(It->second.c_str(), &End, 10);
		if (End == It->second.c_str() || *End != '\0') bOk = false;
		return static_cast<uint64_t>(V);
	}
	double Dbl(const std::string& K, double Def = 0.0)
	{
		const auto It = Map.find(K);
		if (It == Map.end()) return Def;
		char* End = nullptr;
		const double V = std::strtod(It->second.c_str(), &End);
		if (End == It->second.c_str() || *End != '\0') bOk = false;
		return V;
	}
	bool Bool(const std::string& K, bool Def = false) { return Int(K, Def ? 1 : 0) != 0; }
	template <typename E>
	E Enum(const std::string& K, E Def, int Max)
	{
		const long long V = Int(K, static_cast<long long>(Def));
		if (V < 0 || V > Max)
		{
			bOk = false;
			return Def;
		}
		return static_cast<E>(V);
	}
	// Caps element counts so a hostile/corrupt file cannot request huge allocations.
	int Count(const std::string& K)
	{
		const long long N = Int(K + ".n", 0);
		if (N < 0 || N > 100000)
		{
			bOk = false;
			return 0;
		}
		return static_cast<int>(N);
	}
	std::vector<std::string> List(const std::string& K)
	{
		std::vector<std::string> V;
		const int N = Count(K);
		for (int i = 0; i < N; ++i) V.push_back(Str(K + "." + std::to_string(i)));
		return V;
	}
};

// ---------------- sections ----------------
static void WriteEffects(KvWriter& W, const std::string& P, const std::vector<MetricEffect>& Effects)
{
	W.Int(P + ".n", static_cast<long long>(Effects.size()));
	for (size_t i = 0; i < Effects.size(); ++i)
	{
		const std::string Q = P + "." + std::to_string(i);
		W.Int(Q + ".m", static_cast<int>(Effects[i].Metric));
		W.Dbl(Q + ".d", Effects[i].Delta);
	}
}

static std::vector<MetricEffect> ReadEffects(KvReader& R, const std::string& P)
{
	std::vector<MetricEffect> Out;
	const int N = R.Count(P);
	for (int i = 0; i < N; ++i)
	{
		const std::string Q = P + "." + std::to_string(i);
		Out.push_back({R.Enum<EOrgMetric>(Q + ".m", EOrgMetric::Revenue, OrgMetricCount - 1), R.Dbl(Q + ".d")});
	}
	return Out;
}

static void WriteCareer(KvWriter& W, const CareerState& C)
{
	W.Int("career.level", C.Level);
	W.Int("career.xp", C.Xp);
	W.Int("career.spec", static_cast<int>(C.Specialism));
	for (int i = 0; i < CompetencyCount; ++i)
	{
		W.Int("career.comp." + std::to_string(i) + ".s", C.Competencies[static_cast<size_t>(i)].Score);
		W.Int("career.comp." + std::to_string(i) + ".a", C.Competencies[static_cast<size_t>(i)].Assessments);
	}
	W.List("career.passed", C.PassedMissions);
	W.List("career.attempted", C.AttemptedMissions);
}

static void ReadCareer(KvReader& R, CareerState& C)
{
	C.Level = static_cast<int>(R.Int("career.level", 2));
	if (C.Level < 1 || C.Level > static_cast<int>(CareerLadder().size())) R.bOk = false;
	C.Xp = static_cast<int>(R.Int("career.xp"));
	C.Specialism = R.Enum<ESpecialism>("career.spec", ESpecialism::None, static_cast<int>(ESpecialism::Environmental));
	for (int i = 0; i < CompetencyCount; ++i)
	{
		C.Competencies[static_cast<size_t>(i)].Score = static_cast<int>(R.Int("career.comp." + std::to_string(i) + ".s"));
		C.Competencies[static_cast<size_t>(i)].Assessments = static_cast<int>(R.Int("career.comp." + std::to_string(i) + ".a"));
	}
	C.PassedMissions = R.List("career.passed");
	C.AttemptedMissions = R.List("career.attempted");
}

static void WriteOrg(KvWriter& W, const OrganizationState& O)
{
	W.Put("org.profile", O.ProfileId);
	W.Put("org.name", O.Name);
	W.Int("org.week", O.Week);
	W.U64("org.seed", O.EventSeed);
	W.U64("org.draws", O.EventDraws);
	for (int i = 0; i < OrgMetricCount; ++i) W.Dbl("org.metric." + std::to_string(i), O.Metrics[i]);
	W.List("org.flags", O.Flags);
	W.Int("org.dec.n", static_cast<long long>(O.Decisions.size()));
	for (size_t i = 0; i < O.Decisions.size(); ++i)
	{
		const DecisionRecord& D = O.Decisions[i];
		const std::string P = "org.dec." + std::to_string(i);
		W.Put(P + ".id", D.Id);
		W.Put(P + ".mission", D.MissionId);
		W.Put(P + ".summary", D.Summary);
		W.Int(P + ".week", D.Week);
		W.Int(P + ".quality", D.QualityScore);
		W.Put(P + ".outcome", D.Outcome);
		WriteEffects(W, P + ".fx", D.Effects);
	}
	W.Int("org.pend.n", static_cast<long long>(O.Pending.size()));
	for (size_t i = 0; i < O.Pending.size(); ++i)
	{
		const PendingConsequence& C = O.Pending[i];
		const std::string P = "org.pend." + std::to_string(i);
		W.Put(P + ".id", C.Id);
		W.Put(P + ".src", C.SourceDecisionId);
		W.Put(P + ".desc", C.Description);
		W.Int(P + ".week", C.TriggerWeek);
		W.Dbl(P + ".p", C.Probability);
		WriteEffects(W, P + ".fx", C.EffectsIfOccurs);
		W.Put(P + ".flag", C.MitigatingFlag);
		W.Dbl(P + ".mp", C.MitigatedProbabilityFactor);
		W.Dbl(P + ".ms", C.MitigatedSeverityFactor);
		W.Put(P + ".setflag", C.FlagIfOccurs);
	}
	W.Int("org.hist.n", static_cast<long long>(O.History.size()));
	for (size_t i = 0; i < O.History.size(); ++i)
	{
		const ResolvedConsequence& H = O.History[i];
		const std::string P = "org.hist." + std::to_string(i);
		W.Put(P + ".id", H.Id);
		W.Put(P + ".src", H.SourceDecisionId);
		W.Put(P + ".desc", H.Description);
		W.Int(P + ".week", H.Week);
		W.Bool(P + ".occ", H.bOccurred);
		W.Bool(P + ".mit", H.bMitigated);
		W.Dbl(P + ".p", H.EffectiveProbability);
		W.Put(P + ".setflag", H.FlagIfOccurs);
	}
}

static void ReadOrg(KvReader& R, OrganizationState& O)
{
	O.ProfileId = R.Str("org.profile");
	O.Name = R.Str("org.name");
	O.Week = static_cast<int>(R.Int("org.week"));
	O.EventSeed = R.U64("org.seed", 1);
	O.EventDraws = R.U64("org.draws", 0);
	for (int i = 0; i < OrgMetricCount; ++i) O.Metrics[i] = R.Dbl("org.metric." + std::to_string(i));
	O.Flags = R.List("org.flags");
	O.Decisions.clear();
	const int ND = R.Count("org.dec");
	for (int i = 0; i < ND; ++i)
	{
		const std::string P = "org.dec." + std::to_string(i);
		DecisionRecord D;
		D.Id = R.Str(P + ".id");
		D.MissionId = R.Str(P + ".mission");
		D.Summary = R.Str(P + ".summary");
		D.Week = static_cast<int>(R.Int(P + ".week"));
		D.QualityScore = static_cast<int>(R.Int(P + ".quality"));
		D.Outcome = R.Str(P + ".outcome");
		D.Effects = ReadEffects(R, P + ".fx");
		O.Decisions.push_back(D);
	}
	O.Pending.clear();
	const int NP = R.Count("org.pend");
	for (int i = 0; i < NP; ++i)
	{
		const std::string P = "org.pend." + std::to_string(i);
		PendingConsequence C;
		C.Id = R.Str(P + ".id");
		C.SourceDecisionId = R.Str(P + ".src");
		C.Description = R.Str(P + ".desc");
		C.TriggerWeek = static_cast<int>(R.Int(P + ".week"));
		C.Probability = R.Dbl(P + ".p", 1.0);
		C.EffectsIfOccurs = ReadEffects(R, P + ".fx");
		C.MitigatingFlag = R.Str(P + ".flag");
		C.MitigatedProbabilityFactor = R.Dbl(P + ".mp", 1.0);
		C.MitigatedSeverityFactor = R.Dbl(P + ".ms", 1.0);
		C.FlagIfOccurs = R.Str(P + ".setflag");
		O.Pending.push_back(C);
	}
	O.History.clear();
	const int NH = R.Count("org.hist");
	for (int i = 0; i < NH; ++i)
	{
		const std::string P = "org.hist." + std::to_string(i);
		ResolvedConsequence H;
		H.Id = R.Str(P + ".id");
		H.SourceDecisionId = R.Str(P + ".src");
		H.Description = R.Str(P + ".desc");
		H.Week = static_cast<int>(R.Int(P + ".week"));
		H.bOccurred = R.Bool(P + ".occ");
		H.bMitigated = R.Bool(P + ".mit");
		H.EffectiveProbability = R.Dbl(P + ".p");
		H.FlagIfOccurs = R.Str(P + ".setflag");
		O.History.push_back(H);
	}
}

static void WriteRegister(KvWriter& W, const RiskRegister& Reg)
{
	const RiskMatrix& M = Reg.Matrix();
	W.Int("reg.matrix.size", M.Size);
	W.Int("reg.matrix.med", M.MediumFrom);
	W.Int("reg.matrix.high", M.HighFrom);
	W.Int("reg.matrix.crit", M.CriticalFrom);
	W.Int("reg.matrix.cells.n", static_cast<long long>(M.CellOverrides.size()));
	for (size_t i = 0; i < M.CellOverrides.size(); ++i) W.Int("reg.matrix.cells." + std::to_string(i), static_cast<int>(M.CellOverrides[i]));
	const RiskAppetite& A = Reg.Appetite();
	W.Int("reg.app.default", A.DefaultMaxResidualScore);
	W.Int("reg.app.n", static_cast<long long>(A.Rules.size()));
	for (size_t i = 0; i < A.Rules.size(); ++i)
	{
		W.Put("reg.app." + std::to_string(i) + ".cat", A.Rules[i].Category);
		W.Int("reg.app." + std::to_string(i) + ".max", A.Rules[i].MaxResidualScore);
	}
	W.Int("reg.next", Reg.NextIdNumber());
	const auto& Items = Reg.Entries();
	W.Int("reg.risk.n", static_cast<long long>(Items.size()));
	for (size_t i = 0; i < Items.size(); ++i)
	{
		const RiskEntry& E = Items[i];
		const std::string P = "reg.risk." + std::to_string(i);
		W.Put(P + ".id", E.Id);
		W.Put(P + ".title", E.Title);
		W.Put(P + ".cause", E.Cause);
		W.Put(P + ".event", E.Event);
		W.Put(P + ".cons", E.Consequence);
		W.Put(P + ".cat", E.Category);
		W.Put(P + ".owner", E.Owner);
		W.Int(P + ".il", E.InherentLikelihood);
		W.Int(P + ".ii", E.InherentImpact);
		W.Int(P + ".ctl.n", static_cast<long long>(E.Controls.size()));
		for (size_t c = 0; c < E.Controls.size(); ++c)
		{
			const std::string Q = P + ".ctl." + std::to_string(c);
			W.Put(Q + ".name", E.Controls[c].Name);
			W.Int(Q + ".type", static_cast<int>(E.Controls[c].Type));
			W.Int(Q + ".design", E.Controls[c].DesignScore);
			W.Int(Q + ".oper", E.Controls[c].OperatingScore);
		}
		W.Int(P + ".treat", static_cast<int>(E.Treatment));
		W.Put(P + ".aowner", E.ActionOwner);
		W.Put(P + ".deadline", E.ActionDeadline);
		W.List(P + ".evidence", E.Evidence);
		W.Int(P + ".status", static_cast<int>(E.Status));
	}
}

static void ReadRegister(KvReader& R, RiskRegister& Out)
{
	RiskMatrix M;
	M.Size = static_cast<int>(R.Int("reg.matrix.size", 5));
	M.MediumFrom = static_cast<int>(R.Int("reg.matrix.med", 5));
	M.HighFrom = static_cast<int>(R.Int("reg.matrix.high", 10));
	M.CriticalFrom = static_cast<int>(R.Int("reg.matrix.crit", 16));
	const int NC = R.Count("reg.matrix.cells");
	for (int i = 0; i < NC; ++i) M.CellOverrides.push_back(R.Enum<ERating>("reg.matrix.cells." + std::to_string(i), ERating::Low, 3));
	if (!M.IsValid()) R.bOk = false;
	RiskAppetite A;
	A.DefaultMaxResidualScore = static_cast<int>(R.Int("reg.app.default", 9));
	const int NA = R.Count("reg.app");
	for (int i = 0; i < NA; ++i)
	{
		A.Rules.push_back({R.Str("reg.app." + std::to_string(i) + ".cat"), static_cast<int>(R.Int("reg.app." + std::to_string(i) + ".max"))});
	}
	RiskRegister Reg(M, A);
	Reg.SetNextIdNumber(static_cast<int>(R.Int("reg.next", 1)));
	const int N = R.Count("reg.risk");
	for (int i = 0; i < N; ++i)
	{
		const std::string P = "reg.risk." + std::to_string(i);
		RiskEntry E;
		E.Id = R.Str(P + ".id");
		E.Title = R.Str(P + ".title");
		E.Cause = R.Str(P + ".cause");
		E.Event = R.Str(P + ".event");
		E.Consequence = R.Str(P + ".cons");
		E.Category = R.Str(P + ".cat");
		E.Owner = R.Str(P + ".owner");
		E.InherentLikelihood = static_cast<int>(R.Int(P + ".il", 1));
		E.InherentImpact = static_cast<int>(R.Int(P + ".ii", 1));
		const int NCt = R.Count(P + ".ctl");
		for (int c = 0; c < NCt; ++c)
		{
			const std::string Q = P + ".ctl." + std::to_string(c);
			Control Ct;
			Ct.Name = R.Str(Q + ".name");
			Ct.Type = R.Enum<EControlType>(Q + ".type", EControlType::Preventive, 2);
			Ct.DesignScore = static_cast<int>(R.Int(Q + ".design"));
			Ct.OperatingScore = static_cast<int>(R.Int(Q + ".oper"));
			E.Controls.push_back(Ct);
		}
		E.Treatment = R.Enum<ETreatment>(P + ".treat", ETreatment::Undecided, 4);
		E.ActionOwner = R.Str(P + ".aowner");
		E.ActionDeadline = R.Str(P + ".deadline");
		E.Evidence = R.List(P + ".evidence");
		E.Status = R.Enum<ERiskStatus>(P + ".status", ERiskStatus::Open, 3);
		if (E.Id.empty() || Reg.Find(E.Id) != nullptr) R.bOk = false;
		Reg.RestoreEntry(E);
	}
	Out = std::move(Reg);
}

static void WriteMission(KvWriter& W, const m001::MissionState& M)
{
	for (int i = 0; i < m001::ObjectiveCount; ++i) W.Bool("m001.obj." + std::to_string(i), M.bObjectives[i]);
	W.List("m001.viewed", M.ViewedEvidence);
	W.Bool("m001.submitted", M.bSubmitted);
	if (!M.bSubmitted) return;
	const m001::Submission& S = M.Submitted;
	W.List("m001.sub.missing", S.MissingInfo);
	W.Int("m001.sub.il", S.InherentLikelihood);
	W.Int("m001.sub.ii", S.InherentImpact);
	W.Int("m001.sub.ctl.n", static_cast<long long>(S.ControlRatings.size()));
	for (size_t i = 0; i < S.ControlRatings.size(); ++i) W.Int("m001.sub.ctl." + std::to_string(i), static_cast<int>(S.ControlRatings[i]));
	W.Int("m001.sub.rl", S.ResidualLikelihood);
	W.Int("m001.sub.ri", S.ResidualImpact);
	W.Put("m001.sub.treat", S.TreatmentId);
	W.Put("m001.sub.owner", S.ActionOwnerId);
	W.List("m001.sub.tags", S.RationaleTagIds);
	W.Put("m001.sub.text", S.RationaleText);
}

static void ReadMission(KvReader& R, m001::MissionState& M)
{
	M = m001::MissionState();
	for (int i = 0; i < m001::ObjectiveCount; ++i) M.bObjectives[i] = R.Bool("m001.obj." + std::to_string(i));
	M.ViewedEvidence = R.List("m001.viewed");
	M.bSubmitted = R.Bool("m001.submitted");
	if (!M.bSubmitted) return;
	m001::Submission& S = M.Submitted;
	S.MissingInfo = R.List("m001.sub.missing");
	S.InherentLikelihood = static_cast<int>(R.Int("m001.sub.il"));
	S.InherentImpact = static_cast<int>(R.Int("m001.sub.ii"));
	const int N = R.Count("m001.sub.ctl");
	for (int i = 0; i < N; ++i)
	{
		S.ControlRatings.push_back(R.Enum<EControlEffectiveness>("m001.sub.ctl." + std::to_string(i), EControlEffectiveness::Ineffective, 2));
	}
	S.ResidualLikelihood = static_cast<int>(R.Int("m001.sub.rl"));
	S.ResidualImpact = static_cast<int>(R.Int("m001.sub.ri"));
	S.TreatmentId = R.Str("m001.sub.treat");
	S.ActionOwnerId = R.Str("m001.sub.owner");
	S.RationaleTagIds = R.List("m001.sub.tags");
	S.RationaleText = R.Str("m001.sub.text");
	if (S.ControlRatings.size() != m001::ExistingControls().size())
	{
		R.bOk = false;
		return;
	}
	// The debrief is a pure function of the submission, so it is recomputed rather than stored.
	M.Result = m001::Score(S, M.ViewedEvidence);
}

static void WriteActions(KvWriter& W, const ActionTracker& T)
{
	W.Int("act.next", T.NextIdNumber());
	W.Int("act.n", static_cast<long long>(T.All().size()));
	for (size_t i = 0; i < T.All().size(); ++i)
	{
		const CorrectiveAction& A = T.All()[i];
		const std::string P = "act." + std::to_string(i);
		W.Put(P + ".id", A.Id);
		W.Put(P + ".risk", A.RiskId);
		W.Put(P + ".title", A.Title);
		W.Put(P + ".owner", A.Owner);
		W.Int(P + ".created", A.CreatedWeek);
		W.Int(P + ".due", A.DueWeek);
		W.Bool(P + ".started", A.bStarted);
		W.Bool(P + ".done", A.bDone);
		W.Int(P + ".doneWeek", A.DoneWeek);
		W.Put(P + ".evidence", A.CompletionEvidence);
	}
}

static void ReadActions(KvReader& R, ActionTracker& Out)
{
	ActionTracker T;
	const int N = R.Count("act");
	for (int i = 0; i < N; ++i)
	{
		const std::string P = "act." + std::to_string(i);
		CorrectiveAction A;
		A.Id = R.Str(P + ".id");
		A.RiskId = R.Str(P + ".risk");
		A.Title = R.Str(P + ".title");
		A.Owner = R.Str(P + ".owner");
		A.CreatedWeek = static_cast<int>(R.Int(P + ".created"));
		A.DueWeek = static_cast<int>(R.Int(P + ".due"));
		A.bStarted = R.Bool(P + ".started");
		A.bDone = R.Bool(P + ".done");
		A.DoneWeek = static_cast<int>(R.Int(P + ".doneWeek", -1));
		A.CompletionEvidence = R.Str(P + ".evidence");
		if (A.Id.empty() || T.Find(A.Id) != nullptr || (A.bDone && A.CompletionEvidence.empty())) R.bOk = false;
		T.Restore(A);
	}
	T.SetNextIdNumber(static_cast<int>(R.Int("act.next", 1)));
	Out = std::move(T);
}

// KRI history stores week and value only; status, streak and escalation are re-derived on load.
static void WriteKris(KvWriter& W, const KriMonitor& M)
{
	W.U64("kri.seed", M.Seed);
	W.Int("kri.n", static_cast<long long>(M.Series.size()));
	for (size_t i = 0; i < M.Series.size(); ++i)
	{
		const KriSeries& S = M.Series[i];
		const std::string P = "kri." + std::to_string(i);
		W.Put(P + ".id", S.Id);
		W.Int(P + ".h.n", static_cast<long long>(S.History.size()));
		for (size_t h = 0; h < S.History.size(); ++h)
		{
			W.Int(P + ".h." + std::to_string(h) + ".week", S.History[h].Week);
			W.Dbl(P + ".h." + std::to_string(h) + ".value", S.History[h].Value);
		}
	}
}

static void ReadKris(KvReader& R, KriMonitor& Out)
{
	KriMonitor M;
	M.Seed = R.U64("kri.seed", 1);
	const int N = R.Count("kri");
	for (int i = 0; i < N; ++i)
	{
		const std::string P = "kri." + std::to_string(i);
		KriSeries S;
		S.Id = R.Str(P + ".id");
		const KriDefinition* D = FindKri(S.Id);
		if (!D)
		{
			R.bOk = false;
			return;
		}
		const int NH = R.Count(P + ".h");
		for (int h = 0; h < NH; ++h)
		{
			const int Week = static_cast<int>(R.Int(P + ".h." + std::to_string(h) + ".week"));
			const double Value = R.Dbl(P + ".h." + std::to_string(h) + ".value");
			S.History.push_back(MakeReading(*D, Value, Week, S.Latest()));
		}
		M.Series.push_back(std::move(S));
	}
	Out = std::move(M);
}

static void WriteQuantified(KvWriter& W, const std::vector<QuantifiedRisk>& Q)
{
	W.Int("quant.n", static_cast<long long>(Q.size()));
	for (size_t i = 0; i < Q.size(); ++i)
	{
		const std::string P = "quant." + std::to_string(i);
		W.Put(P + ".risk", Q[i].RiskId);
		W.Int(P + ".s.n", static_cast<long long>(Q[i].Scenarios.size()));
		for (size_t s = 0; s < Q[i].Scenarios.size(); ++s)
		{
			const std::string S = P + ".s." + std::to_string(s);
			W.Put(S + ".name", Q[i].Scenarios[s].Name);
			W.Dbl(S + ".p", Q[i].Scenarios[s].AnnualProbability);
			W.Dbl(S + ".loss", Q[i].Scenarios[s].Loss);
		}
	}
}

static void ReadQuantified(KvReader& R, std::vector<QuantifiedRisk>& Out)
{
	Out.clear();
	const int N = R.Count("quant");
	for (int i = 0; i < N; ++i)
	{
		const std::string P = "quant." + std::to_string(i);
		QuantifiedRisk Q;
		Q.RiskId = R.Str(P + ".risk");
		const int NS = R.Count(P + ".s");
		for (int s = 0; s < NS; ++s)
		{
			const std::string S = P + ".s." + std::to_string(s);
			Q.Scenarios.push_back({R.Str(S + ".name"), R.Dbl(S + ".p"), R.Dbl(S + ".loss")});
		}
		if (Q.RiskId.empty() || !ValidateScenarios(Q.Scenarios, true)) R.bOk = false;
		Out.push_back(std::move(Q));
	}
}

static void WriteCampaign(KvWriter& W, const CampaignState& C)
{
	W.Put("camp.active", C.ActiveMissionId);
	W.List("camp.viewed", C.ViewedEvidence);
	W.Int("camp.rec.n", static_cast<long long>(C.Records.size()));
	for (size_t i = 0; i < C.Records.size(); ++i)
	{
		const MissionRecord& Rec = C.Records[i];
		const std::string P = "camp.rec." + std::to_string(i);
		W.Put(P + ".id", Rec.MissionId);
		W.Int(P + ".week", Rec.Week);
		W.Int(P + ".level", Rec.Level);
		W.Int(P + ".score", Rec.Score);
		W.Bool(P + ".passed", Rec.bPassed);
		W.Int(P + ".a.n", static_cast<long long>(Rec.Answers.size()));
		for (size_t a = 0; a < Rec.Answers.size(); ++a)
		{
			const std::string Q = P + ".a." + std::to_string(a);
			W.Put(Q + ".q", Rec.Answers[a].QuestionId);
			W.List(Q + ".o", Rec.Answers[a].OptionIds);
		}
	}
}

static void ReadCampaign(KvReader& R, CampaignState& Out)
{
	Out = CampaignState();
	Out.ActiveMissionId = R.Str("camp.active");
	Out.ViewedEvidence = R.List("camp.viewed");
	const int N = R.Count("camp.rec");
	for (int i = 0; i < N; ++i)
	{
		const std::string P = "camp.rec." + std::to_string(i);
		MissionRecord Rec;
		Rec.MissionId = R.Str(P + ".id");
		Rec.Week = static_cast<int>(R.Int(P + ".week"));
		Rec.Level = static_cast<int>(R.Int(P + ".level"));
		Rec.Score = static_cast<int>(R.Int(P + ".score"));
		Rec.bPassed = R.Bool(P + ".passed");
		const int NA = R.Count(P + ".a");
		for (int a = 0; a < NA; ++a)
		{
			const std::string Q = P + ".a." + std::to_string(a);
			Rec.Answers.push_back({R.Str(Q + ".q"), R.List(Q + ".o")});
		}
		if (Rec.MissionId.empty() || Rec.Score < 0 || Rec.Score > 100 || Rec.Level < 1 || Rec.Level > static_cast<int>(CareerLadder().size())) R.bOk = false;
		Out.Records.push_back(std::move(Rec));
	}
}

static void WriteWorld(KvWriter& W, const WorldState& S)
{
	W.Int("world.stress", S.Stress);
	W.Int("world.burnouts", S.Burnouts);
	W.Int("world.nextMsg", S.NextMessage);
	W.List("world.clues", S.Clues);
	W.List("world.fired", S.FiredEvents);
	W.Put("world.background", S.Background);
	W.List("world.skills", S.Skills);
	W.List("world.talks", S.ConversationsHad);
	W.Int("world.rel.n", static_cast<long long>(S.Relations.size()));
	for (size_t i = 0; i < S.Relations.size(); ++i)
	{
		const std::string P = "world.rel." + std::to_string(i);
		W.Put(P + ".id", S.Relations[i].CharacterId);
		W.Int(P + ".v", S.Relations[i].Value);
	}
	W.Int("world.msg.n", static_cast<long long>(S.Inbox.size()));
	for (size_t i = 0; i < S.Inbox.size(); ++i)
	{
		const InboxMessage& M = S.Inbox[i];
		const std::string P = "world.msg." + std::to_string(i);
		W.Put(P + ".id", M.Id);
		W.Put(P + ".event", M.EventId);
		W.Int(P + ".cat", static_cast<int>(M.Category));
		W.Put(P + ".from", M.From);
		W.Put(P + ".subject", M.Subject);
		W.Put(P + ".body", M.Body);
		W.Int(P + ".week", M.Week);
		W.Int(P + ".deadline", M.DeadlineWeek);
		W.Bool(P + ".read", M.bRead);
		W.Put(P + ".choice", M.ChoiceId);
		W.Put(P + ".outcome", M.Outcome);
		W.Bool(P + ".auto", M.bAutoResolved);
	}
	W.Int("world.news.n", static_cast<long long>(S.News.size()));
	for (size_t i = 0; i < S.News.size(); ++i)
	{
		const std::string P = "world.news." + std::to_string(i);
		W.Int(P + ".week", S.News[i].Week);
		W.Put(P + ".text", S.News[i].Text);
		W.Int(P + ".tone", S.News[i].Tone);
	}
	W.Int("world.share.n", static_cast<long long>(S.Shares.size()));
	for (size_t i = 0; i < S.Shares.size(); ++i)
	{
		const std::string P = "world.share." + std::to_string(i);
		W.Int(P + ".week", S.Shares[i].Week);
		W.Dbl(P + ".v", S.Shares[i].Value);
	}
	W.Int("world.open.n", static_cast<long long>(S.CrisisOpened.size()));
	for (size_t i = 0; i < S.CrisisOpened.size(); ++i)
	{
		const std::string P = "world.open." + std::to_string(i);
		W.Put(P + ".id", S.CrisisOpened[i].first);
		W.Int(P + ".week", S.CrisisOpened[i].second);
	}
}

// v6 added the living world. Older saves start with default stress and an empty inbox; the game
// fills relationship start values from Data/People.json on load (InitWorld).
static void ReadWorld(KvReader& R, WorldState& Out)
{
	Out = WorldState();
	Out.Stress = static_cast<int>(R.Int("world.stress", StressStart));
	Out.Burnouts = static_cast<int>(R.Int("world.burnouts", 0));
	Out.NextMessage = static_cast<int>(R.Int("world.nextMsg", 1));
	Out.Clues = R.List("world.clues");
	Out.FiredEvents = R.List("world.fired");
	Out.Background = R.Str("world.background"); // v7; older saves: no background, no skills
	Out.Skills = R.List("world.skills");
	Out.ConversationsHad = R.List("world.talks");
	if (Out.Stress < 0 || Out.Stress > StressMax || Out.Burnouts < 0 || Out.NextMessage < 1) R.bOk = false;
	const int NR = R.Count("world.rel");
	for (int i = 0; i < NR; ++i)
	{
		const std::string P = "world.rel." + std::to_string(i);
		Relation Rel{R.Str(P + ".id"), static_cast<int>(R.Int(P + ".v"))};
		if (Rel.CharacterId.empty() || Rel.Value < RelationMin || Rel.Value > RelationMax) R.bOk = false;
		Out.Relations.push_back(Rel);
	}
	const int NM = R.Count("world.msg");
	for (int i = 0; i < NM; ++i)
	{
		const std::string P = "world.msg." + std::to_string(i);
		InboxMessage M;
		M.Id = R.Str(P + ".id");
		M.EventId = R.Str(P + ".event");
		M.Category = R.Enum<EMessageCategory>(P + ".cat", EMessageCategory::Team, static_cast<int>(EMessageCategory::Count) - 1);
		M.From = R.Str(P + ".from");
		M.Subject = R.Str(P + ".subject");
		M.Body = R.Str(P + ".body");
		M.Week = static_cast<int>(R.Int(P + ".week"));
		M.DeadlineWeek = static_cast<int>(R.Int(P + ".deadline", -1));
		M.bRead = R.Bool(P + ".read");
		M.ChoiceId = R.Str(P + ".choice");
		M.Outcome = R.Str(P + ".outcome");
		M.bAutoResolved = R.Bool(P + ".auto");
		if (M.Id.empty() || M.DeadlineWeek < -1) R.bOk = false;
		Out.Inbox.push_back(std::move(M));
	}
	const int NN = R.Count("world.news");
	for (int i = 0; i < NN; ++i)
	{
		const std::string P = "world.news." + std::to_string(i);
		NewsItem N{static_cast<int>(R.Int(P + ".week")), R.Str(P + ".text"), static_cast<int>(R.Int(P + ".tone"))};
		if (N.Tone < -1 || N.Tone > 1) R.bOk = false;
		Out.News.push_back(N);
	}
	const int NS = R.Count("world.share");
	for (int i = 0; i < NS; ++i)
	{
		const std::string P = "world.share." + std::to_string(i);
		SharePoint S{static_cast<int>(R.Int(P + ".week")), R.Dbl(P + ".v", 100.0)};
		if (!std::isfinite(S.Value)) R.bOk = false;
		Out.Shares.push_back(S);
	}
	const int NO = R.Count("world.open");
	for (int i = 0; i < NO; ++i)
	{
		const std::string P = "world.open." + std::to_string(i);
		Out.CrisisOpened.push_back({R.Str(P + ".id"), static_cast<int>(R.Int(P + ".week"))});
	}
}

std::string SerializeGame(const GameState& S)
{
	KvWriter W;
	W.Body = std::string(Magic) + "\n";
	W.Int("version", CurrentSaveVersion);
	W.Put("profile.name", S.ProfileName);
	W.Int("profile.difficulty", static_cast<int>(S.Difficulty));
	W.U64("profile.seed", S.Seed);
	W.Put("checkpoint", S.Checkpoint);
	W.Dbl("settings.mouse", S.Settings.MouseSensitivity);
	W.Dbl("settings.text", S.Settings.TextScale);
	W.Dbl("settings.volume", S.Settings.MasterVolume);
	W.Bool("settings.reducedMotion", S.Settings.bReducedMotion);
	W.Bool("settings.hints", S.Settings.bHints);
	W.Bool("settings.fullscreen", S.Settings.bFullscreen);
	W.Int("settings.gfx", S.Settings.GraphicsQuality);
	WriteCareer(W, S.Career);
	WriteOrg(W, S.Org);
	WriteRegister(W, S.Register);
	WriteMission(W, S.Mission001);
	WriteActions(W, S.Actions);
	WriteKris(W, S.Kris);
	WriteQuantified(W, S.Quantified);
	WriteCampaign(W, S.Campaign);
	WriteWorld(W, S.World);
	W.List("achievements", S.Achievements);
	W.List("learning", S.LearningHistory);
	char Sum[32];
	std::snprintf(Sum, sizeof(Sum), "%016" PRIx64, Fnv1a64(W.Body));
	return W.Body + "checksum\t" + Sum + "\n";
}

// v1 (pre-release prototype) used "player" for the profile name, stored difficulty as
// text, had no settings block and no org.draws. Migrate in place to v2 keys.
static void MigrateV1ToV2(KvReader& R)
{
	if (R.Has("player"))
	{
		R.Map["profile.name"] = R.Map["player"];
		R.Map.erase("player");
	}
	if (R.Has("difficulty"))
	{
		const std::string D = R.Map["difficulty"];
		R.Map["profile.difficulty"] = D == "easy" ? "0" : D == "hard" ? "2" : "1";
		R.Map.erase("difficulty");
	}
	if (!R.Has("org.draws")) R.Map["org.draws"] = "0";
	R.Map["version"] = "2";
}

// v2 had no action tracker, KRI history or quantified risks. Defaults: KRI history rebuilt
// deterministically from the organisation and seed; if Mission 001 was already submitted, its
// treatment actions are recreated as of the decision week; no quantified risks.
static void MigrateV2ToV3(GameState& S)
{
	S.Kris = MakeKriMonitor(S.Org, S.Seed, KriHistoryWeeksAtStart);
	S.Actions = ActionTracker();
	S.Quantified.clear();
	if (S.Mission001.bSubmitted)
	{
		int Week = S.Org.Week;
		for (const DecisionRecord& D : S.Org.Decisions)
		{
			if (D.Id == "M001-treatment") Week = D.Week;
		}
		for (const CorrectiveAction& A : m001::TreatmentActions(S.Mission001.Submitted, Week))
		{
			S.Actions.Create(A);
		}
	}
}

// v4 renamed the organisations and the contractor to invented names (Docs/GameDesign.md
// "Fictional names"). Old ids are mapped, and old display names are replaced wherever a
// saved string contains them (organisation name, action owners, decision summaries).
static void MigrateV3ToV4(KvReader& R)
{
	static const std::pair<const char*, const char*> Ids[] = {{"meridian", "valtren"}, {"aldmoor", "orrinbrook"}, {"kestrel", "quellard"}};
	static const std::pair<const char*, const char*> Names[] = {
		{"Meridian Cold Chain Ltd", "Valtren Coldline Logistics"},
		{"Aldmoor Mutual Insurance", "Orrinbrook Mutual Assurance"},
		{"Kestrel Grid Projects", "Quellard Grid Projects"},
		{"CoolServ Ltd", "Kelvarin Refrigeration Services"}};
	if (R.Has("org.profile"))
	{
		for (const auto& Id : Ids)
		{
			if (R.Map["org.profile"] == Id.first) R.Map["org.profile"] = Id.second;
		}
	}
	for (auto& KV : R.Map)
	{
		for (const auto& N : Names)
		{
			const std::string From = N.first;
			size_t Pos = 0;
			while ((Pos = KV.second.find(From, Pos)) != std::string::npos)
			{
				KV.second.replace(Pos, From.size(), N.second);
				Pos += std::strlen(N.second);
			}
		}
	}
}

bool DeserializeGame(const std::string& Text, GameState& Out, LoadResult& Info)
{
	Info = LoadResult();
	Info.Status = ELoadStatus::Corrupt;
	const std::string Header = std::string(Magic) + "\n";
	if (Text.compare(0, Header.size(), Header) != 0)
	{
		Info.Message = "Missing save header";
		return false;
	}
	const size_t SumPos = Text.rfind("checksum\t");
	if (SumPos == std::string::npos || (SumPos > 0 && Text[SumPos - 1] != '\n'))
	{
		Info.Message = "Missing checksum";
		return false;
	}
	std::string Expected = Text.substr(SumPos + 9);
	while (!Expected.empty() && (Expected.back() == '\n' || Expected.back() == '\r')) Expected.pop_back();
	char Actual[32];
	std::snprintf(Actual, sizeof(Actual), "%016" PRIx64, Fnv1a64(Text.substr(0, SumPos)));
	if (Expected != Actual)
	{
		Info.Message = "Checksum mismatch";
		return false;
	}

	KvReader R;
	std::istringstream In(Text.substr(Header.size(), SumPos - Header.size()));
	std::string Line;
	while (std::getline(In, Line))
	{
		const size_t Tab = Line.find('\t');
		if (Tab == std::string::npos)
		{
			Info.Message = "Malformed line";
			return false;
		}
		R.Map[Line.substr(0, Tab)] = Unescape(Line.substr(Tab + 1));
	}

	Info.LoadedVersion = static_cast<int>(R.Int("version", 0));
	if (Info.LoadedVersion > CurrentSaveVersion)
	{
		Info.Status = ELoadStatus::UnsupportedVersion;
		Info.Message = "Save is from a newer version of Riskbound";
		return false;
	}
	if (Info.LoadedVersion < 1)
	{
		Info.Message = "Invalid version";
		return false;
	}
	if (Info.LoadedVersion == 1)
	{
		MigrateV1ToV2(R);
		Info.bMigrated = true;
	}
	if (Info.LoadedVersion < 4)
	{
		MigrateV3ToV4(R);
		Info.bMigrated = true;
	}

	GameState S;
	S.ProfileName = R.Str("profile.name", "Analyst");
	S.Difficulty = R.Enum<EDifficulty>("profile.difficulty", EDifficulty::Professional, 2);
	S.Seed = R.U64("profile.seed", 1);
	S.Checkpoint = R.Str("checkpoint");
	S.Settings.MouseSensitivity = R.Dbl("settings.mouse", 1.0);
	S.Settings.TextScale = R.Dbl("settings.text", 1.0);
	S.Settings.MasterVolume = R.Dbl("settings.volume", 0.8);
	S.Settings.bReducedMotion = R.Bool("settings.reducedMotion", false);
	S.Settings.bHints = R.Bool("settings.hints", true);
	S.Settings.bFullscreen = R.Bool("settings.fullscreen", true);
	S.Settings.GraphicsQuality = static_cast<int>(R.Int("settings.gfx", 2));
	S.Settings = ClampSettings(S.Settings); // a hand-edited save must not break the UI
	ReadCareer(R, S.Career);
	ReadOrg(R, S.Org);
	ReadRegister(R, S.Register);
	ReadMission(R, S.Mission001);
	if (Info.LoadedVersion < 3)
	{
		MigrateV2ToV3(S);
		Info.bMigrated = true;
	}
	else
	{
		ReadActions(R, S.Actions);
		ReadKris(R, S.Kris);
		ReadQuantified(R, S.Quantified);
	}
	// v5 added the campaign and consequence flags; older saves default to no campaign progress.
	ReadCampaign(R, S.Campaign);
	if (Info.LoadedVersion < 5) Info.bMigrated = true;
	ReadWorld(R, S.World);
	if (Info.LoadedVersion < 7) Info.bMigrated = true;
	S.Achievements = R.List("achievements");
	S.LearningHistory = R.List("learning");
	if (!R.bOk)
	{
		Info.Message = "Invalid field values";
		return false;
	}
	Out = std::move(S);
	Info.Status = ELoadStatus::Ok;
	return true;
}

std::string BackupPath(const std::string& Path)
{
	return Path + ".bak";
}

static bool ReadFile(const std::string& Path, std::string& Out)
{
	std::ifstream F(ToPath(Path), std::ios::binary);
	if (!F) return false;
	std::ostringstream Ss;
	Ss << F.rdbuf();
	Out = Ss.str();
	return true;
}

static bool IsValidSaveFile(const std::string& Path)
{
	std::string Text;
	GameState Tmp;
	LoadResult Info;
	return ReadFile(Path, Text) && DeserializeGame(Text, Tmp, Info);
}

bool SaveGameToFile(const std::string& Path, const GameState& State, std::string* OutError)
{
	const auto Fail = [&](const std::string& Msg) {
		if (OutError) *OutError = Msg;
		return false;
	};
	const fs::path Primary = ToPath(Path);
	const fs::path Temp = ToPath(Path + ".tmp");
	const fs::path Backup = ToPath(BackupPath(Path));
	std::error_code Ec;
	if (Primary.has_parent_path()) fs::create_directories(Primary.parent_path(), Ec);

	const std::string Text = SerializeGame(State);
	{
		std::ofstream F(Temp, std::ios::binary | std::ios::trunc);
		if (!F) return Fail("Cannot open temp file");
		F.write(Text.data(), static_cast<std::streamsize>(Text.size()));
		F.flush();
		if (!F) return Fail("Write failed");
	}
	if (!IsValidSaveFile(Path + ".tmp"))
	{
		fs::remove(Temp, Ec);
		return Fail("Verification of written save failed");
	}
	if (fs::exists(Primary, Ec))
	{
		if (IsValidSaveFile(Path))
		{
			fs::copy_file(Primary, Backup, fs::copy_options::overwrite_existing, Ec);
			if (Ec) return Fail("Backup rotation failed: " + Ec.message());
		}
		else
		{
			// Keep the corrupt file for inspection; do not let it replace a good backup.
			fs::rename(Primary, ToPath(Path + ".corrupt"), Ec);
		}
	}
	fs::rename(Temp, Primary, Ec);
	if (Ec) return Fail("Final rename failed: " + Ec.message());
	return true;
}

LoadResult LoadGameFromFile(const std::string& Path, GameState& Out)
{
	LoadResult Primary;
	std::string Text;
	const bool bPrimaryExists = ReadFile(Path, Text);
	if (bPrimaryExists && DeserializeGame(Text, Out, Primary))
	{
		return Primary;
	}
	if (Primary.Status == ELoadStatus::UnsupportedVersion)
	{
		return Primary;
	}
	LoadResult Backup;
	std::string BakText;
	const bool bBackupExists = ReadFile(BackupPath(Path), BakText);
	if (bBackupExists && DeserializeGame(BakText, Out, Backup))
	{
		Backup.Status = ELoadStatus::RecoveredFromBackup;
		Backup.Message = bPrimaryExists ? "Primary save invalid (" + Primary.Message + "); loaded backup" : "Primary save missing; loaded backup";
		return Backup;
	}
	LoadResult R;
	R.Status = (bPrimaryExists || bBackupExists) ? ELoadStatus::Corrupt : ELoadStatus::NotFound;
	R.Message = bPrimaryExists ? Primary.Message : "No save found";
	return R;
}
} // namespace rb
