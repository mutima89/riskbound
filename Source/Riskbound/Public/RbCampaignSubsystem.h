// Campaign missions after Mission 001 (Gate 3): availability, play, debrief and promotion.
// Thin calls into rb::Campaign (RiskboundSim); the mission catalogue is content, set once per session.
#pragma once

#include "CoreMinimal.h"
#include "RbCampaign.h"
#include "RbGameState.h"
#include "RbWorld.h"
#include "RbGrowth.h"
#include "RbCampaignTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RbCampaignSubsystem.generated.h"

class URbGameSubsystem;

UCLASS()
class RISKBOUND_API URbCampaignSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Replaces the mission catalogue. Returns the validation problems (empty = accepted);
	// an invalid catalogue is rejected and the previous one kept.
	TArray<FString> SetCatalogue(rb::MissionCatalogue&& Catalogue);
	const rb::MissionCatalogue& GetCatalogue() const { return Catalogue; }

	// Reads <project>/Data/Missions/*.json (staged with the game). Called on Initialize.
	// Returns false and keeps no missions if any file is invalid; problems are logged.
	bool LoadFromDisk();
	const TArray<FString>& GetLoadErrors() const { return LoadErrors; }
	// Achievements from Data/Achievements.json (empty if that file is missing or invalid).
	const std::vector<rb::AchievementDef>& GetAchievementDefs() const { return Achievements; }

	// Missions in play order: in progress, available, passed, then locked (each by level). Crises that
	// open only after an incident stay hidden until then, unless bIncludeHidden.
	UFUNCTION(BlueprintPure, Category = "Riskbound|Campaign")
	TArray<FRbMissionSummary> GetMissions(bool bIncludeHidden = false) const;

	UFUNCTION(BlueprintPure, Category = "Riskbound|Campaign")
	FRbMissionView GetMission(const FString& MissionId) const;

	UFUNCTION(BlueprintPure, Category = "Riskbound|Campaign")
	FString GetActiveMissionId() const;

	// True when every case open at the player's level is passed but the promotion bar is not met:
	// passed cases reopen as reviews (rb::IsCampaignStuck).
	UFUNCTION(BlueprintPure, Category = "Riskbound|Campaign")
	bool IsStuck() const;

	UFUNCTION(BlueprintCallable, Category = "Riskbound|Campaign")
	ERbMissionResult StartMission(const FString& MissionId);

	// Where: the HQ location the player is in when examining it ("" at the workstation).
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Campaign")
	ERbMissionResult ViewEvidence(const FString& MissionId, const FString& EvidenceId, const FString& Where = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Riskbound|Campaign")
	ERbMissionResult AbandonMission();

	UFUNCTION(BlueprintCallable, Category = "Riskbound|Campaign")
	ERbMissionResult ValidateAnswers(const FString& MissionId, const TArray<FRbMissionAnswer>& Answers) const;

	// Scores, applies consequences, advances time and runs the promotion review; autosaves.
	// Where: the HQ location of the submission ("" at the workstation); board papers are presented in person.
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Campaign")
	ERbMissionResult SubmitMission(const FString& MissionId, const TArray<FRbMissionAnswer>& Answers, const FString& Where = TEXT(""));

	// What the active case still needs in person, for the objective line ("" if nothing).
	FString InPersonObjective() const;

	// The debrief of the latest attempt at a mission. Consequences and promotion are only
	// present for a submission made in this session; the scores are recomputed from the save.
	UFUNCTION(BlueprintPure, Category = "Riskbound|Campaign")
	FRbMissionDebriefView GetDebrief(const FString& MissionId) const;

	static FString ResultText(ERbMissionResult Result);

	// ---- Living world: people, inbox, story, news and shares (Data/People|Story|Events.json) ----
	const rb::WorldContent& GetWorldContent() const { return World; }
	// Relationship start values for a new or loaded game (rb::InitWorld).
	void InitWorldFor(rb::GameState& State) const;
	// Runs rb::UpdateWorld after time passed outside a campaign mission (Mission 001), with notices.
	void RunWorldUpdate(const std::vector<rb::ResolvedConsequence>& Resolved, int32 Weeks);

	// Newest first.
	UFUNCTION(BlueprintPure, Category = "Riskbound|World")
	TArray<FRbInboxMessageView> GetInbox() const;

	UFUNCTION(BlueprintCallable, Category = "Riskbound|World")
	bool ReplyToMessage(const FString& MessageId, const FString& ChoiceId, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Riskbound|World")
	void MarkMessageRead(const FString& MessageId);

	// Work colleagues first (executives, then others), personal contacts last.
	UFUNCTION(BlueprintPure, Category = "Riskbound|World")
	TArray<FRbPersonView> GetPeople() const;

	// Clues found so far, in chapter order.
	UFUNCTION(BlueprintPure, Category = "Riskbound|World")
	TArray<FRbClueView> GetClues() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound|World")
	FRbWorldView GetWorldView() const;

	FString PersonName(const std::string& Id) const;

	// ---- Phase 3: skills, backgrounds, conversations (RbGrowth.h, Data/Dialogue.json) ----
	UFUNCTION(BlueprintPure, Category = "Riskbound|Growth")
	TArray<FRbSkillView> GetSkills() const;
	UFUNCTION(BlueprintPure, Category = "Riskbound|Growth")
	int32 GetSkillPoints() const;
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Growth")
	bool LearnSkill(const FString& SkillId, FString& OutError);
	UFUNCTION(BlueprintPure, Category = "Riskbound|Growth")
	TArray<FRbBackgroundView> GetBackgrounds() const;
	UFUNCTION(BlueprintPure, Category = "Riskbound|Growth")
	FString GetBackgroundName() const;
	// The conversation to have now with Person for Trigger ("talk", or "evidence:<mission>/<evidence>").
	UFUNCTION(BlueprintPure, Category = "Riskbound|Growth")
	FRbConversationView StartConversation(const FString& Person, const FString& Trigger) const;
	// Applies a reply; returns the next point (bEnded when the conversation is over).
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Growth")
	FRbConversationView ChooseResponse(const FString& ConversationId, const FString& NodeId, const FString& ResponseId, FString& OutError);

	// A ceremony the HUD plays when the screen is free (promotion, the CRO capstone). Cleared when taken.
	FString TakePendingCeremony() { FString C = PendingCeremony; PendingCeremony.Reset(); return C; }

	// Answers in progress for a mission, kept for the session so a board paper drafted at the
	// workstation is still there at the boardroom lectern. Cleared on submit or set aside.
	const TMap<FString, TArray<FString>>* GetDraft(const FString& MissionId) const { return Drafts.Find(MissionId); }
	void SetDraft(const FString& MissionId, const TMap<FString, TArray<FString>>& Answers) { Drafts.Add(MissionId, Answers); }

private:
	URbGameSubsystem* Game() const;
	FRbMissionDebriefView ToView(const rb::MissionDef& M, const rb::MissionDebrief& D, bool bFull) const;

	void LoadWorld();
	void AnnounceWorldUpdate(const std::vector<std::string>& NewMessages, const std::vector<std::string>& Escalated, bool bBurnout);
	FRbInboxMessageView ToView(const rb::InboxMessage& M) const;

	rb::MissionCatalogue Catalogue;
	rb::WorldContent World;
	TMap<FString, TMap<FString, TArray<FString>>> Drafts;
	rb::DialogueCatalogue Dialogue;
	FString PendingCeremony;
	FRbConversationView NodeView(const rb::ConversationDef& C, const std::string& NodeId) const;
	TArray<FString> LoadErrors;
	std::vector<rb::AchievementDef> Achievements;
	// Full debriefs from this session, keyed by mission, with the record count they belong to.
	TMap<FString, TPair<int32, FRbMissionDebriefView>> SessionDebriefs;
};
