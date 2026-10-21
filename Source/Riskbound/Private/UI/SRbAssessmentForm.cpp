#include "SRbAssessmentForm.h"

#include "RbGameSubsystem.h"
#include "RbUIStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Riskbound"

namespace
{
TSharedRef<SWidget> ChoiceLabel(const FRbOptionView& Option)
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[RbUI::MakeBody(FText::FromString(Option.Label), 14)];
	if (!Option.Description.IsEmpty())
	{
		Box->AddSlot().AutoHeight()[RbUI::MakeBody(FText::FromString(Option.Description), 12, RbUI::TextMuted)];
	}
	return Box;
}
} // namespace

void SRbAssessmentForm::Construct(const FArguments& InArgs)
{
	Game = InArgs._Game;
	Draft = InArgs._Draft;
	OnSubmit = InArgs._OnSubmit;
	OnClose = InArgs._OnClose;
	check(Game.IsValid() && Draft.IsValid());

	const URbGameSubsystem* G = Game.Get();
	const TArray<FString> Controls = G->GetControlNames();
	if (Draft->ControlChoice.Num() != Controls.Num())
	{
		Draft->ControlChoice.Init(-1, Controls.Num());
	}
	const int32 MatrixSize = G->GetMatrixSize();
	FRbSubmissionInput& In = Draft->Input;

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	auto Add = [&Body](TSharedRef<SWidget> W) { Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)[W]; };

	Add(Section(LOCTEXT("MissingTitle", "1. Missing information"),
		LOCTEXT("MissingHelp", "Which facts do you still need before this risk can be rated with confidence? Select all that apply."),
		MultiChoice(G->GetMissingInfoOptions(), &In.MissingInfo)));

	Add(Section(LOCTEXT("InherentTitle", "2. Inherent risk (before controls)"),
		LOCTEXT("InherentHelp", "Rate likelihood and impact on the 5-point scale, ignoring the existing controls."),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[LevelPicker(LOCTEXT("InherentL", "Likelihood"), &In.InherentLikelihood, MatrixSize)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[LevelPicker(LOCTEXT("InherentI", "Impact"), &In.InherentImpact, MatrixSize)]));

	Add(Section(LOCTEXT("ControlsTitle", "3. Control effectiveness"),
		LOCTEXT("ControlsHelp", "Rate each existing control on the evidence you have seen, not on how it is described."),
		ControlRatings(Controls)));

	Add(Section(LOCTEXT("ResidualTitle", "4. Residual risk (after controls)"),
		LOCTEXT("ResidualHelp", "Rate likelihood and impact taking the controls as they actually operate."),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[LevelPicker(LOCTEXT("ResidualL", "Likelihood"), &In.ResidualLikelihood, MatrixSize)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[LevelPicker(LOCTEXT("ResidualI", "Impact"), &In.ResidualImpact, MatrixSize)]));

	Add(Section(LOCTEXT("TreatmentTitle", "5. Recommended treatment"),
		LOCTEXT("TreatmentHelp", "Choose one. The cost-benefit appraisal is shown in your debrief."),
		SingleChoice(G->GetTreatmentOptions(), &In.TreatmentId)));

	Add(Section(LOCTEXT("OwnerTitle", "6. Action owner"),
		LOCTEXT("OwnerHelp", "Who should own and deliver the action?"),
		SingleChoice(G->GetActionOwnerOptions(), &In.ActionOwnerId)));

	Add(Section(LOCTEXT("RationaleTitle", "7. Rationale"),
		LOCTEXT("RationaleHelp", "Tag the reasons that support your recommendation, then explain in your own words."),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[MultiChoice(G->GetRationaleTags(), &In.RationaleTagIds)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.MinDesiredHeight(70.0f)
			[
				SNew(SMultiLineEditableTextBox)
				.Font(RbUI::Font(14))
				.HintText(LOCTEXT("RationaleHint", "Write your rationale for Priya..."))
				.Text(FText::FromString(In.RationaleText))
				.OnTextChanged_Lambda([this](const FText& T) { Draft->Input.RationaleText = T.ToString(); })
				.AutoWrapText(true)
			]
		]));

	TSharedPtr<SButton> SubmitButton;
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			// Progress sits on the eyebrow line, so it never collides with a long title.
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[RbUI::MakeEyebrow(LOCTEXT("FormEyebrow", "Mission 001  /  Assessment"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(RbUI::FontW(15, RbUI::EWeight::SemiBold))
				.Text_Lambda([this]() { return FText::Format(LOCTEXT("Progress", "{0} / {1} sections complete"), CompletedSections(), SectionCount); })
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(CompletedSections() == SectionCount ? RbUI::Good : RbUI::TextMuted); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight()[RbUI::MakeHeading(LOCTEXT("FormTitle", "Risk assessment: R-014 cold-store temperature excursion"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 12.0f)
		[
			RbUI::MakeBody(LOCTEXT("FormIntro", "Your answers are kept if you close the form. Submitting is final. Tab and the arrow keys move between fields; Space selects."), 13, RbUI::TextMuted)
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()[Body]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return StatusText.IsEmpty() ? MissingReason() : StatusText; })
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(!StatusText.IsEmpty() ? (bStatusIsError ? RbUI::Bad : RbUI::Good)
					: (CompletedSections() == SectionCount ? RbUI::Good : RbUI::TextMuted)); })
				.Font(RbUI::Font(14, true))
				.AutoWrapText(true)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)
			[
				RbUI::MakeButton(LOCTEXT("Close", "Close (keep draft)"), FOnClicked::CreateLambda([this]() { OnClose(); return FReply::Handled(); }))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				RbUI::MakeButton(LOCTEXT("Submit", "Submit assessment"), FOnClicked::CreateSP(this, &SRbAssessmentForm::HandleSubmit), &SubmitButton,
					TAttribute<bool>::CreateLambda([this]() { return CompletedSections() == SectionCount; }), true)
			]
		]
	];
	if (!FirstFocus.IsValid())
	{
		FirstFocus = SubmitButton;
	}
}

TSharedRef<SWidget> SRbAssessmentForm::Section(const FText& Title, const FText& Help, TSharedRef<SWidget> Content) const
{
	return SNew(SBorder)
		.BorderImage(RbUI::CardBrush())
		.Padding(14.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[RbUI::MakeHeading(Title, 16)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 8.0f)[RbUI::MakeBody(Help, 12, RbUI::TextMuted)]
			+ SVerticalBox::Slot().AutoHeight()[Content]
		];
}

TSharedRef<SWidget> SRbAssessmentForm::MultiChoice(const TArray<FRbOptionView>& Options, TArray<FString>* Selected) const
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (const FRbOptionView& Option : Options)
	{
		const FString Id = Option.Id;
		Box->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			SNew(SCheckBox)
			.Style(&RbUI::CheckStyle())
			.IsChecked_Lambda([Selected, Id]() { return Selected->Contains(Id) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Selected, Id](ECheckBoxState NewState)
			{
				if (NewState == ECheckBoxState::Checked) Selected->AddUnique(Id);
				else Selected->Remove(Id);
			})
			[
				ChoiceLabel(Option)
			]
		];
	}
	return Box;
}

TSharedRef<SWidget> SRbAssessmentForm::SingleChoice(const TArray<FRbOptionView>& Options, FString* Selected) const
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (const FRbOptionView& Option : Options)
	{
		const FString Id = Option.Id;
		Box->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			SNew(SCheckBox)
			.Style(&RbUI::RadioStyle())
			.IsChecked_Lambda([Selected, Id]() { return *Selected == Id ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Selected, Id](ECheckBoxState) { *Selected = Id; })
			[
				ChoiceLabel(Option)
			]
		];
	}
	return Box;
}

TSharedRef<SWidget> SRbAssessmentForm::LevelPicker(const FText& Label, int32* Value, int32 MaxLevel)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(120.0f)[RbUI::MakeBody(Label, 14)]
		];
	for (int32 Level = 1; Level <= MaxLevel; ++Level)
	{
		TSharedRef<SCheckBox> Choice = SNew(SCheckBox)
			.Style(&RbUI::RadioStyle())
			.IsChecked_Lambda([Value, Level]() { return *Value == Level ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Value, Level](ECheckBoxState) { *Value = Level; })
			[
				RbUI::MakeBody(FText::AsNumber(Level), 14)
			];
		if (!FirstFocus.IsValid())
		{
			FirstFocus = Choice;
		}
		Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 16.0f, 0.0f)[Choice];
	}
	Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
	[
		RbUI::MakeBody(LOCTEXT("ScaleHint", "(1 = rare / minor, 5 = almost certain / severe)"), 12, RbUI::TextMuted)
	];
	return Row;
}

TSharedRef<SWidget> SRbAssessmentForm::ControlRatings(const TArray<FString>& Controls)
{
	static const FText RatingNames[] = {
		LOCTEXT("Ineffective", "Ineffective"),
		LOCTEXT("Partial", "Partially effective"),
		LOCTEXT("Effective", "Effective")};

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (int32 Index = 0; Index < Controls.Num(); ++Index)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(300.0f)[RbUI::MakeBody(FText::FromString(Controls[Index]), 14)]
			];
		for (int32 Rating = 0; Rating < 3; ++Rating)
		{
			TSharedPtr<FRbAssessmentDraft> D = Draft;
			Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 16.0f, 0.0f)
			[
				SNew(SCheckBox)
				.Style(&RbUI::RadioStyle())
				.IsChecked_Lambda([D, Index, Rating]() { return D->ControlChoice[Index] == Rating ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([D, Index, Rating](ECheckBoxState) { D->ControlChoice[Index] = Rating; })
				[
					RbUI::MakeBody(RatingNames[Rating], 13)
				]
			];
		}
		Box->AddSlot().AutoHeight().Padding(0.0f, 3.0f)[Row];
	}
	return Box;
}

FRbSubmissionInput SRbAssessmentForm::BuildInput(const FRbAssessmentDraft& InDraft)
{
	FRbSubmissionInput Out = InDraft.Input;
	Out.ControlRatings.Reset();
	for (int32 Choice : InDraft.ControlChoice)
	{
		if (Choice < 0)
		{
			Out.ControlRatings.Reset(); // incomplete: the sim reports IncompleteSubmission
			break;
		}
		Out.ControlRatings.Add(static_cast<ERbControlRating>(Choice));
	}
	return Out;
}

int32 SRbAssessmentForm::CompletedSections() const
{
	// Completeness only (every section answered); correctness stays hidden until the debrief.
	const FRbSubmissionInput& In = Draft->Input;
	int32 Done = 0;
	Done += In.MissingInfo.Num() > 0;
	Done += In.InherentLikelihood > 0 && In.InherentImpact > 0;
	Done += !Draft->ControlChoice.Contains(-1);
	Done += In.ResidualLikelihood > 0 && In.ResidualImpact > 0;
	Done += !In.TreatmentId.IsEmpty();
	Done += !In.ActionOwnerId.IsEmpty();
	Done += In.RationaleTagIds.Num() > 0 || !In.RationaleText.TrimStartAndEnd().IsEmpty();
	return Done;
}

FText SRbAssessmentForm::MissingReason() const
{
	const FRbSubmissionInput& In = Draft->Input;
	TArray<FString> Missing;
	if (In.MissingInfo.Num() == 0) Missing.Add(TEXT("1"));
	if (In.InherentLikelihood == 0 || In.InherentImpact == 0) Missing.Add(TEXT("2"));
	if (Draft->ControlChoice.Contains(-1)) Missing.Add(TEXT("3"));
	if (In.ResidualLikelihood == 0 || In.ResidualImpact == 0) Missing.Add(TEXT("4"));
	if (In.TreatmentId.IsEmpty()) Missing.Add(TEXT("5"));
	if (In.ActionOwnerId.IsEmpty()) Missing.Add(TEXT("6"));
	if (In.RationaleTagIds.Num() == 0 && In.RationaleText.TrimStartAndEnd().IsEmpty()) Missing.Add(TEXT("7"));
	bStatusIsError = false;
	return Missing.IsEmpty() ? LOCTEXT("ReadyToSubmit", "All sections answered. Submitting is final.")
		: FText::Format(LOCTEXT("StillMissing", "To submit, answer section {0}."), FText::FromString(FString::Join(Missing, TEXT(", "))));
}

FReply SRbAssessmentForm::HandleSubmit()
{
	const ERbSubmitResult Result = OnSubmit(BuildInput(*Draft));
	bStatusIsError = Result != ERbSubmitResult::Ok;
	StatusText = FText::FromString(URbGameSubsystem::SubmitResultText(Result));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
