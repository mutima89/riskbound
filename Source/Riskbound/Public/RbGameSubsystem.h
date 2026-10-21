// Owns the rb::GameState for the running session and exposes it to gameplay and UI.
// Every mission action goes through here so UI, self-test and automation tests share one path.
#pragma once

#include "CoreMinimal.h"
#include "RbGameState.h"
#include "RbTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RbGameSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRbOnStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRbOnNotice, const FString&, Message);

UCLASS()
class RISKBOUND_API URbGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static const TCHAR* CheckpointStart;
	static const TCHAR* CheckpointMetManager;
	static const TCHAR* CheckpointSubmitted;
	static const TCHAR* CheckpointDebriefed;

	// ---- Session ----
	UFUNCTION(BlueprintCallable, Category = "Riskbound")
	void NewGame(ERbDifficulty Difficulty, const FString& ProfileName = TEXT("Analyst"), const FString& Background = TEXT("graduate"));

	// Loads the save slot through URbSaveSubsystem.
	UFUNCTION(BlueprintCallable, Category = "Riskbound")
	ERbLoadOutcome Continue(FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	bool HasGame() const { return bHasGame; }

	// ---- Mission 001 actions (the calls the UI makes) ----
	UFUNCTION(BlueprintCallable, Category = "Riskbound")
	ERbManagerTalk TalkToManager();

	UFUNCTION(BlueprintCallable, Category = "Riskbound")
	bool ViewEvidence(const FString& EvidenceId);

	UFUNCTION(BlueprintCallable, Category = "Riskbound")
	ERbSubmitResult ValidateAssessment(const FRbSubmissionInput& Input) const;

	UFUNCTION(BlueprintCallable, Category = "Riskbound")
	ERbSubmitResult SubmitAssessment(const FRbSubmissionInput& Input);

	UFUNCTION(BlueprintCallable, Category = "Riskbound")
	void MarkDebriefReceived();

	// ---- Views ----
	UFUNCTION(BlueprintPure, Category = "Riskbound")
	FString GetCurrentObjectiveText() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	TArray<FRbObjectiveView> GetObjectives() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	TArray<FRbEvidenceView> GetEvidence() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	FRbEvidenceView GetEvidenceItem(const FString& EvidenceId) const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	TArray<FRbOptionView> GetMissingInfoOptions() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	TArray<FRbOptionView> GetTreatmentOptions() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	TArray<FRbOptionView> GetActionOwnerOptions() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	TArray<FRbOptionView> GetRationaleTags() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	TArray<FString> GetControlNames() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	int32 GetMatrixSize() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	FRbDebriefView GetDebrief() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	FRbCareerView GetCareer() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	FRbRegisterEntryView GetRegisterEntry(const FString& RiskId) const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	FString GetCheckpoint() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	bool HasMetManager() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	bool IsSubmitted() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	bool IsDebriefed() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	ERbDifficulty GetDifficulty() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	TArray<FString> GetAchievementTitles() const;

	// Number of achievements in the game (50 with Data/Achievements.json loaded).
	UFUNCTION(BlueprintPure, Category = "Riskbound")
	int32 GetAchievementTotal() const;

	// ---- Settings (persisted in the save, applied to the engine here) ----
	UFUNCTION(BlueprintPure, Category = "Riskbound")
	FRbSettingsView GetSettings() const;

	UFUNCTION(BlueprintCallable, Category = "Riskbound")
	void SetSettings(const FRbSettingsView& NewSettings, bool bApplyWindowMode);

	void ApplySettings(bool bApplyWindowMode) const;

	// ---- C++ access for save and tests ----
	const rb::GameState& GetState() const { return State; }
	// For URbToolsSubsystem: edit, then call StateEdited() so achievements and UI refresh.
	rb::GameState& EditState() { return State; }
	void StateEdited() { AfterAction(false); }
	// As StateEdited, and autosaves (mission submissions).
	void StateCheckpoint() { AfterAction(true); }
	void SetLoadedState(rb::GameState&& Loaded);

	static FString SubmitResultText(ERbSubmitResult Result);

	UPROPERTY(BlueprintAssignable, Category = "Riskbound")
	FRbOnStateChanged OnStateChanged;

	// Player-facing notices: autosaves, achievements, save recovery.
	UPROPERTY(BlueprintAssignable, Category = "Riskbound")
	FRbOnNotice OnNotice;

	void Notify(const FString& Message);

private:
	void AfterAction(bool bAutosave);
	FString AchievementTitle(const std::string& Id) const;

	rb::GameState State;
	bool bHasGame = false;
};
