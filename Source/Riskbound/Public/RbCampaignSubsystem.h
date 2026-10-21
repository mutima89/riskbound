// Campaign missions after Mission 001 (Gate 3): availability, play, debrief and promotion.
// Thin calls into rb::Campaign (RiskboundSim); the mission catalogue is content, set once per session.
#pragma once

#include "CoreMinimal.h"
#include "RbCampaign.h"
#include "RbGameState.h"
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

	UFUNCTION(BlueprintCallable, Category = "Riskbound|Campaign")
	ERbMissionResult ViewEvidence(const FString& MissionId, const FString& EvidenceId);

	UFUNCTION(BlueprintCallable, Category = "Riskbound|Campaign")
	ERbMissionResult AbandonMission();

	UFUNCTION(BlueprintCallable, Category = "Riskbound|Campaign")
	ERbMissionResult ValidateAnswers(const FString& MissionId, const TArray<FRbMissionAnswer>& Answers) const;

	// Scores, applies consequences, advances time and runs the promotion review; autosaves.
	UFUNCTION(BlueprintCallable, Category = "Riskbound|Campaign")
	ERbMissionResult SubmitMission(const FString& MissionId, const TArray<FRbMissionAnswer>& Answers);

	// The debrief of the latest attempt at a mission. Consequences and promotion are only
	// present for a submission made in this session; the scores are recomputed from the save.
	UFUNCTION(BlueprintPure, Category = "Riskbound|Campaign")
	FRbMissionDebriefView GetDebrief(const FString& MissionId) const;

	static FString ResultText(ERbMissionResult Result);

private:
	URbGameSubsystem* Game() const;
	FRbMissionDebriefView ToView(const rb::MissionDef& M, const rb::MissionDebrief& D, bool bFull) const;

	rb::MissionCatalogue Catalogue;
	TArray<FString> LoadErrors;
	std::vector<rb::AchievementDef> Achievements;
	// Full debriefs from this session, keyed by mission, with the record count they belong to.
	TMap<FString, TPair<int32, FRbMissionDebriefView>> SessionDebriefs;
};
