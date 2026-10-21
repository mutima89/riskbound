// Gate 5 hardening: save fuzzing and corruption, settings limits, content validation per file,
// and deterministic replay of the whole campaign with save/load between every step.
#include "RbTest.h"

#include "RbCampaign.h"
#include "RbGameState.h"
#include "RbMissionData.h"
#include "RbRng.h"
#include "RbSave.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace rb;

#ifndef RB_DATA_DIR
#error "RB_DATA_DIR must point at the repository's Data folder"
#endif

namespace
{
std::string ReadAll(const std::filesystem::path& P)
{
	std::ifstream In(P, std::ios::binary);
	std::stringstream Ss;
	Ss << In.rdbuf();
	return Ss.str();
}

std::vector<std::pair<std::string, std::string>> MissionFiles()
{
	std::vector<std::pair<std::string, std::string>> Files;
	for (const auto& Entry : std::filesystem::directory_iterator(std::filesystem::path(RB_DATA_DIR) / "Missions"))
	{
		if (Entry.path().extension() == ".json") Files.push_back({Entry.path().filename().string(), ReadAll(Entry.path())});
	}
	std::sort(Files.begin(), Files.end());
	return Files;
}

MissionCatalogue Catalogue()
{
	MissionCatalogue C;
	std::vector<std::string> Errors;
	LoadMissionCatalogue(MissionFiles(), C, Errors);
	return C;
}

// Re-signs a save body after editing it, so the edit reaches the field parsers instead of being
// stopped by the checksum (FNV-1a 64, Docs/SaveFormat.md).
std::string Resign(std::string Text)
{
	const size_t Pos = Text.rfind("checksum\t");
	if (Pos != std::string::npos) Text = Text.substr(0, Pos);
	uint64_t H = 0xCBF29CE484222325ULL;
	for (unsigned char C : Text)
	{
		H ^= C;
		H *= 0x100000001B3ULL;
	}
	char Sum[32];
	std::snprintf(Sum, sizeof(Sum), "%016" PRIx64, H);
	return Text + "checksum\t" + Sum + "\n";
}

std::vector<std::string> Lines(const std::string& Text)
{
	std::vector<std::string> Out;
	std::istringstream In(Text);
	std::string L;
	while (std::getline(In, L)) Out.push_back(L);
	return Out;
}

std::string Join(const std::vector<std::string>& L)
{
	std::string Out;
	for (const std::string& S : L) Out += S + "\n";
	return Out;
}

void PassMission001(GameState& G)
{
	G.Mission001.MeetManager();
	for (const m001::EvidenceItem& E : m001::Evidence()) G.Mission001.ViewEvidence(E.Id);
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
	SubmitMission001(G, S);
}

// Answers chosen by a seeded random policy (not the best ones), so replays cover failures,
// retries, incidents and the crises they open.
std::vector<MissionAnswer> RandomAnswers(const MissionDef& M, Rng& R)
{
	std::vector<MissionAnswer> Out;
	for (const MissionQuestionDef& Q : M.Questions)
	{
		MissionAnswer A{Q.Id, {}};
		const int N = static_cast<int>(Q.Options.size());
		if (Q.Kind == EQuestionKind::Numeric)
		{
			A.OptionIds.push_back(std::to_string(Q.Answer * (0.5 + R.Uniform())));
		}
		else if (Q.Kind == EQuestionKind::Rating)
		{
			A.OptionIds = {std::to_string(R.UniformInt(1, 5)), std::to_string(R.UniformInt(1, 5))};
		}
		else if (Q.Kind == EQuestionKind::Single)
		{
			A.OptionIds.push_back(Q.Options[static_cast<size_t>(R.UniformInt(0, N - 1))].Id);
		}
		else
		{
			for (const MissionOptionDef& O : Q.Options)
			{
				if (R.Bernoulli(0.5)) A.OptionIds.push_back(O.Id);
			}
			if (A.OptionIds.empty()) A.OptionIds.push_back(Q.Options[0].Id);
		}
		Out.push_back(A);
	}
	return Out;
}

GameState Reload(const GameState& G)
{
	GameState L;
	LoadResult Info;
	const bool bOk = DeserializeGame(SerializeGame(G), L, Info);
	CHECK(bOk);
	return L;
}

// Plays the campaign with a random policy; with bReloadEveryStep the game is saved and loaded
// between every action, as if the player quit and continued each time.
GameState PlayCampaign(const MissionCatalogue& C, uint64_t Seed, EDifficulty Difficulty, bool bReloadEveryStep, int MaxMissions)
{
	GameState G = NewGame("Replay", Difficulty, Seed);
	PassMission001(G);
	Rng Policy(Seed * 7919 + 1);
	for (int Played = 0; Played < MaxMissions; ++Played)
	{
		const auto Open = AvailableMissions(G, C);
		if (Open.empty()) break;
		const MissionDef& M = *Open[static_cast<size_t>(Policy.UniformInt(0, static_cast<int>(Open.size()) - 1))];
		if (StartMission(G, M) != EMissionError::None) break;
		if (bReloadEveryStep) G = Reload(G);
		for (const MissionEvidenceDef& E : M.Evidence)
		{
			ViewMissionEvidence(G, M, E.Id, E.At);
			if (bReloadEveryStep) G = Reload(G);
		}
		if (SubmitMission(G, M, RandomAnswers(M, Policy), nullptr, nullptr, nullptr, M.PresentAt) != EMissionError::None) break;
		if (bReloadEveryStep) G = Reload(G);
	}
	return G;
}
} // namespace

TEST(Save_FuzzedFilesNeverCrashAndNeverLoadInvalidState)
{
	const MissionCatalogue C = Catalogue();
	GameState G = PlayCampaign(C, 42, EDifficulty::Professional, false, 8);
	G.Settings.TextScale = 1.3;
	const std::string Base = SerializeGame(G);
	const std::vector<std::string> BaseLines = Lines(Base);
	const char* Junk[] = {"", "-1", "999999999999", "abc", "1e308", "nan", "inf", "-0", "100001", "\t", "0x10", "3.5", "M999"};
	Rng R(2026);
	int Rejected = 0, Loaded = 0;
	for (int Trial = 0; Trial < 4000; ++Trial)
	{
		std::string Text = Base;
		const int Kind = Trial % 5;
		if (Kind == 0)
		{
			// One flipped byte: the checksum must catch it.
			const size_t P = static_cast<size_t>(R.UniformInt(0, static_cast<int>(Text.size()) - 1));
			Text[P] = static_cast<char>(Text[P] ^ (1 << R.UniformInt(0, 7)));
			GameState Out;
			LoadResult Info;
			CHECK(!DeserializeGame(Text, Out, Info));
			++Rejected;
			continue;
		}
		if (Kind == 1)
		{
			Text = Text.substr(0, static_cast<size_t>(R.UniformInt(0, static_cast<int>(Text.size()) - 2))); // truncated
		}
		else
		{
			std::vector<std::string> L = BaseLines;
			L.pop_back(); // checksum line, re-added by Resign
			const size_t Line = static_cast<size_t>(R.UniformInt(2, static_cast<int>(L.size()) - 1));
			if (Kind == 2)
			{
				const size_t Tab = L[Line].find('\t');
				L[Line] = L[Line].substr(0, Tab + 1) + Junk[R.UniformInt(0, static_cast<int>(sizeof(Junk) / sizeof(Junk[0])) - 1)];
			}
			else if (Kind == 3)
			{
				L.erase(L.begin() + static_cast<std::ptrdiff_t>(Line));
			}
			else
			{
				std::swap(L[Line], L[static_cast<size_t>(R.UniformInt(2, static_cast<int>(L.size()) - 1))]);
			}
			Text = Resign(Join(L));
		}
		GameState Out;
		LoadResult Info;
		if (!DeserializeGame(Text, Out, Info))
		{
			++Rejected;
			continue;
		}
		++Loaded;
		// Whatever loads must be a sane, self-consistent game: limits hold and it saves and loads again identically.
		CHECK(Out.Career.Level >= 1 && Out.Career.Level <= 10);
		CHECK(Out.Settings.TextScale >= 0.8 && Out.Settings.TextScale <= 1.6);
		CHECK(Out.Settings.MasterVolume >= 0.0 && Out.Settings.MasterVolume <= 1.0);
		CHECK(Out.Settings.GraphicsQuality >= 0 && Out.Settings.GraphicsQuality <= 3);
		const std::string Again = SerializeGame(Out);
		GameState Twice;
		CHECK(DeserializeGame(Again, Twice, Info));
		CHECK(SerializeGame(Twice) == Again);
	}
	std::printf("    fuzz: %d rejected, %d loaded as valid games\n", Rejected, Loaded);
	CHECK(Rejected >= 800); // every flipped byte at least
	CHECK(Loaded > 0);      // benign edits (e.g. a deleted optional line) still load
}

TEST(Save_HostileCountsAndVersionsAreRejected)
{
	const std::string Base = SerializeGame(NewGame("H", EDifficulty::Professional, 3));
	GameState Out;
	LoadResult Info;
	for (const char* Line : {"camp.rec.n\t100001", "org.dec.n\t-5", "career.level\t11", "career.level\t0", "profile.difficulty\t7", "version\t0"})
	{
		std::vector<std::string> L = Lines(Base);
		L.pop_back();
		const std::string Key = std::string(Line).substr(0, std::string(Line).find('\t') + 1);
		bool bReplaced = false;
		for (std::string& S : L)
		{
			if (S.compare(0, Key.size(), Key) == 0)
			{
				S = Line;
				bReplaced = true;
			}
		}
		if (!bReplaced) L.push_back(Line);
		CHECK(!DeserializeGame(Resign(Join(L)), Out, Info));
	}
	std::vector<std::string> L = Lines(Base);
	L.pop_back();
	for (std::string& S : L)
	{
		if (S.compare(0, 8, "version\t") == 0) S = "version\t99";
	}
	CHECK(!DeserializeGame(Resign(Join(L)), Out, Info));
	CHECK(Info.Status == ELoadStatus::UnsupportedVersion);
}

TEST(Settings_AccessibilityChoicesPersistAndAreClamped)
{
	GameState G = NewGame("S", EDifficulty::Analyst, 8);
	G.Settings.TextScale = 1.6;
	G.Settings.bReducedMotion = true;
	G.Settings.bHints = false;
	G.Settings.MasterVolume = 0.0;
	G.Settings.MouseSensitivity = 0.4;
	G.Settings.GraphicsQuality = 0;
	G.Settings.bFullscreen = false;
	const GameState L = Reload(G);
	CHECK_NEAR(L.Settings.TextScale, 1.6, 1e-12);
	CHECK(L.Settings.bReducedMotion);
	CHECK(!L.Settings.bHints);
	CHECK_NEAR(L.Settings.MasterVolume, 0.0, 1e-12);
	CHECK_NEAR(L.Settings.MouseSensitivity, 0.4, 1e-12);
	CHECK_EQ(L.Settings.GraphicsQuality, 0);
	CHECK(!L.Settings.bFullscreen);

	PlayerSettings Wild;
	Wild.TextScale = 9.0;
	Wild.MasterVolume = -3.0;
	Wild.MouseSensitivity = std::nan("");
	Wild.GraphicsQuality = 12;
	const PlayerSettings C = ClampSettings(Wild);
	CHECK_NEAR(C.TextScale, 1.6, 1e-12);
	CHECK_NEAR(C.MasterVolume, 0.0, 1e-12);
	CHECK_NEAR(C.MouseSensitivity, 1.0, 1e-12); // non-finite: default
	CHECK_EQ(C.GraphicsQuality, 3);

	// A hand-edited save with a huge text scale loads clamped instead of breaking the UI.
	std::vector<std::string> Text = Lines(SerializeGame(G));
	Text.pop_back();
	for (std::string& S : Text)
	{
		if (S.compare(0, 14, "settings.text\t") == 0) S = "settings.text\t40";
	}
	GameState Edited;
	LoadResult Info;
	CHECK(DeserializeGame(Resign(Join(Text)), Edited, Info));
	CHECK_NEAR(Edited.Settings.TextScale, 1.6, 1e-12);
}

TEST(Content_EveryFileIsValidatedAndABreakInAnyFileIsReported)
{
	const auto Files = MissionFiles();
	REQUIRE(Files.size() == size_t(52)); // 29 missions, 6 case studies, 12 crises, capstone, 4 story chapters
	MissionCatalogue C;
	std::vector<std::string> Errors;
	CHECK(LoadMissionCatalogue(Files, C, Errors));
	CHECK(Errors.empty());
	// Breaking any one file (a required key renamed) fails the load and names that file.
	int Reported = 0;
	for (size_t i = 0; i < Files.size(); ++i)
	{
		auto Broken = Files;
		std::string& Text = Broken[i].second;
		const size_t P = Text.find("\"weight\"");
		REQUIRE(P != std::string::npos);
		Text.replace(P, 8, "\"wieght\"");
		Errors.clear();
		MissionCatalogue Out;
		const bool bOk = LoadMissionCatalogue(Broken, Out, Errors);
		const bool bNamed = std::any_of(Errors.begin(), Errors.end(), [&](const std::string& E) { return E.find(Files[i].first) != std::string::npos; });
		CHECK(!bOk);
		CHECK(bNamed);
		Reported += !bOk && bNamed ? 1 : 0;
	}
	CHECK_EQ(Reported, 52);
}

TEST(Campaign_ReplayIsDeterministicWithSaveLoadBetweenEveryStep)
{
	const MissionCatalogue C = Catalogue();
	for (const EDifficulty D : {EDifficulty::Analyst, EDifficulty::Professional, EDifficulty::Executive})
	{
		for (uint64_t Seed : {3ULL, 77ULL})
		{
			const GameState A = PlayCampaign(C, Seed, D, false, 60);
			const GameState B = PlayCampaign(C, Seed, D, false, 60);
			const GameState Reloaded = PlayCampaign(C, Seed, D, true, 60);
			CHECK(SerializeGame(A) == SerializeGame(B));
			CHECK(SerializeGame(A) == SerializeGame(Reloaded));
			CHECK(A.Campaign.Records.size() > size_t(5));
		}
	}
	// Different seeds give different games (incident draws differ).
	CHECK(SerializeGame(PlayCampaign(C, 3, EDifficulty::Professional, false, 60)) != SerializeGame(PlayCampaign(C, 4, EDifficulty::Professional, false, 60)));
}
