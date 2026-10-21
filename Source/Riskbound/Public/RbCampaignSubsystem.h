// Campaign missions after Mission 001 (Gate 3): availability, play, debrief and promotion.
// Thin calls into rb::Campaign (RiskboundSim); the mission catalogue is content, set once per session.
#pragma once

#include "CoreMinimal.h"
#include "RbCampaign.h"
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

	UFUNCTION(BlueprintPure, Category = "Riskbound|Campaign")
	TArray<FRbMissionSummary> GetMissions() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound|Campaign")
	FRbMissionView GetMission(const FString& MissionId) const;

	UFUNCTION(BlueprintPure, Category = "Riskbound|Campaign")
	FString GetActiveMissionId() const;

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
	// Full debriefs from this session, keyed by mission, with the record count they belong to.
	TMap<FString, TPair<int32, FRbMissionDebriefView>> SessionDebriefs;
};
