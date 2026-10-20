// Owns the code-built Slate UI and the screen flow. Each flow step calls URbGameSubsystem,
// which is the same path the self-test and automation tests drive.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RbTypes.h"
#include "RbHUD.generated.h"

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
	void StartNewGame(ERbDifficulty Difficulty);
	ERbLoadOutcome ContinueGame(FString& OutMessage);
	void OpenPauseMenu();
	void OpenSettings(TFunction<void()> OnBack);
	void OpenJournal();
	bool SaveNow(FString& OutMessage);
	void QuitToDesktop();

	// ---- Mission flow ----
	void TalkToManager();
	void OpenEvidence(const FString& EvidenceId, TFunction<void()> OnBack = nullptr);
	void OpenWorkstation();
	void OpenAssessment();
	ERbSubmitResult SubmitAssessment(const FRbSubmissionInput& Input);
	void OpenDebrief();

	// ---- Screen stack (one modal screen at a time) ----
	void ShowScreen(TSharedRef<SWidget> Content, TSharedPtr<SWidget> Focus, TFunction<void()> OnBack, bool bPauseGame);
	void CloseScreen();
	bool IsScreenOpen() const { return Screen.IsValid(); }
	void Notice(const FString& Message);
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
