#include "RbScreens.h"

#include "RbGameSubsystem.h"
#include "RbHUD.h"
#include "RbSaveSubsystem.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Riskbound"

using namespace RbUI;

namespace
{
TSharedRef<SVerticalBox> Column() { return SNew(SVerticalBox); }

void AddRow(const TSharedRef<SVerticalBox>& Box, TSharedRef<SWidget> W, float Bottom = 10.0f)
{
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Bottom)[W];
}

void AddBullets(const TSharedRef<SVerticalBox>& Box, const FText& Heading, const TArray<FString>& Items)
{
	if (Items.IsEmpty()) return;
	AddRow(Box, MakeHeading(Heading, 16), 4.0f);
	for (const FString& Item : Items)
	{
		AddRow(Box, MakeBody(FText::FromString(TEXT("-  ") + Item), 14), 3.0f);
	}
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[SNew(SBox)];
}

FOnClicked Click(TFunction<void()> Fn)
{
	return FOnClicked::CreateLambda([Fn]() { Fn(); return FReply::Handled(); });
}

TSharedRef<SWidget> Scrolling(TSharedRef<SWidget> Content)
{
	return SNew(SScrollBox) + SScrollBox::Slot()[Content];
}

FText Percent(double Ratio)
{
	FNumberFormattingOptions O;
	O.MaximumFractionalDigits = 0;
	return FText::AsPercent(Ratio, &O);
}

FText Ratio2(double Value)
{
	FNumberFormattingOptions O;
	O.MinimumFractionalDigits = 2;
	O.MaximumFractionalDigits = 2;
	return FText::AsNumber(Value, &O);
}

TSharedRef<SWidget> TableRow(const FText& A, const FText& B, const FText& C, bool bHeader = false)
{
	const int32 Size = 14;
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.2f)[SNew(STextBlock).Text(A).Font(Font(Size, bHeader)).ColorAndOpacity(bHeader ? TextMuted : TextPrimary)]
		+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Text(B).Font(Font(Size, bHeader)).ColorAndOpacity(bHeader ? TextMuted : TextPrimary).AutoWrapText(true)]
		+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Text(C).Font(Font(Size, bHeader)).ColorAndOpacity(bHeader ? TextMuted : TextPrimary).AutoWrapText(true)];
}

const FSliderStyle& SliderStyle()
{
	static FSliderStyle Style = []
	{
		FSliderStyle S = FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider");
		const FSlateColorBrush Bar(FLinearColor(1.0f, 1.0f, 1.0f, 0.22f));
		const FSlateRoundedBoxBrush Handle(Accent, 7.0f, FVector2f(14.0f, 14.0f));
		S.SetNormalBarImage(Bar).SetHoveredBarImage(Bar).SetDisabledBarImage(Bar);
		S.SetNormalThumbImage(Handle).SetHoveredThumbImage(Handle).SetDisabledThumbImage(Handle);
		S.SetBarThickness(3.0f);
		return S;
	}();
	return Style;
}

TSharedRef<SWidget> SettingSlider(const FText& Label, float Min, float Max, float Step, TFunction<float()> Get, TFunction<void(float)> Set)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(220.0f)[MakeBody(Label, 15)]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(SSlider)
			.MinValue(Min)
			.MaxValue(Max)
			.StepSize(Step)
			.IsFocusable(true)
			.Style(&SliderStyle())
			.Value_Lambda([Get]() { return Get(); })
			.OnValueChanged_Lambda([Set](float V) { Set(V); })
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox).WidthOverride(60.0f)
			[
				SNew(STextBlock).Font(Font(15)).ColorAndOpacity(TextPrimary).Text_Lambda([Get]() { return Ratio2(Get()); })
			]
		];
}

TSharedRef<SWidget> SettingToggle(const FText& Label, TFunction<bool()> Get, TFunction<void(bool)> Set)
{
	return SNew(SCheckBox)
		.IsChecked_Lambda([Get]() { return Get() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([Set](ECheckBoxState S) { Set(S == ECheckBoxState::Checked); })
		[
			MakeBody(Label, 15)
		];
}
} // namespace

namespace RbScreens
{
FRbScreenParts MainMenu(ARbHUD* Hud)
{
	TSharedPtr<SButton> First;
	const bool bHasSave = Hud->Saves()->HasSave();
	TSharedRef<SVerticalBox> Box = Column();
	AddRow(Box, MakeEyebrow(LOCTEXT("Studio", "Valtren Coldline Logistics  /  Risk function")), Space2);
	AddRow(Box, SNew(STextBlock).Text(LOCTEXT("GameTitle", "RISKBOUND")).Font(FontW(52, EWeight::Bold)).ColorAndOpacity(TextPrimary), 2.0f);
	AddRow(Box, MakeBody(LOCTEXT("Tagline", "From graduate trainee to Chief Risk Officer, one real decision at a time."), 16, TextMuted), 24.0f);
	AddRow(Box, MakeButton(LOCTEXT("NewGame", "New game"), Click([Hud]() { Hud->ShowDifficultyPicker(); }), &First, true, true), 8.0f);
	AddRow(Box, MakeButton(LOCTEXT("Continue", "Continue"), Click([Hud]()
	{
		FString Message;
		const ERbLoadOutcome Outcome = Hud->ContinueGame(Message);
		if (Outcome != ERbLoadOutcome::Ok && Outcome != ERbLoadOutcome::RecoveredFromBackup)
		{
			Hud->ShowMainMenu(); // shows MenuMessage
		}
	}), nullptr, bHasSave), 8.0f);
	AddRow(Box, MakeButton(LOCTEXT("Settings", "Settings"), Click([Hud]() { Hud->OpenSettings([Hud]() { Hud->ShowMainMenu(); }); })), 8.0f);
	AddRow(Box, MakeButton(LOCTEXT("Quit", "Quit"), Click([Hud]() { Hud->QuitToDesktop(); })), 16.0f);
	if (!Hud->MenuMessage.IsEmpty())
	{
		AddRow(Box, MakeBody(Hud->MenuMessage, 14, Bad));
	}
	AddRow(Box, MakeBody(bHasSave ? LOCTEXT("SaveFound", "Saved game found.") : LOCTEXT("NoSave", "No saved game yet."), 12, TextMuted), 0.0f);
	return {MakeModalFrame(Box, 620.0f), First};
}

FRbScreenParts DifficultyPicker(ARbHUD* Hud)
{
	struct FChoice
	{
		ERbDifficulty Difficulty;
		FText Label;
		FText Help;
	};
	const FChoice Choices[] = {
		{ERbDifficulty::Analyst, LOCTEXT("Analyst", "Analyst"), LOCTEXT("AnalystHelp", "Hints on. Operational incidents are less likely.")},
		{ERbDifficulty::Professional, LOCTEXT("Professional", "Professional"), LOCTEXT("ProfessionalHelp", "The baseline experience. Hints on, adjustable in settings.")},
		{ERbDifficulty::Executive, LOCTEXT("Executive", "Executive"), LOCTEXT("ExecutiveHelp", "No hints by default. Incidents are more likely.")},
	};
	TSharedPtr<SButton> First;
	TSharedRef<SVerticalBox> Box = Column();
	AddRow(Box, MakeHeading(LOCTEXT("ChooseDifficulty", "Choose difficulty")), 4.0f);
	AddRow(Box, MakeBody(LOCTEXT("DifficultyNote", "Difficulty changes how often things go wrong, never how your decisions are judged."), 14, TextMuted), 16.0f);
	for (const FChoice& C : Choices)
	{
		const ERbDifficulty D = C.Difficulty;
		TSharedPtr<SButton>* Out = First.IsValid() ? nullptr : &First;
		AddRow(Box, MakeButton(C.Label, Click([Hud, D]() { Hud->StartNewGame(D); }), Out), 2.0f);
		AddRow(Box, MakeBody(C.Help, 13, TextMuted), 12.0f);
	}
	AddRow(Box, MakeButton(LOCTEXT("Back", "Back"), Click([Hud]() { Hud->ShowMainMenu(); })), 0.0f);
	return {MakeModalFrame(Box, 620.0f), First};
}

FRbScreenParts PauseMenu(ARbHUD* Hud)
{
	TSharedPtr<SButton> First;
	TSharedRef<SVerticalBox> Box = Column();
	AddRow(Box, MakeHeading(LOCTEXT("Paused", "Paused")), 16.0f);
	AddRow(Box, MakeButton(LOCTEXT("Resume", "Resume"), Click([Hud]() { Hud->CloseScreen(); }), &First, true, true), 8.0f);
	AddRow(Box, MakeButton(LOCTEXT("Save", "Save game"), Click([Hud]() { FString M; Hud->SaveNow(M); })), 8.0f);
	AddRow(Box, MakeButton(LOCTEXT("Journal", "Journal"), Click([Hud]() { Hud->CloseScreen(); Hud->OpenJournal(); })), 8.0f);
	AddRow(Box, MakeButton(LOCTEXT("PauseSettings", "Settings"), Click([Hud]() { Hud->OpenSettings([Hud]() { Hud->CloseScreen(); Hud->OpenPauseMenu(); }); })), 8.0f);
	AddRow(Box, MakeButton(LOCTEXT("QuitDesktop", "Quit to desktop"), Click([Hud]() { Hud->QuitToDesktop(); })), 12.0f);
	AddRow(Box, MakeBody(LOCTEXT("PauseNote", "Progress autosaves after meeting your manager and after submitting. Unsaved investigation steps are lost on quit."), 12, TextMuted), 0.0f);
	return {MakeModalFrame(Box, 520.0f), First};
}

FRbScreenParts Settings(ARbHUD* Hud, TFunction<void()> OnBack)
{
	TWeakObjectPtr<URbGameSubsystem> Game(Hud->Game());
	auto Get = [Game]() { return Game.IsValid() ? Game->GetSettings() : FRbSettingsView(); };
	auto Update = [Game](TFunction<void(FRbSettingsView&)> Change, bool bWindowMode)
	{
		if (!Game.IsValid()) return;
		FRbSettingsView S = Game->GetSettings();
		Change(S);
		Game->SetSettings(S, bWindowMode);
	};

	TSharedPtr<SButton> Back;
	TSharedRef<SVerticalBox> Box = Column();
	AddRow(Box, MakeHeading(LOCTEXT("SettingsTitle", "Settings")), 16.0f);
	AddRow(Box, SettingSlider(LOCTEXT("Sensitivity", "Mouse sensitivity"), 0.1f, 5.0f, 0.1f,
		[Get]() { return Get().MouseSensitivity; }, [Update](float V) { Update([V](FRbSettingsView& S) { S.MouseSensitivity = V; }, false); }));
	AddRow(Box, SettingSlider(LOCTEXT("TextSize", "Text size"), 0.8f, 1.6f, 0.1f,
		[Get]() { return Get().TextScale; }, [Update](float V) { Update([V](FRbSettingsView& S) { S.TextScale = V; }, false); }));
	AddRow(Box, SettingSlider(LOCTEXT("Volume", "Master volume"), 0.0f, 1.0f, 0.05f,
		[Get]() { return Get().MasterVolume; }, [Update](float V) { Update([V](FRbSettingsView& S) { S.MasterVolume = V; }, false); }));
	{
		// Graphics quality: engine scalability presets. Low switches off Lumen for slower GPUs.
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(220.0f)[MakeBody(LOCTEXT("Quality", "Graphics quality"), 15)]];
		const FText Names[] = {LOCTEXT("QLow", "Low"), LOCTEXT("QMedium", "Medium"), LOCTEXT("QHigh", "High"), LOCTEXT("QEpic", "Epic")};
		for (int32 Q = 0; Q < 4; ++Q)
		{
			Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Space4, 0.0f)
			[
				SNew(SCheckBox)
				.Style(&FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("RadioButton"))
				.IsChecked_Lambda([Get, Q]() { return Get().GraphicsQuality == Q ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([Update, Q](ECheckBoxState) { Update([Q](FRbSettingsView& S) { S.GraphicsQuality = Q; }, false); })
				[
					MakeBody(Names[Q], 14)
				]
			];
		}
		AddRow(Box, Row);
	}
	AddRow(Box, SettingToggle(LOCTEXT("ReducedMotion", "Reduced motion (no motion blur)"),
		[Get]() { return Get().bReducedMotion; }, [Update](bool B) { Update([B](FRbSettingsView& S) { S.bReducedMotion = B; }, false); }));
	AddRow(Box, SettingToggle(LOCTEXT("Hints", "Hints (evidence locations in briefing and journal)"),
		[Get]() { return Get().bHints; }, [Update](bool B) { Update([B](FRbSettingsView& S) { S.bHints = B; }, false); }));
	AddRow(Box, SettingToggle(LOCTEXT("Fullscreen", "Fullscreen (off = windowed)"),
		[Get]() { return Get().bFullscreen; }, [Update](bool B) { Update([B](FRbSettingsView& S) { S.bFullscreen = B; }, true); }));
	AddRow(Box, MakeBody(LOCTEXT("SettingsNote", "Settings are stored with your save. Text size applies to screens opened after this one. Left/Right adjust a focused slider."), 12, TextMuted), 12.0f);
	AddRow(Box, MakeButton(LOCTEXT("SettingsBack", "Back"), Click(OnBack), &Back), 0.0f);
	return {MakeModalFrame(Box, 760.0f), Back};
}

FRbScreenParts Journal(ARbHUD* Hud)
{
	const URbGameSubsystem* Game = Hud->Game();
	const bool bHints = Game->GetSettings().bHints;
	TSharedPtr<SButton> Close;
	TSharedRef<SVerticalBox> Box = Column();
	const FRbCareerView Career = Game->GetCareer();
	AddRow(Box, MakeHeading(LOCTEXT("JournalTitle", "Journal")), 2.0f);
	AddRow(Box, MakeBody(FText::Format(LOCTEXT("JournalCareer", "Level {0}: {1}"), Career.Level, FText::FromString(Career.Title)), 14, TextMuted), 12.0f);
	AddRow(Box, MakeHeading(LOCTEXT("Objectives", "Mission 001: The Unresolved Warning"), 16), 4.0f);
	for (const FRbObjectiveView& O : Game->GetObjectives())
	{
		AddRow(Box, MakeBody(FText::FromString((O.bComplete ? TEXT("[x]  ") : TEXT("[  ]  ")) + O.Text), 14, O.bComplete ? TextMuted : TextPrimary), 2.0f);
	}
	AddRow(Box, SNew(SBox).HeightOverride(10.0f));
	AddRow(Box, MakeHeading(LOCTEXT("Documents", "Documents"), 16), 4.0f);
	for (const FRbEvidenceView& E : Game->GetEvidence())
	{
		FString Line = (E.bViewed ? TEXT("[read]  ") : TEXT("[unread]  ")) + E.Title;
		if (E.bViewed || bHints) Line += TEXT("  -  ") + E.Location;
		AddRow(Box, MakeBody(FText::FromString(Line), 14, E.bViewed ? TextMuted : TextPrimary), 2.0f);
	}
	TSharedRef<SVerticalBox> Root = Column();
	Root->AddSlot().FillHeight(1.0f)[Scrolling(Box)];
	Root->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f).HAlign(HAlign_Right)
	[
		MakeButton(LOCTEXT("CloseJournal", "Close"), Click([Hud]() { Hud->CloseScreen(); }), &Close)
	];
	return {MakeModalFrame(Root, 820.0f), Close};
}

FRbScreenParts Workstation(ARbHUD* Hud)
{
	const URbGameSubsystem* Game = Hud->Game();
	const bool bSubmitted = Game->IsSubmitted();
	auto Status = [Game](const TCHAR* Id) { return Game->GetEvidenceItem(Id).bViewed ? FString(TEXT("  (read)")) : FString(TEXT("  (new)")); };
	auto BackHere = [Hud]() { Hud->OpenWorkstation(); };

	TSharedPtr<SButton> First;
	TSharedRef<SVerticalBox> Box = Column();
	AddRow(Box, MakeHeading(LOCTEXT("WorkstationTitle", "Workstation: junior risk analyst")), 4.0f);
	AddRow(Box, MakeBody(LOCTEXT("WorkstationHelp", "Inbox and the operational risk register. The assessment form for R-014 is filed from here."), 14, TextMuted), 16.0f);
	AddRow(Box, MakeButton(FText::FromString(TEXT("Inbox: Incident report IR-2291") + Status(TEXT("incident_report"))),
		Click([Hud, BackHere]() { Hud->OpenEvidence(TEXT("incident_report"), BackHere); }), &First), 8.0f);
	AddRow(Box, MakeButton(FText::FromString(TEXT("Risk register: R-014 cold-store temperature excursion") + Status(TEXT("risk_register_R014"))),
		Click([Hud, BackHere]() { Hud->OpenEvidence(TEXT("risk_register_R014"), BackHere); })), 8.0f);
	AddRow(Box, MakeButton(bSubmitted ? LOCTEXT("FormDone", "Assessment form (submitted)") : LOCTEXT("FormOpen", "Assessment form: R-014"),
		Click([Hud]() { Hud->OpenAssessment(); }), nullptr, !bSubmitted), 8.0f);
	AddRow(Box, MakeButton(LOCTEXT("LogOff", "Log off"), Click([Hud]() { Hud->CloseScreen(); })), 0.0f);
	return {MakeModalFrame(Box, 720.0f), First};
}

FRbScreenParts Document(const FText& Title, const FText& Subtitle, const FText& Body, TFunction<void()> OnClose)
{
	TSharedPtr<SButton> Close;
	TSharedRef<SVerticalBox> Box = Column();
	AddRow(Box, MakeHeading(Title), 2.0f);
	AddRow(Box, MakeBody(Subtitle, 13, TextMuted), 14.0f);
	TSharedRef<SVerticalBox> Root = Column();
	Root->AddSlot().AutoHeight()[Box];
	Root->AddSlot().FillHeight(1.0f)
	[
		SNew(SBorder).BorderImage(CardBrush()).Padding(16.0f)[Scrolling(MakeBody(Body, 16))]
	];
	Root->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f).HAlign(HAlign_Right)
	[
		MakeButton(LOCTEXT("CloseDoc", "Close"), Click(OnClose), &Close)
	];
	return {MakeModalFrame(Root, 820.0f), Close};
}

FRbScreenParts Dialogue(const FText& Speaker, const FText& Text, TFunction<void()> OnContinue)
{
	TSharedPtr<SButton> Continue;
	TSharedRef<SVerticalBox> Box = Column();
	AddRow(Box, SNew(STextBlock).Text(Speaker).Font(Font(16, true)).ColorAndOpacity(Accent), 8.0f);
	AddRow(Box, MakeBody(Text, 16), 16.0f);
	Box->AddSlot().AutoHeight().HAlign(HAlign_Right)[MakeButton(LOCTEXT("DialogueContinue", "Continue"), Click(OnContinue), &Continue, true, true)];
	return {MakeModalFrame(Box, 820.0f), Continue};
}

FRbScreenParts Debrief(const FRbDebriefView& D, const FRbCareerView& Career, const TArray<FString>& Achievements, TFunction<void()> OnClose)
{
	TSharedPtr<SButton> Close;
	TSharedRef<SVerticalBox> Box = Column();
	AddRow(Box, MakeHeading(LOCTEXT("DebriefTitle", "Debrief: Mission 001, The Unresolved Warning")), 6.0f);
	if (!D.bAvailable)
	{
		AddRow(Box, MakeBody(LOCTEXT("NoDebrief", "Submit your assessment first."), 15));
	}
	else
	{
		AddRow(Box, SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("Outcome", "{0}   Decision quality {1}/100"),
				D.bPassed ? LOCTEXT("Passed", "PASSED") : LOCTEXT("NotPassed", "NOT PASSED"), D.OverallScore))
			.Font(Font(20, true))
			.ColorAndOpacity(D.bPassed ? Good : Bad), 4.0f);
		AddRow(Box, MakeBody(LOCTEXT("QualityNote", "Decision quality is judged on the evidence you had, separately from how events turn out."), 13, TextMuted), 14.0f);

		AddRow(Box, MakeHeading(LOCTEXT("Criteria", "How your assessment was judged"), 16), 6.0f);
		for (const FRbCriterionView& C : D.Criteria)
		{
			AddRow(Box, SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("CriterionFmt", "{0}  (weight {1}%):  {2}/100"), FText::FromString(C.Name), C.Weight, C.Score))
					.Font(Font(14, true))
					.ColorAndOpacity(C.Score >= 60 ? TextPrimary : Bad)
				]
				+ SVerticalBox::Slot().AutoHeight()[MakeBody(FText::FromString(C.Feedback), 13, TextMuted)], 6.0f);
		}

		AddRow(Box, MakeHeading(LOCTEXT("LossTitle", "Loss exposure before treatment"), 16), 4.0f);
		AddRow(Box, MakeBody(FText::Format(LOCTEXT("ExpectedLoss", "Expected loss (probability-weighted average per year): {0}"), Money(D.BaselineExpectedLoss)), 14), 2.0f);
		AddRow(Box, MakeBody(FText::Format(LOCTEXT("WorstCase", "Worst single scenario (not the expected loss): {0}"), Money(D.BaselineWorstCase)), 14), 12.0f);

		AddRow(Box, MakeHeading(LOCTEXT("AppraisalTitle", "Treatment appraisal"), 16), 4.0f);
		const FRbAppraisalView& A = D.Chosen;
		const FRbAppraisalView& R = D.Recommended;
		AddRow(Box, TableRow(LOCTEXT("Metric", "Metric"), FText::Format(LOCTEXT("YourChoice", "Your choice: {0}"), FText::FromString(A.TreatmentLabel)), FText::FromString(R.TreatmentLabel), true), 4.0f);
		AddRow(Box, TableRow(LOCTEXT("Reduction", "Annual risk reduction"), Money(A.AnnualRiskReduction), Money(R.AnnualRiskReduction)), 2.0f);
		AddRow(Box, TableRow(LOCTEXT("PvBenefit", "PV of benefit"), Money(A.PvBenefit), Money(R.PvBenefit)), 2.0f);
		AddRow(Box, TableRow(LOCTEXT("PvCost", "PV of cost"), Money(A.PvCost), Money(R.PvCost)), 2.0f);
		AddRow(Box, TableRow(LOCTEXT("Npv", "NPV"), Money(A.Npv), Money(R.Npv)), 2.0f);
		AddRow(Box, TableRow(LOCTEXT("Roi", "ROI"), Percent(A.Roi), Percent(R.Roi)), 2.0f);
		AddRow(Box, TableRow(LOCTEXT("Bcr", "Benefit-cost ratio"), Ratio2(A.BenefitCostRatio), Ratio2(R.BenefitCostRatio)), 2.0f);
		AddRow(Box, TableRow(LOCTEXT("CostEffective", "Cost-effective (NPV > 0)"),
			A.bCostEffective ? LOCTEXT("Yes", "Yes") : LOCTEXT("No", "No"), R.bCostEffective ? LOCTEXT("Yes", "Yes") : LOCTEXT("No", "No")), 14.0f);

		AddBullets(Box, LOCTEXT("Assumptions", "Assumptions"), D.Assumptions);
		AddBullets(Box, LOCTEXT("TradeOffs", "Trade-offs"), D.TradeOffs);
		AddBullets(Box, LOCTEXT("Improvements", "What to improve"), D.Improvements);
	}

	AddRow(Box, MakeHeading(LOCTEXT("CareerTitle", "Career"), 16), 4.0f);
	AddRow(Box, MakeBody(FText::Format(LOCTEXT("CareerLine", "Level {0}: {1}.  Passed missions: {2}.  XP {3} (feedback only; promotion needs competence evidence)."),
		Career.Level, FText::FromString(Career.Title), FText::FromString(FString::Join(Career.PassedMissions, TEXT(", "))), Career.Xp), 14), 4.0f);
	AddBullets(Box, LOCTEXT("Gates", "Before your next promotion"), Career.UnmetPromotionGates);
	AddBullets(Box, LOCTEXT("Achievements", "Achievements"), Achievements);

	TSharedRef<SVerticalBox> Root = Column();
	Root->AddSlot().FillHeight(1.0f)[Scrolling(Box)];
	Root->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f).HAlign(HAlign_Right)
	[
		MakeButton(LOCTEXT("CloseDebrief", "Close"), Click(OnClose), &Close)
	];
	return {MakeModalFrame(Root, 1000.0f), Close};
}

FText BriefingText(bool bHints)
{
	FText Text = LOCTEXT("Briefing",
		"Welcome to Valtren Coldline Logistics. Straight to it: Bay 3 at our Northgate cold store had another temperature excursion last night. "
		"42,000 of client stock written off, and that is the third excursion in six months.\n\n"
		"Our register still rates R-014 as Medium with Effective controls, and nobody has reviewed it in 14 months. "
		"I want your independent assessment. Start with the incident report in your inbox and the R-014 entry, both on your workstation in the analyst workspace. "
		"Then find out whether the controls actually work.\n\n"
		"When you are ready, complete the assessment form on your workstation and come back here for a debrief.");
	if (bHints)
	{
		Text = FText::Format(LOCTEXT("BriefingHints", "{0}\n\nHint: the alarm log is on the monitoring room terminal, the maintenance binder is in the meeting room, "
			"and the manual temperature checks are on a clipboard in the training area. My filing cabinet has the insurance schedule; the client contract is in the boardroom."), Text);
	}
	return Text;
}

FText RegisterEntryText(const FRbRegisterEntryView& E)
{
	if (!E.bFound) return LOCTEXT("NoEntry", "Register entry not found.");
	FString S = FString::Printf(TEXT("Current register record %s: %s\nCategory: %s    Owner: %s\nCause: %s\nEvent: %s\nConsequence: %s\n"),
		*E.Id, *E.Title, *E.Category, *E.Owner, *E.Cause, *E.Event, *E.Consequence);
	S += FString::Printf(TEXT("Inherent: L%d x I%d (%s)    Residual: L%d x I%d (%s)\nAppetite: %s    Treatment: %s    Status: %s\n"),
		E.InherentLikelihood, E.InherentImpact, *E.InherentRating, E.ResidualLikelihood, E.ResidualImpact, *E.ResidualRating,
		*E.Appetite, *E.Treatment, *E.Status);
	if (!E.ActionOwner.IsEmpty()) S += FString::Printf(TEXT("Action owner: %s\n"), *E.ActionOwner);
	if (!E.Controls.IsEmpty()) S += TEXT("Recorded controls: ") + FString::Join(E.Controls, TEXT("; "));
	return FText::FromString(S);
}
} // namespace RbScreens

#undef LOCTEXT_NAMESPACE
