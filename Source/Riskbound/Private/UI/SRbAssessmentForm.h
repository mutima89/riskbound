// R-014 assessment form: missing info, inherent L/I, control ratings, residual, treatment, owner, rationale.
// Treatment options show their description only; appraisal figures appear in the debrief after Submit().
#pragma once

#include "CoreMinimal.h"
#include "RbTypes.h"
#include "Widgets/SCompoundWidget.h"

class SButton;
class SVerticalBox;
class URbGameSubsystem;

// Work in progress, kept by the HUD so closing the form never loses answers.
struct FRbAssessmentDraft
{
	FRbSubmissionInput Input;
	TArray<int32> ControlChoice; // -1 = not yet rated
};

class SRbAssessmentForm : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRbAssessmentForm) {}
		SLATE_ARGUMENT(TWeakObjectPtr<URbGameSubsystem>, Game)
		SLATE_ARGUMENT(TSharedPtr<FRbAssessmentDraft>, Draft)
		SLATE_ARGUMENT(TFunction<ERbSubmitResult(const FRbSubmissionInput&)>, OnSubmit)
		SLATE_ARGUMENT(TFunction<void()>, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TSharedPtr<SWidget> GetInitialFocus() const { return FirstFocus; }

	// Fills Input.ControlRatings from the draft choices; empty when any control is unrated.
	static FRbSubmissionInput BuildInput(const FRbAssessmentDraft& Draft);

private:
	TSharedRef<SWidget> Section(const FText& Title, const FText& Help, TSharedRef<SWidget> Body) const;
	TSharedRef<SWidget> MultiChoice(const TArray<FRbOptionView>& Options, TArray<FString>* Selected) const;
	TSharedRef<SWidget> SingleChoice(const TArray<FRbOptionView>& Options, FString* Selected) const;
	TSharedRef<SWidget> LevelPicker(const FText& Label, int32* Value, int32 MaxLevel);
	TSharedRef<SWidget> ControlRatings(const TArray<FString>& Controls);
	FReply HandleSubmit();
	int32 CompletedSections() const;
	FText MissingReason() const;
	static constexpr int32 SectionCount = 7;

	TWeakObjectPtr<URbGameSubsystem> Game;
	TSharedPtr<FRbAssessmentDraft> Draft;
	TFunction<ERbSubmitResult(const FRbSubmissionInput&)> OnSubmit;
	TFunction<void()> OnClose;
	TSharedPtr<SWidget> FirstFocus;
	FText StatusText;
	mutable bool bStatusIsError = false;
};
