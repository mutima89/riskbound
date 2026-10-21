// Owns the code-built Slate UI and the screen flow. Each flow step calls URbGameSubsystem,
// which is the same path the self-test and automation tests drive.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RbTypes.h"
#include "RbHUD.generated.h"

class SRbCaseFiles;
class SRbHudOverlay;
class SWidget;
class URbGameSubsystem;
class URbSaveSubsystem;
struct FRbAssessmentDraft;

UCLASS()
class RISKBOUND_API ARbHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// ---- Menus ----
	void ShowMainMenu();
	void ShowDifficultyPicker();
	void StartNewGame(ERbDifficulty Difficulty, const FString& Background = TEXT("graduate"));
	void ShowBackgroundPicker(ERbDifficulty Difficulty);
	void ShowChallenges();
	void StartChallenge(int32 Mode); // 1 survival, 2 daily
	void ShowChallengeResults();
	void OpenGlossary(TFunction<void()> OnBack);
	void OpenReview(bool bEnding, TFunction<void()> OnBack);
	// Photo mode: hides the HUD; F12 takes a screenshot (Saved/Screenshots).
	void TogglePhotoMode();
	bool IsPhotoMode() const { return bPhotoMode; }
	void OpenSkills(TFunction<void()> OnBack);
	// A conversation with tone choices (Data/Dialogue.json); OnEnd runs when it is over.
	void PlayConversation(const struct FRbConversationView& View, TFunction<void()> OnEnd);
	// Letterboxed title moment over the boardroom (promotion, CRO); skipped by Reduced motion's camera move.
	void PlayCeremony(const FString& Title, const FString& Subtitle);
	bool IsCeremonyPlaying() const { return CeremonyRoot.IsValid(); }
	ERbLoadOutcome ContinueGame(FString& OutMessage);
	void OpenPauseMenu();
	void OpenSettings(TFunction<void()> OnBack);
	void OpenJournal();
	// Messages from colleagues, incidents, the regulator and home (I key, or the workstation).
	TSharedPtr<class SRbInbox> OpenInbox(const FString& MessageId = FString());
	void OpenCredits(TFunction<void()> OnBack);
	bool SaveNow(FString& OutMessage);
	void QuitToDesktop();

	// ---- Mission flow ----
	void TalkToManager();
	void OpenEvidence(const FString& EvidenceId, TFunction<void()> OnBack = nullptr);
	void OpenWorkstation();
	// Opens the risk tool hub on a tab (index of SRbToolHub::ETab).
	void OpenToolHub(int32 Tab = 0);
	void OpenAssessment();
	// Campaign missions after M001. Optionally opens a mission (or its debrief) directly.
	// Where: the HQ location of the terminal ("" for the player's workstation; "boardroom" for the lectern).
	TSharedPtr<SRbCaseFiles> OpenCaseFiles(const FString& MissionId = FString(), bool bDebrief = false, const FString& Where = FString());
	// In-person casework (ARbCaseMarker): evidence examined or a person interviewed at a location.
	void ExamineInPerson(const FString& MissionId, const FString& EvidenceId, const FString& Where);
	ERbSubmitResult SubmitAssessment(const FRbSubmissionInput& Input);
	void OpenDebrief();

	// ---- Screen stack (one modal screen at a time) ----
	void ShowScreen(TSharedRef<SWidget> Content, TSharedPtr<SWidget> Focus, TFunction<void()> OnBack, bool bPauseGame);
	void CloseScreen();
	bool IsScreenOpen() const { return Screen.IsValid(); }
	void Notice(const FString& Message);
	void ClearNotices();
	void OnSettingsChanged();

	URbGameSubsystem* Game() const;
	URbSaveSubsystem* Saves() const;

	FText MenuMessage;

private:
	UFUNCTION()
	void HandleNotice(const FString& Message);

	void BuildOverlay();
	void SetGameInput();

	TSharedPtr<SWidget> Screen;
	TSharedPtr<SWidget> CeremonyRoot;
	bool bPhotoMode = false;
	bool bEndingPending = false;
	double CeremonyEnds = 0.0;
	void EndCeremony();
	UPROPERTY(Transient)
	TObjectPtr<class ACameraActor> CeremonyCamera;
	TSharedPtr<SWidget> OverlayRoot;
	TSharedPtr<SRbHudOverlay> Overlay;
	TSharedPtr<FRbAssessmentDraft> Draft;
	bool bMainMenuOpen = false;

	// Main menu backdrop: a slow camera drift through the office (static under Reduced Motion).
	void StartMenuCamera();
	void StopMenuCamera();
	UPROPERTY(Transient)
	TObjectPtr<class ACameraActor> MenuCamera;
	double MenuCameraTime = 0.0;
};
