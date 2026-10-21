// Workstation case files (Gate 3/4): campaign missions after Mission 001. Lists missions with why
// they are open or closed, runs one (briefing, evidence, questions) and shows its debrief.
// Every rule is in URbCampaignSubsystem / RiskboundSim; this widget only collects answers.
#pragma once

#include "CoreMinimal.h"
#include "RbCampaignTypes.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SWidget;
class URbCampaignSubsystem;
class URbGameSubsystem;

class SRbCaseFiles : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRbCaseFiles) {}
		SLATE_ARGUMENT(TWeakObjectPtr<URbCampaignSubsystem>, Campaign)
		SLATE_ARGUMENT(TWeakObjectPtr<URbGameSubsystem>, Game)
		SLATE_ARGUMENT(TFunction<void()>, OnClose)
		SLATE_ARGUMENT(TFunction<void()>, OnMissionStarted)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	TSharedPtr<SWidget> GetInitialFocus() const { return FirstFocus; }

	void ShowList();
	void ShowMission(const FString& MissionId);
	void ShowDebrief(const FString& MissionId);

	// The answers as currently selected (also used by the self-test).
	TArray<FRbMissionAnswer> CurrentAnswers() const;
	void SetAnswer(const FString& QuestionId, const FString& OptionId, bool bMulti, bool bSelected);

private:
	enum class EView : uint8 { List, Mission, Debrief };
	void Rebuild();
	TSharedRef<SWidget> BuildList();
	TSharedRef<SWidget> BuildMission();
	TSharedRef<SWidget> BuildDebrief();

	TWeakObjectPtr<URbCampaignSubsystem> Campaign;
	TWeakObjectPtr<URbGameSubsystem> Game;
	TFunction<void()> OnClose;
	TFunction<void()> OnMissionStarted;
	TSharedPtr<SBox> Body;
	TSharedPtr<SWidget> FirstFocus;
	EView View = EView::List;
	FString MissionId;
	FString OpenEvidenceId;
	TMap<FString, TArray<FString>> Answers;
	FText Status;
};
