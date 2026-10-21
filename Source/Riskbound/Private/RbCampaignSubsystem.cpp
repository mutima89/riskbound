#include "RbCampaignSubsystem.h"

#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RbLog.h"
#include "RbMissionData.h"
#include "RbConvert.h"
#include "RbGameSubsystem.h"
#include "RbAudioSubsystem.h"

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
	case rb::EMissionError::EvidenceElsewhere: return ERbMissionResult::EvidenceElsewhere;
	case rb::EMissionError::PresentInPerson: return ERbMissionResult::PresentInPerson;
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
	// Player-made missions (Docs/Modding.md): added only if the whole catalogue still validates.
	ModCount = 0;
	ModProblems.Reset();
	const FString ModDir = FPaths::ProjectSavedDir() / TEXT("Mods/Missions");
	TArray<FString> ModNames;
	IFileManager::Get().FindFiles(ModNames, *(ModDir / TEXT("*.json")), true, false);
	if (bOk && !ModNames.IsEmpty())
	{
		std::vector<std::pair<std::string, std::string>> All = Files;
		ModNames.Sort();
		for (const FString& Name : ModNames)
		{
			FString Text;
			if (FFileHelper::LoadFileToString(Text, *(ModDir / Name))) All.push_back({ToStd(Name), ToStd(Text)});
		}
		rb::MissionCatalogue WithMods;
		std::vector<std::string> ModErrs;
		if (rb::LoadMissionCatalogue(All, WithMods, ModErrs))
		{
			ModCount = static_cast<int32>(WithMods.Missions.size() - Loaded.Missions.size());
			Loaded = MoveTemp(WithMods);
		}
		ModProblems = ToFStrings(ModErrs);
		for (const FString& E : ModProblems) UE_LOG(LogRiskbound, Warning, TEXT("Mod missions: %s"), *E);
	}
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
	LoadWorld();
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
		const rb::MissionAvailability A = rb::CheckAvailability(S, M, &Catalogue);
		V.bAvailable = A.bAvailable;
		V.bReview = A.bReview;
		V.Reasons = ToFStrings(A.Reasons);
		V.bActive = S.Campaign.ActiveMissionId == M.Id;
		V.bPassed = S.Career.HasPassed(M.Id);
		V.WeeksToEscalate = V.bAvailable ? rb::WeeksUntilEscalation(S, M) : -1;
		for (const rb::MissionRecord& R : S.Campaign.Records)
		{
			if (R.MissionId != M.Id) continue;
			++V.Attempts;
			V.BestScore = FMath::Max(V.BestScore, R.Score);
		}
		const bool bUntriggered = std::any_of(M.RequiresFlags.begin(), M.RequiresFlags.end(), [&S](const std::string& F) { return !S.Org.HasFlag(F); });
		// Story chapters stay secret until the player has the evidence to open them.
		const bool bUndiscovered = M.RequiresClues > static_cast<int>(S.World.Clues.size());
		if (!bIncludeHidden && (bUntriggered || bUndiscovered) && V.Attempts == 0 && !V.bActive && !V.bAvailable) continue; // a surprise, not a to-do
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
	V.PresentAt = ToFString(M->PresentAt);
	V.PresentAtName = ToFString(rb::LocationName(M->PresentAt));
	for (const rb::MissionEvidenceDef& E : M->Evidence)
	{
		const bool bViewed = V.bActive && std::find(S.Campaign.ViewedEvidence.begin(), S.Campaign.ViewedEvidence.end(), E.Id) != S.Campaign.ViewedEvidence.end();
		FRbMissionEvidenceView EV{ToFString(E.Id), ToFString(E.Title), ToFString(E.Body), E.bRequired, bViewed};
		EV.At = ToFString(E.At);
		EV.AtName = ToFString(rb::LocationName(E.At));
		EV.PersonName = E.Person.empty() ? FString() : PersonName(E.Person);
		V.Evidence.Add(EV);
	}
	for (const rb::MissionQuestionDef& Q : M->Questions)
	{
		FRbMissionQuestionView QV;
		QV.Id = ToFString(Q.Id);
		QV.Prompt = ToFString(Q.Prompt);
		QV.bMulti = Q.Kind == rb::EQuestionKind::Multi;
		QV.Kind = ToFString(rb::ToString(Q.Kind));
		QV.Unit = ToFString(Q.Unit);
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

bool URbCampaignSubsystem::IsStuck() const
{
	return !Catalogue.Missions.empty() && rb::IsCampaignStuck(Game()->GetState(), Catalogue);
}

FString URbCampaignSubsystem::GetActiveMissionId() const
{
	return ToFString(Game()->GetState().Campaign.ActiveMissionId);
}

ERbMissionResult URbCampaignSubsystem::StartMission(const FString& MissionId)
{
	const rb::MissionDef* M = Catalogue.Find(ToStd(MissionId));
	if (!M) return ERbMissionResult::UnknownMission;
	const ERbMissionResult R = FromSimError(rb::StartMission(Game()->EditState(), *M, &Catalogue));
	if (R == ERbMissionResult::Ok) Game()->StateCheckpoint();
	return R;
}

ERbMissionResult URbCampaignSubsystem::ViewEvidence(const FString& MissionId, const FString& EvidenceId, const FString& Where)
{
	const rb::MissionDef* M = Catalogue.Find(ToStd(MissionId));
	if (!M) return ERbMissionResult::UnknownMission;
	const ERbMissionResult R = FromSimError(rb::ViewMissionEvidence(Game()->EditState(), *M, ToStd(EvidenceId), ToStd(Where)));
	if (R == ERbMissionResult::Ok) Game()->StateEdited();
	return R;
}

ERbMissionResult URbCampaignSubsystem::AbandonMission()
{
	Drafts.Remove(GetActiveMissionId());
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

FString URbCampaignSubsystem::InPersonObjective() const
{
	const rb::GameState& S = Game()->GetState();
	const rb::MissionDef* M = Catalogue.Find(S.Campaign.ActiveMissionId);
	if (!M) return FString();
	for (const rb::MissionEvidenceDef& E : M->Evidence)
	{
		if (E.At.empty() || std::find(S.Campaign.ViewedEvidence.begin(), S.Campaign.ViewedEvidence.end(), E.Id) != S.Campaign.ViewedEvidence.end()) continue;
		const FString Room = ToFString(rb::LocationName(E.At));
		return E.Person.empty() ? FString::Printf(TEXT("Go to the %s: %s"), *Room.ToLower(), *ToFString(E.Title))
			: FString::Printf(TEXT("Go to the %s and talk to %s"), *Room.ToLower(), *PersonName(E.Person));
	}
	if (!M->PresentAt.empty())
	{
		return FString::Printf(TEXT("Answer at your workstation, then present at the lectern in the %s"), *ToFString(rb::LocationName(M->PresentAt)).ToLower());
	}
	return FString();
}

ERbMissionResult URbCampaignSubsystem::SubmitMission(const FString& MissionId, const TArray<FRbMissionAnswer>& Answers, const FString& Where)
{
	const rb::MissionDef* M = Catalogue.Find(ToStd(MissionId));
	if (!M) return ERbMissionResult::UnknownMission;
	rb::MissionDebrief D;
	const ERbMissionResult R = FromSimError(rb::SubmitMission(Game()->EditState(), *M, ToSimAnswers(Answers), &D, &Catalogue, &World, ToStd(Where)));
	if (R != ERbMissionResult::Ok) return R;
	URbAudioSubsystem::Cue(this, D.bPassed ? ERbCue::MissionPassed : ERbCue::MissionFailed);
	Drafts.Remove(MissionId);
	SessionDebriefs.Add(MissionId, TPair<int32, FRbMissionDebriefView>(static_cast<int32>(Game()->GetState().Campaign.Records.size()), ToView(*M, D, true)));
	if (D.bPromoted)
	{
		Game()->Notify(FString::Printf(TEXT("Promoted to %s"), *ToFString(rb::LevelDef(D.Level).Title)));
		PendingCeremony = ToFString(rb::LevelDef(D.Level).Title);
		URbAudioSubsystem::Cue(this, ERbCue::Promotion);
	}
	if (D.bPassed && M->Kind == rb::EMissionKind::Capstone) PendingCeremony = TEXT("CRO");
	if (Game()->GetState().Challenge.bOver) bChallengeOverPending = true;
	AnnounceWorldUpdate(D.NewMessages, D.Escalated, D.bBurnout);
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
	V.bReview = D.bReview;
	if (bFull)
	{
		for (const rb::ResolvedConsequence& R : D.Resolved) V.Consequences.Add(ConsequenceText(R));
		V.bPromoted = D.bPromoted;
		V.Level = D.Level;
		V.LevelTitle = ToFString(rb::LevelDef(D.Level).Title);
		V.PromotionGaps = ToFStrings(D.PromotionGaps);
		V.Fallout = ToFStrings(D.Fallout);
		for (const std::string& Id : D.Escalated)
		{
			const rb::MissionDef* E = Catalogue.Find(Id);
			V.Escalated.Add(E ? ToFString(E->Title) : ToFString(Id));
		}
		V.NewMessages = static_cast<int32>(D.NewMessages.size());
		V.bBurnout = D.bBurnout;
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
		rb::EMissionError::UnansweredQuestion, rb::EMissionError::UnknownOption, rb::EMissionError::SingleNeedsOneOption,
		rb::EMissionError::EvidenceElsewhere, rb::EMissionError::PresentInPerson};
	return ToFString(rb::ToString(Map[static_cast<int32>(Result)]));
}

// ---------------------------------------------------------------- living world

void URbCampaignSubsystem::LoadWorld()
{
	World = rb::WorldContent();
	const FString Dir = FPaths::ProjectDir() / TEXT("Data");
	auto Read = [&Dir, this](const TCHAR* Name, std::string& Out)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *(Dir / Name)))
		{
			LoadErrors.Add(FString(Name) + TEXT(": could not be read"));
			return false;
		}
		Out = ToStd(Text);
		return true;
	};
	std::vector<std::string> Errors;
	std::string People, Story, Events;
	rb::WorldContent Loaded;
	if (Read(TEXT("People.json"), People)) rb::PeopleFromJson(People, "People.json", Loaded.People, Errors);
	if (Read(TEXT("Story.json"), Story)) rb::StoryFromJson(Story, "Story.json", Loaded.Story, Errors);
	if (Read(TEXT("Events.json"), Events)) rb::EventsFromJson(Events, "Events.json", Loaded.Events, Loaded.Fallout, Errors);
	for (const std::string& E : rb::ValidateWorld(Loaded, &Catalogue)) Errors.push_back(E);
	for (const std::string& E : Errors) UE_LOG(LogRiskbound, Error, TEXT("World data: %s"), *ToFString(E));
	LoadErrors.Append(ToFStrings(Errors));
	// Like missions: broken world content loads nothing rather than half a world.
	if (Errors.empty() && !People.empty() && !Story.empty() && !Events.empty()) World = MoveTemp(Loaded);
	Dialogue = rb::DialogueCatalogue();
	std::string DialogueText;
	std::vector<std::string> DErrors;
	if (Read(TEXT("Dialogue.json"), DialogueText)) rb::DialogueFromJson(DialogueText, "Dialogue.json", Dialogue, DErrors);
	for (const std::string& E : rb::ValidateDialogue(Dialogue, World.People, World.Story)) DErrors.push_back(E);
	for (const std::string& E : DErrors) UE_LOG(LogRiskbound, Error, TEXT("Dialogue data: %s"), *ToFString(E));
	LoadErrors.Append(ToFStrings(DErrors));
	if (!DErrors.empty()) Dialogue = rb::DialogueCatalogue();
	Glossary.clear();
	Endings.clear();
	std::string GlossaryText, EndingsText;
	std::vector<std::string> LErrors;
	if (Read(TEXT("Glossary.json"), GlossaryText)) rb::GlossaryFromJson(GlossaryText, "Glossary.json", Glossary, LErrors);
	if (Read(TEXT("Endings.json"), EndingsText)) rb::EndingsFromJson(EndingsText, "Endings.json", Endings, LErrors);
	for (const std::string& E : rb::ValidateGlossary(Glossary, &Catalogue)) LErrors.push_back(E);
	for (const std::string& E : rb::ValidateEndings(Endings)) LErrors.push_back(E);
	for (const std::string& E : LErrors) UE_LOG(LogRiskbound, Error, TEXT("Learning data: %s"), *ToFString(E));
	LoadErrors.Append(ToFStrings(LErrors));
	UE_LOG(LogRiskbound, Display, TEXT("World data: %d people, %d clues, %d events"), static_cast<int32>(World.People.People.size()),
		static_cast<int32>(World.Story.Clues.size()), static_cast<int32>(World.Events.Events.size()));
}

void URbCampaignSubsystem::InitWorldFor(rb::GameState& State) const
{
	rb::InitWorld(State, World);
}

void URbCampaignSubsystem::RunWorldUpdate(const std::vector<rb::ResolvedConsequence>& Resolved, int32 Weeks)
{
	const rb::WorldUpdate U = rb::UpdateWorld(Game()->EditState(), &Catalogue, World, Resolved, Weeks);
	AnnounceWorldUpdate(U.NewMessages, U.Escalated, U.bBurnout);
}

void URbCampaignSubsystem::AnnounceWorldUpdate(const std::vector<std::string>& NewMessages, const std::vector<std::string>& Escalated, bool bBurnout)
{
	const rb::GameState& S = Game()->GetState();
	int32 Incidents = 0, Replies = 0;
	for (const std::string& Id : NewMessages)
	{
		const rb::InboxMessage* M = rb::FindMessage(S, Id);
		if (!M) continue;
		Incidents += M->Category == rb::EMessageCategory::Incident ? 1 : 0;
		Replies += M->NeedsReply() ? 1 : 0;
	}
	if (bBurnout)
	{
		Game()->Notify(TEXT("Stress forced you to take two weeks off. Check your inbox."));
		URbAudioSubsystem::Cue(this, ERbCue::Burnout);
	}
	for (const std::string& Id : Escalated)
	{
		const rb::MissionDef* M = Catalogue.Find(Id);
		Game()->Notify(FString::Printf(TEXT("Escalated without you: %s"), M ? *ToFString(M->Title) : *ToFString(Id)));
	}
	if (Incidents > 0 || !Escalated.empty()) URbAudioSubsystem::Cue(this, ERbCue::Incident);
	if (!NewMessages.empty())
	{
		FString Text = NewMessages.size() == 1 ? FString(TEXT("1 new message in your inbox")) : FString::Printf(TEXT("%d new messages in your inbox"), static_cast<int32>(NewMessages.size()));
		if (Replies > 0) Text += FString::Printf(TEXT(" (%d need%s a reply)"), Replies, Replies == 1 ? TEXT("s") : TEXT(""));
		Game()->Notify(Text + TEXT(". Press I to read."));
		if (Incidents == 0) URbAudioSubsystem::Cue(this, ERbCue::Message);
	}
}

FString URbCampaignSubsystem::PersonName(const std::string& Id) const
{
	if (Id == "monitoring") return TEXT("Risk monitoring");
	const rb::CharacterDef* C = World.People.Find(Id);
	return C ? ToFString(C->Name) : TEXT("Valtren");
}

FRbInboxMessageView URbCampaignSubsystem::ToView(const rb::InboxMessage& M) const
{
	const rb::GameState& S = Game()->GetState();
	FRbInboxMessageView V;
	V.Id = ToFString(M.Id);
	V.Category = ToFString(rb::ToString(M.Category));
	V.FromName = M.Category == rb::EMessageCategory::News ? FString(TEXT("Newswire")) : PersonName(M.From);
	if (const rb::CharacterDef* C = World.People.Find(M.From)) V.FromRole = ToFString(C->Role);
	V.Subject = ToFString(M.Subject);
	V.Body = ToFString(M.Body);
	V.Week = M.Week;
	V.bRead = M.bRead;
	V.bNeedsReply = M.NeedsReply();
	V.WeeksLeft = V.bNeedsReply ? FMath::Max(0, M.DeadlineWeek - S.Org.Week) : -1;
	V.Outcome = ToFString(M.Outcome);
	V.bAutoResolved = M.bAutoResolved;
	if (const rb::EventDef* E = World.Events.Find(M.EventId))
	{
		for (const rb::EventChoiceDef& C : E->Choices)
		{
			if (C.Id == M.ChoiceId) V.ChosenLabel = ToFString(C.Label);
			if (C.Label.empty() || C.Label[0] != '(') V.Choices.Add({ToFString(C.Id), ToFString(C.Label)});
		}
	}
	return V;
}

TArray<FRbInboxMessageView> URbCampaignSubsystem::GetInbox() const
{
	TArray<FRbInboxMessageView> Out;
	const rb::GameState& S = Game()->GetState();
	for (auto It = S.World.Inbox.rbegin(); It != S.World.Inbox.rend(); ++It) Out.Add(ToView(*It));
	// Replies due soonest first, then newest.
	Out.StableSort([](const FRbInboxMessageView& A, const FRbInboxMessageView& B)
	{
		if (A.bNeedsReply != B.bNeedsReply) return A.bNeedsReply;
		if (A.bNeedsReply && A.WeeksLeft != B.WeeksLeft) return A.WeeksLeft < B.WeeksLeft;
		return false;
	});
	return Out;
}

bool URbCampaignSubsystem::ReplyToMessage(const FString& MessageId, const FString& ChoiceId, FString& OutError)
{
	const rb::EReplyError E = rb::ReplyToMessage(Game()->EditState(), World, ToStd(MessageId), ToStd(ChoiceId));
	if (E != rb::EReplyError::None)
	{
		OutError = ToFString(rb::ToString(E));
		return false;
	}
	OutError.Reset();
	URbAudioSubsystem::Cue(this, ERbCue::Confirm);
	Game()->StateCheckpoint();
	return true;
}

void URbCampaignSubsystem::MarkMessageRead(const FString& MessageId)
{
	if (rb::MarkMessageRead(Game()->EditState(), ToStd(MessageId))) Game()->StateEdited();
}

TArray<FRbPersonView> URbCampaignSubsystem::GetPeople() const
{
	TArray<FRbPersonView> Out;
	const rb::GameState& S = Game()->GetState();
	for (const rb::CharacterDef& C : World.People.People)
	{
		FRbPersonView V;
		V.Id = ToFString(C.Id);
		V.Name = ToFString(C.Name);
		V.Role = ToFString(C.Role);
		V.Bio = ToFString(C.Bio);
		V.Relation = rb::GetRelation(S, C.Id);
		V.RelationLabel = ToFString(rb::RelationLabel(V.Relation));
		V.bExecutive = C.bExecutive;
		V.bPersonal = C.bPersonal;
		V.Faction = ToFString(C.Faction);
		Out.Add(V);
	}
	Out.StableSort([](const FRbPersonView& A, const FRbPersonView& B)
	{
		const int32 GA = A.bPersonal ? 2 : (A.bExecutive ? 0 : 1), GB = B.bPersonal ? 2 : (B.bExecutive ? 0 : 1);
		return GA < GB;
	});
	return Out;
}

TArray<FRbClueView> URbCampaignSubsystem::GetClues() const
{
	TArray<FRbClueView> Out;
	const rb::GameState& S = Game()->GetState();
	for (const rb::ClueDef& C : World.Story.Clues)
	{
		if (rb::HasClue(S, C.Id)) Out.Add({ToFString(C.Title), ToFString(C.Summary), C.Chapter});
	}
	return Out;
}

FRbWorldView URbCampaignSubsystem::GetWorldView() const
{
	FRbWorldView V;
	const URbGameSubsystem* G = Game();
	if (!G || !G->HasGame()) return V;
	const rb::GameState& S = G->GetState();
	V.bValid = true;
	V.Week = S.Org.Week;
	V.Stress = S.World.Stress;
	V.Burnouts = S.World.Burnouts;
	V.ShareIndex = rb::CurrentShareIndex(S);
	if (S.World.Shares.size() >= 2) V.ShareChange = S.World.Shares.back().Value - S.World.Shares[S.World.Shares.size() - 2].Value;
	for (const rb::SharePoint& P : S.World.Shares) V.ShareHistory.Add(static_cast<float>(P.Value));
	for (auto It = S.World.News.rbegin(); It != S.World.News.rend(); ++It) V.News.Add({It->Week, ToFString(It->Text), It->Tone});
	V.Unread = rb::UnreadCount(S);
	V.AwaitingReply = rb::AwaitingReplyCount(S);
	V.StoryTitle = ToFString(World.Story.Title);
	V.CluesFound = static_cast<int32>(S.World.Clues.size());
	V.CluesTotal = static_cast<int32>(World.Story.Clues.size());
	V.Morale = S.Org.Get(rb::EOrgMetric::EmployeeMorale);
	V.Reputation = S.Org.Get(rb::EOrgMetric::Reputation);
	return V;
}

// ---------------------------------------------------------------- phase 3: growth

TArray<FRbSkillView> URbCampaignSubsystem::GetSkills() const
{
	TArray<FRbSkillView> Out;
	const rb::GameState& S = Game()->GetState();
	const bool bPoints = rb::SkillPointsAvailable(S) > 0;
	for (const rb::SkillDef& D : rb::SkillDefs())
	{
		FRbSkillView V;
		V.Id = D.Id;
		V.Name = D.Name;
		V.Branch = D.Branch;
		V.Description = D.Description;
		if (const rb::SkillDef* Req = D.Requires[0] ? rb::FindSkill(D.Requires) : nullptr) V.RequiresName = Req->Name;
		V.bLearned = rb::HasSkill(S, D.Id);
		V.bCanLearn = !V.bLearned && bPoints && (D.Requires[0] == '\0' || rb::HasSkill(S, D.Requires));
		Out.Add(V);
	}
	return Out;
}

int32 URbCampaignSubsystem::GetSkillPoints() const
{
	return rb::SkillPointsAvailable(Game()->GetState());
}

bool URbCampaignSubsystem::LearnSkill(const FString& SkillId, FString& OutError)
{
	const rb::ESkillError E = rb::LearnSkill(Game()->EditState(), ToStd(SkillId));
	if (E != rb::ESkillError::None)
	{
		OutError = ToFString(rb::ToString(E));
		return false;
	}
	OutError.Reset();
	URbAudioSubsystem::Cue(this, ERbCue::Confirm);
	Game()->StateCheckpoint();
	return true;
}

TArray<FRbBackgroundView> URbCampaignSubsystem::GetBackgrounds() const
{
	TArray<FRbBackgroundView> Out;
	for (const rb::BackgroundDef& B : rb::BackgroundDefs())
	{
		FRbBackgroundView V;
		V.Id = B.Id;
		V.Name = B.Name;
		V.Description = B.Description;
		if (const rb::SkillDef* S = B.StartingSkill[0] ? rb::FindSkill(B.StartingSkill) : nullptr) V.StartingSkill = S->Name;
		Out.Add(V);
	}
	return Out;
}

FString URbCampaignSubsystem::GetBackgroundName() const
{
	const rb::BackgroundDef* B = rb::FindBackground(Game()->GetState().World.Background);
	return B ? FString(B->Name) : FString();
}

FRbConversationView URbCampaignSubsystem::NodeView(const rb::ConversationDef& C, const std::string& NodeId) const
{
	FRbConversationView V;
	V.bFound = true;
	V.Id = ToFString(C.Id);
	V.PersonName = PersonName(C.Person);
	if (const rb::CharacterDef* P = World.People.Find(C.Person)) V.PersonRole = ToFString(P->Role);
	V.Title = ToFString(C.Title);
	const auto Node = std::find_if(C.Nodes.begin(), C.Nodes.end(), [&](const rb::DialogueNodeDef& N) { return N.Id == NodeId; });
	if (Node == C.Nodes.end())
	{
		V.bEnded = true;
		return V;
	}
	V.NodeId = ToFString(Node->Id);
	V.Line = ToFString(Node->Line);
	for (const rb::DialogueResponseDef* R : rb::AvailableResponses(Game()->GetState(), *Node))
	{
		FRbDialogueResponseView RV;
		RV.Id = ToFString(R->Id);
		RV.Label = ToFString(R->Label);
		RV.Tone = ToFString(rb::ToString(R->Tone));
		if (const rb::SkillDef* S = R->RequiresSkill.empty() ? nullptr : rb::FindSkill(R->RequiresSkill)) RV.SkillName = S->Name;
		V.Responses.Add(RV);
	}
	return V;
}

FRbConversationView URbCampaignSubsystem::StartConversation(const FString& Person, const FString& Trigger) const
{
	const rb::ConversationDef* C = rb::ConversationFor(Game()->GetState(), Dialogue, ToStd(Person), ToStd(Trigger));
	if (!C || C->Nodes.empty()) return FRbConversationView();
	return NodeView(*C, C->Nodes.front().Id);
}

FRbConversationView URbCampaignSubsystem::ChooseResponse(const FString& ConversationId, const FString& NodeId, const FString& ResponseId, FString& OutError)
{
	std::string Next;
	const rb::EDialogueError E = rb::ChooseResponse(Game()->EditState(), Dialogue, ToStd(ConversationId), ToStd(NodeId), ToStd(ResponseId), Next);
	if (E != rb::EDialogueError::None)
	{
		OutError = ToFString(rb::ToString(E));
		return FRbConversationView();
	}
	OutError.Reset();
	Game()->StateEdited();
	const rb::ConversationDef* C = Dialogue.Find(ToStd(ConversationId));
	if (!C || Next.empty())
	{
		FRbConversationView Ended;
		Ended.bFound = true;
		Ended.bEnded = true;
		Ended.Id = ConversationId;
		return Ended;
	}
	return NodeView(*C, Next);
}

// ---------------------------------------------------------------- phase 4

std::string URbCampaignSubsystem::RbConvertToStd(const FString& S)
{
	return ToStd(S);
}

FString URbCampaignSubsystem::ExportCertificate() const
{
	const FString Date = FDateTime::Now().ToString(TEXT("%Y-%m-%d"));
	const std::string Text = rb::CertificateMarkdown(Game()->GetState(), GetReview(), GetEnding(), ToStd(Date));
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Riskbound-Certificate-%s.md"), *Date));
	return FFileHelper::SaveStringToFile(ToFString(Text), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ? Path : FString();
}

TArray<FString> URbCampaignSubsystem::WhatIfLines(const FString& MissionId) const
{
	TArray<FString> Out;
	const rb::MissionDef* M = Catalogue.Find(ToStd(MissionId));
	const rb::GameState& S = Game()->GetState();
	const rb::MissionRecord* Last = nullptr;
	for (const rb::MissionRecord& R : S.Campaign.Records)
	{
		if (M && R.MissionId == M->Id) Last = &R;
	}
	if (!M || !Last) return Out;
	for (const rb::WhatIfQuestion& Q : rb::WhatIf(*M, *Last))
	{
		Out.Add(TEXT("Q: ") + ToFString(Q.Prompt));
		if (!Q.YourAnswer.empty()) Out.Add(FString::Printf(TEXT("    Your answer: %s"), *ToFString(Q.YourAnswer)));
		for (const rb::WhatIfOption& O : Q.Options)
		{
			FString Line = FString::Printf(TEXT("    %s %s  (%d)"), O.bChosen ? TEXT("[you]") : TEXT("     "), *ToFString(O.Label), O.Score);
			for (const std::string& C : O.Consequences) Line += TEXT("  ->  ") + ToFString(C);
			Out.Add(Line);
		}
	}
	return Out;
}

static FString LeaderboardPath()
{
	return FPaths::ProjectSavedDir() / TEXT("Leaderboard.txt");
}

TArray<FString> URbCampaignSubsystem::LeaderboardLines() const
{
	FString Text;
	FFileHelper::LoadFileToString(Text, *LeaderboardPath());
	TArray<FString> Out;
	for (const rb::LeaderboardEntry& E : rb::DeserializeLeaderboard(ToStd(Text)))
	{
		Out.Add(FString::Printf(TEXT("%-9s %4d   %s   %s   %s"), *ToFString(E.Mode), E.Points, *ToFString(E.Profile), *ToFString(E.Detail), *ToFString(E.When)));
	}
	return Out;
}

void URbCampaignSubsystem::RecordOnLeaderboard(const FString& Mode, const FString& Detail, int32 Points)
{
	FString Text;
	FFileHelper::LoadFileToString(Text, *LeaderboardPath());
	std::vector<rb::LeaderboardEntry> Entries = rb::DeserializeLeaderboard(ToStd(Text));
	const rb::GameState& S = Game()->GetState();
	rb::AddToLeaderboard(Entries, {ToStd(Mode), S.ProfileName, ToStd(Detail), Points, S.Org.Week, ToStd(FDateTime::Now().ToString(TEXT("%Y-%m-%d")))});
	FFileHelper::SaveStringToFile(ToFString(rb::SerializeLeaderboard(Entries)), *LeaderboardPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void URbCampaignSubsystem::StartChallengeGame(rb::EGameMode Mode)
{
	EndClassroom();
	Game()->NewGame(ERbDifficulty::Professional, Mode == rb::EGameMode::Survival ? TEXT("Survivor") : TEXT("Daily"));
	rb::GameState& S = Game()->EditState();
	rb::StartChallenge(S, Catalogue, Mode, ToStd(FDateTime::Now().ToString(TEXT("%Y-%m-%d"))));
	S.Checkpoint = "challenge";
	Game()->StateCheckpoint();
}

FString URbCampaignSubsystem::ChallengeStatus() const
{
	const rb::ChallengeState& C = Game()->GetState().Challenge;
	if (C.Mode == rb::EGameMode::Career) return FString();
	if (IsClassroom()) return FString::Printf(TEXT("Classroom: %s's turn (%d of %d)"), *ClassroomCurrentPlayer(), ClassIndex + 1, ClassPlayers.Num());
	const FString Mission = ToFString(rb::CurrentChallengeMission(Game()->GetState()));
	const rb::MissionDef* M = Catalogue.Find(ToStd(Mission));
	if (C.Mode == rb::EGameMode::Survival)
	{
		return C.bOver ? FString::Printf(TEXT("Survival over: %d crises handled"), C.Points)
			: FString::Printf(TEXT("Survival: crisis %d of %d, %d lives  -  %s"), C.Index + 1, static_cast<int32>(C.Sequence.size()), C.Lives, M ? *ToFString(M->Title) : TEXT(""));
	}
	return C.bOver ? FString::Printf(TEXT("Daily %s complete: %d points"), *ToFString(C.Date), C.Points)
		: FString::Printf(TEXT("Daily %s: case %d of %d, %d points  -  %s"), *ToFString(C.Date), C.Index + 1, static_cast<int32>(C.Sequence.size()), C.Points, M ? *ToFString(M->Title) : TEXT(""));
}

void URbCampaignSubsystem::StartClassroom(const TArray<FString>& Players, const FString& CaseId)
{
	ClassPlayers = Players;
	ClassResults.Reset();
	ClassCase = CaseId;
	ClassIndex = 0;
	Game()->NewGame(ERbDifficulty::Professional, ClassroomCurrentPlayer());
	rb::GameState& S = Game()->EditState();
	S.Seed = 20261020; // identical games for every player
	rb::StartCustomChallenge(S, {ToStd(CaseId)});
	S.Checkpoint = "challenge";
	Game()->StateEdited();
}

bool URbCampaignSubsystem::ClassroomNext()
{
	if (!IsClassroom()) return false;
	const rb::GameState& S = Game()->GetState();
	int32 Score = 0;
	for (const rb::MissionRecord& R : S.Campaign.Records) Score = R.Score;
	ClassResults.Add(FString::Printf(TEXT("%s,%s,%d"), *ClassroomCurrentPlayer(), *ClassCase, Score));
	if (++ClassIndex >= ClassPlayers.Num())
	{
		bChallengeOverPending = true;
		return false;
	}
	Game()->NewGame(ERbDifficulty::Professional, ClassroomCurrentPlayer());
	rb::GameState& Next = Game()->EditState();
	Next.Seed = 20261020;
	rb::StartCustomChallenge(Next, {ToStd(ClassCase)});
	Next.Checkpoint = "challenge";
	Game()->StateEdited();
	return true;
}

TArray<FString> URbCampaignSubsystem::ClassroomResults() const
{
	return ClassResults;
}

FString URbCampaignSubsystem::ExportClassroomCsv() const
{
	FString Csv = TEXT("player,case,score\n");
	for (const FString& L : ClassResults) Csv += L + TEXT("\n");
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Riskbound-Classroom-%s.csv"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d-%H%M"))));
	return FFileHelper::SaveStringToFile(Csv, *Path) ? Path : FString();
}

rb::PerformanceReview URbCampaignSubsystem::GetReview() const
{
	return rb::MakeReview(Game()->GetState(), World.People);
}

const rb::EndingDef* URbCampaignSubsystem::GetEnding() const
{
	return rb::ChooseEnding(Game()->GetState(), Endings);
}

bool URbCampaignSubsystem::IsChallenge() const
{
	return Game() && Game()->GetState().Challenge.Mode != rb::EGameMode::Career;
}
