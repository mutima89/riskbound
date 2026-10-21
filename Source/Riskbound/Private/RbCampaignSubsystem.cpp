#include "RbCampaignSubsystem.h"

#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RbLog.h"
#include "RbMissionData.h"
#include "RbConvert.h"
#include "RbGameSubsystem.h"

#include <algorithm>

using namespace RbConvert;

namespace
{
ERbMissionResult FromSimError(rb::EMissionError E)
{
	switch (E)
	{
	case rb::EMissionError::None: return ERbMissionResult::Ok;
	case rb::EMissionError::Unavailable: return ERbMissionResult::Unavailable;
	case rb::EMissionError::AnotherMissionActive: return ERbMissionResult::AnotherMissionActive;
	case rb::EMissionError::NotActive: return ERbMissionResult::NotActive;
	case rb::EMissionError::UnknownEvidence: return ERbMissionResult::UnknownEvidence;
	case rb::EMissionError::RequiredEvidenceNotViewed: return ERbMissionResult::RequiredEvidenceNotViewed;
	case rb::EMissionError::UnansweredQuestion: return ERbMissionResult::UnansweredQuestion;
	case rb::EMissionError::UnknownOption: return ERbMissionResult::UnknownOption;
	case rb::EMissionError::SingleNeedsOneOption: return ERbMissionResult::SingleNeedsOneOption;
	}
	return ERbMissionResult::UnknownOption;
}

std::vector<rb::MissionAnswer> ToSimAnswers(const TArray<FRbMissionAnswer>& In)
{
	std::vector<rb::MissionAnswer> Out;
	for (const FRbMissionAnswer& A : In)
	{
		Out.push_back({ToStd(A.QuestionId), ToStdVector(A.OptionIds)});
	}
	return Out;
}

FString ConsequenceText(const rb::ResolvedConsequence& R)
{
	const FString What = ToFString(R.Description);
	if (R.bOccurred) return FString::Printf(TEXT("Week %d: %s%s"), R.Week, *What, R.bMitigated ? TEXT(" (impact reduced by your controls)") : TEXT(""));
	return FString::Printf(TEXT("Week %d: avoided - %s%s"), R.Week, *What, R.bMitigated ? TEXT(" (your controls prevented it)") : TEXT(""));
}
} // namespace

void URbCampaignSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<URbGameSubsystem>();
	LoadFromDisk();
}

bool URbCampaignSubsystem::LoadFromDisk()
{
	const FString Dir = FPaths::ProjectDir() / TEXT("Data/Missions");
	TArray<FString> Names;
	IFileManager::Get().FindFiles(Names, *(Dir / TEXT("*.json")), true, false);
	Names.Sort();
	std::vector<std::pair<std::string, std::string>> Files;
	LoadErrors.Reset();
	for (const FString& Name : Names)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *(Dir / Name)))
		{
			LoadErrors.Add(Name + TEXT(": could not be read"));
			continue;
		}
		Files.push_back({ToStd(Name), ToStd(Text)});
	}
	rb::MissionCatalogue Loaded;
	std::vector<std::string> Errors;
	const bool bOk = rb::LoadMissionCatalogue(Files, Loaded, Errors) && LoadErrors.IsEmpty();
	LoadErrors.Append(ToFStrings(Errors));
	for (const FString& E : LoadErrors)
	{
		UE_LOG(LogRiskbound, Error, TEXT("Mission data: %s"), *E);
	}
	Catalogue = bOk ? MoveTemp(Loaded) : rb::MissionCatalogue();
	SessionDebriefs.Reset();

	// Achievements reference missions, so they are validated against the catalogue just loaded.
	Achievements.clear();
	FString AchievementText;
	const FString AchievementPath = FPaths::ProjectDir() / TEXT("Data/Achievements.json");
	if (FFileHelper::LoadFileToString(AchievementText, *AchievementPath))
	{
		std::vector<rb::AchievementDef> Defs;
		std::vector<std::string> AErrors;
		rb::AchievementsFromJson(ToStd(AchievementText), "Achievements.json", Defs, AErrors);
		for (const std::string& E : rb::ValidateAchievements(Defs, Catalogue)) AErrors.push_back(E);
		for (const std::string& E : AErrors) UE_LOG(LogRiskbound, Error, TEXT("Achievement data: %s"), *ToFString(E));
		LoadErrors.Append(ToFStrings(AErrors));
		if (AErrors.empty()) Achievements = std::move(Defs);
	}
	else
	{
		LoadErrors.Add(TEXT("Achievements.json: could not be read"));
	}
	UE_LOG(LogRiskbound, Display, TEXT("Achievement data: %d achievements"), static_cast<int32>(Achievements.size()));
	UE_LOG(LogRiskbound, Display, TEXT("Mission data: %d missions from %s (%d problems)"), static_cast<int32>(Catalogue.Missions.size()), *Dir, LoadErrors.Num());
	return bOk;
}

URbGameSubsystem* URbCampaignSubsystem::Game() const
{
	return GetGameInstance()->GetSubsystem<URbGameSubsystem>();
}

TArray<FString> URbCampaignSubsystem::SetCatalogue(rb::MissionCatalogue&& NewCatalogue)
{
	const TArray<FString> Problems = ToFStrings(rb::ValidateCatalogue(NewCatalogue));
	if (Problems.IsEmpty())
	{
		Catalogue = MoveTemp(NewCatalogue);
		SessionDebriefs.Reset();
	}
	return Problems;
}

TArray<FRbMissionSummary> URbCampaignSubsystem::GetMissions(bool bIncludeHidden) const
{
	TArray<FRbMissionSummary> Out;
	const rb::GameState& S = Game()->GetState();
	for (const rb::MissionDef& M : Catalogue.Missions)
	{
		FRbMissionSummary V;
		V.Id = ToFString(M.Id);
		V.Title = ToFString(M.Title);
		V.Kind = ToFString(rb::ToString(M.Kind));
		V.Department = ToFString(M.Department);
		V.MinLevel = M.MinLevel;
		const rb::MissionAvailability A = rb::CheckAvailability(S, M);
		V.bAvailable = A.bAvailable;
		V.Reasons = ToFStrings(A.Reasons);
		V.bActive = S.Campaign.ActiveMissionId == M.Id;
		V.bPassed = S.Career.HasPassed(M.Id);
		for (const rb::MissionRecord& R : S.Campaign.Records)
		{
			if (R.MissionId != M.Id) continue;
			++V.Attempts;
			V.BestScore = FMath::Max(V.BestScore, R.Score);
		}
		const bool bUntriggered = std::any_of(M.RequiresFlags.begin(), M.RequiresFlags.end(), [&S](const std::string& F) { return !S.Org.HasFlag(F); });
		if (!bIncludeHidden && bUntriggered && V.Attempts == 0 && !V.bActive) continue; // a surprise, not a to-do
		Out.Add(V);
	}
	auto Group = [](const FRbMissionSummary& V) { return V.bActive ? 0 : (V.bAvailable ? 1 : (V.bPassed ? 2 : 3)); };
	Out.StableSort([&Group](const FRbMissionSummary& A, const FRbMissionSummary& B)
	{
		if (Group(A) != Group(B)) return Group(A) < Group(B);
		if (A.MinLevel != B.MinLevel) return A.MinLevel < B.MinLevel;
		return A.Id < B.Id;
	});
	return Out;
}

FRbMissionView URbCampaignSubsystem::GetMission(const FString& MissionId) const
{
	FRbMissionView V;
	const rb::MissionDef* M = Catalogue.Find(ToStd(MissionId));
	if (!M) return V;
	const rb::GameState& S = Game()->GetState();
	V.bFound = true;
	V.Id = MissionId;
	V.Title = ToFString(M->Title);
	V.Kind = ToFString(rb::ToString(M->Kind));
	V.Department = ToFString(M->Department);
	V.Briefing = ToFStrings(rb::BriefingFor(S, *M));
	V.bActive = S.Campaign.ActiveMissionId == M->Id;
	V.DecisionAuthority = rb::LevelDef(S.Career.Level).DecisionAuthority;
	for (const rb::MissionEvidenceDef& E : M->Evidence)
	{
		const bool bViewed = V.bActive && std::find(S.Campaign.ViewedEvidence.begin(), S.Campaign.ViewedEvidence.end(), E.Id) != S.Campaign.ViewedEvidence.end();
		V.Evidence.Add({ToFString(E.Id), ToFString(E.Title), ToFString(E.Body), E.bRequired, bViewed});
	}
	for (const rb::MissionQuestionDef& Q : M->Questions)
	{
		FRbMissionQuestionView QV;
		QV.Id = ToFString(Q.Id);
		QV.Prompt = ToFString(Q.Prompt);
		QV.bMulti = Q.Kind == rb::EQuestionKind::Multi;
		// Shuffled per save (rb::OptionDisplayOrder) so the answer's position in the content gives nothing away.
		for (int Index : rb::OptionDisplayOrder(S.Seed, M->Id, Q.Id, static_cast<int>(Q.Options.size())))
		{
			const rb::MissionOptionDef& O = Q.Options[static_cast<size_t>(Index)];
			QV.Options.Add({ToFString(O.Id), ToFString(O.Label)}); // never the score
		}
		V.Questions.Add(QV);
	}
	return V;
}

FString URbCampaignSubsystem::GetActiveMissionId() const
{
	return ToFString(Game()->GetState().Campaign.ActiveMissionId);
}

ERbMissionResult URbCampaignSubsystem::StartMission(const FString& MissionId)
{
	const rb::MissionDef* M = Catalogue.Find(ToStd(MissionId));
	if (!M) return ERbMissionResult::UnknownMission;
	const ERbMissionResult R = FromSimError(rb::StartMission(Game()->EditState(), *M));
	if (R == ERbMissionResult::Ok) Game()->StateCheckpoint();
	return R;
}

ERbMissionResult URbCampaignSubsystem::ViewEvidence(const FString& MissionId, const FString& EvidenceId)
{
	const rb::MissionDef* M = Catalogue.Find(ToStd(MissionId));
	if (!M) return ERbMissionResult::UnknownMission;
	const ERbMissionResult R = FromSimError(rb::ViewMissionEvidence(Game()->EditState(), *M, ToStd(EvidenceId)));
	if (R == ERbMissionResult::Ok) Game()->StateEdited();
	return R;
}

ERbMissionResult URbCampaignSubsystem::AbandonMission()
{
	const ERbMissionResult R = FromSimError(rb::AbandonMission(Game()->EditState()));
	if (R == ERbMissionResult::Ok) Game()->StateCheckpoint();
	return R;
}

ERbMissionResult URbCampaignSubsystem::ValidateAnswers(const FString& MissionId, const TArray<FRbMissionAnswer>& Answers) const
{
	const rb::MissionDef* M = Catalogue.Find(ToStd(MissionId));
	if (!M) return ERbMissionResult::UnknownMission;
	return FromSimError(rb::ValidateAnswers(Game()->GetState(), *M, ToSimAnswers(Answers)));
}

ERbMissionResult URbCampaignSubsystem::SubmitMission(const FString& MissionId, const TArray<FRbMissionAnswer>& Answers)
{
	const rb::MissionDef* M = Catalogue.Find(ToStd(MissionId));
	if (!M) return ERbMissionResult::UnknownMission;
	rb::MissionDebrief D;
	const ERbMissionResult R = FromSimError(rb::SubmitMission(Game()->EditState(), *M, ToSimAnswers(Answers), &D));
	if (R != ERbMissionResult::Ok) return R;
	SessionDebriefs.Add(MissionId, TPair<int32, FRbMissionDebriefView>(static_cast<int32>(Game()->GetState().Campaign.Records.size()), ToView(*M, D, true)));
	if (D.bPromoted)
	{
		Game()->Notify(FString::Printf(TEXT("Promoted to %s"), *ToFString(rb::LevelDef(D.Level).Title)));
	}
	Game()->StateCheckpoint();
	return R;
}

FRbMissionDebriefView URbCampaignSubsystem::ToView(const rb::MissionDef& M, const rb::MissionDebrief& D, bool bFull) const
{
	FRbMissionDebriefView V;
	V.bAvailable = true;
	V.MissionId = ToFString(M.Id);
	V.Title = ToFString(M.Title);
	V.Score = D.Score;
	V.PassMark = M.PassMark;
	V.bPassed = D.bPassed;
	for (const rb::MissionCriterion& C : D.Criteria)
	{
		FRbMissionCriterionView CV;
		CV.Prompt = ToFString(C.Prompt);
		CV.Competency = ToFString(rb::ToString(C.Competency));
		CV.Score = C.Score;
		CV.Weight = C.Weight;
		CV.bAuthorityExceeded = C.bAuthorityExceeded;
		CV.Feedback = ToFStrings(C.Feedback);
		V.Criteria.Add(CV);
	}
	for (const rb::AssessmentResult& A : D.CompetencyEvidence)
	{
		V.CompetencyEvidence.Add({ToFString(rb::ToString(A.Competency)), A.Score, 1});
	}
	V.XpAwarded = D.XpAwarded;
	V.Lesson = ToFString(D.Lesson);
	if (bFull)
	{
		for (const rb::ResolvedConsequence& R : D.Resolved) V.Consequences.Add(ConsequenceText(R));
		V.bPromoted = D.bPromoted;
		V.Level = D.Level;
		V.LevelTitle = ToFString(rb::LevelDef(D.Level).Title);
		V.PromotionGaps = ToFStrings(D.PromotionGaps);
	}
	return V;
}

FRbMissionDebriefView URbCampaignSubsystem::GetDebrief(const FString& MissionId) const
{
	const rb::MissionDef* M = Catalogue.Find(ToStd(MissionId));
	if (!M) return FRbMissionDebriefView();
	const rb::GameState& S = Game()->GetState();
	const rb::MissionRecord* Latest = nullptr;
	int32 LatestCount = 0;
	for (size_t i = 0; i < S.Campaign.Records.size(); ++i)
	{
		if (S.Campaign.Records[i].MissionId != M->Id) continue;
		Latest = &S.Campaign.Records[i];
		LatestCount = static_cast<int32>(i) + 1;
	}
	if (!Latest) return FRbMissionDebriefView(); // never submitted: the rubric stays hidden
	const TPair<int32, FRbMissionDebriefView>* Session = SessionDebriefs.Find(MissionId);
	if (Session && Session->Key == LatestCount && Session->Value.Score == Latest->Score) return Session->Value;
	FRbMissionDebriefView V = ToView(*M, rb::ScoreMission(*M, Latest->Answers, Latest->Level), false);
	V.Level = S.Career.Level;
	V.LevelTitle = ToFString(rb::LevelDef(S.Career.Level).Title);
	V.PromotionGaps = ToFStrings(S.Career.CheckPromotion().UnmetGates);
	return V;
}

FString URbCampaignSubsystem::ResultText(ERbMissionResult Result)
{
	switch (Result)
	{
	case ERbMissionResult::Ok: return FString();
	case ERbMissionResult::UnknownMission: return TEXT("Unknown mission.");
	default: break;
	}
	static const rb::EMissionError Map[] = {rb::EMissionError::None, rb::EMissionError::Unavailable, rb::EMissionError::AnotherMissionActive,
		rb::EMissionError::NotActive, rb::EMissionError::UnknownEvidence, rb::EMissionError::RequiredEvidenceNotViewed,
		rb::EMissionError::UnansweredQuestion, rb::EMissionError::UnknownOption, rb::EMissionError::SingleNeedsOneOption};
	return ToFString(rb::ToString(Map[static_cast<int32>(Result)]));
}
