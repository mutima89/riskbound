#include "SRbToolHub.h"

#include "RbQuant.h"
#include "Framework/Application/SlateApplication.h"
#include "RbToolsSubsystem.h"
#include "RbUIStyle.h"
#include "SRbCharts.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Riskbound"

using namespace RbUI;

namespace
{
FOnClicked HubClick(TFunction<void()> Fn)
{
	return FOnClicked::CreateLambda([Fn]() { Fn(); return FReply::Handled(); });
}

FText HubNum(double V, int32 Fraction = 0)
{
	FNumberFormattingOptions O;
	O.MinimumFractionalDigits = Fraction;
	O.MaximumFractionalDigits = Fraction;
	return FText::AsNumber(V, &O);
}

FText HubPct(double V)
{
	FNumberFormattingOptions O;
	O.MaximumFractionalDigits = 1;
	return FText::AsPercent(V, &O);
}

FLinearColor HubRatingColor(const FString& Rating)
{
	if (Rating == TEXT("Critical")) return Bad;
	if (Rating == TEXT("High")) return Warning;
	if (Rating == TEXT("Medium")) return FLinearColor::FromSRGBColor(FColor(0xC9, 0xB4, 0x58));
	return Good;
}

FLinearColor HubStatusColor(const FString& Status)
{
	return Status == TEXT("Red") || Status == TEXT("Overdue") ? Bad : (Status == TEXT("Amber") ? Warning : Good);
}

TSharedRef<SWidget> HubCard(TSharedRef<SWidget> Content)
{
	return SNew(SBorder).BorderImage(CardBrush()).Padding(Space4)[Content];
}

TSharedRef<SWidget> HubCell(const FText& Text, float Width, const FLinearColor& Color = TextPrimary, bool bBold = false)
{
	return SNew(SBox).WidthOverride(Width)
		[
			// Long values end in an ellipsis and show in full on hover, instead of being clipped.
			SNew(STextBlock).Text(Text).ToolTipText(Text).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			.Font(FontW(13, bBold ? EWeight::SemiBold : EWeight::Regular)).ColorAndOpacity(Color)
		];
}

TSharedRef<SWidget> HubKeyValue(const FText& Key, const FText& Value, const FLinearColor& Color = TextPrimary)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(340.0f)[MakeBody(Key, 14, TextMuted)]]
		+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(Value).Font(FontW(15, EWeight::SemiBold)).ColorAndOpacity(Color)];
}
} // namespace

void SRbToolHub::Construct(const FArguments& InArgs)
{
	Tools = InArgs._Tools;
	OnClose = InArgs._OnClose;
	Scenarios = {{TEXT("Frequent, small"), 0.4, 20000.0}, {TEXT("Occasional, serious"), 0.1, 150000.0}, {TEXT("Rare, severe"), 0.02, 900000.0}};
	Appraisal.ExpectedLossBefore = 80000.0;
	Appraisal.ExpectedLossAfter = 30000.0;
	Appraisal.OneOffCost = 40000.0;
	Appraisal.AnnualCost = 10000.0;
	if (Tools.IsValid() && Tools->GetStressPresets().Num()) Shock = Tools->GetStressPresets()[0];


	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[MakeEyebrow(LOCTEXT("HubEyebrow", "Workstation  /  Risk tools"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, Space1, 0.0f, Space3)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)[MakeHeading(LOCTEXT("HubTitle", "Risk tools"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(FontW(13, EWeight::Medium)).ColorAndOpacity(TextMuted)
				.Text_Lambda([this]() { return Tools.IsValid() ? FText::Format(LOCTEXT("HubWeek", "Week {0}"), Tools->GetWeek()) : FText::GetEmpty(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space3)[SAssignNew(TabBar, SBox)]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(Body, SBox)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, Space3, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text_Lambda([this]() { return Status; }).Font(FontW(13, EWeight::Medium)).ColorAndOpacity(TextMuted).AutoWrapText(true)
			]
			+ SHorizontalBox::Slot().AutoWidth()[MakeButton(LOCTEXT("HubClose", "Back to workstation"), HubClick([this]() { OnClose(); }))]
		]
	];
	Rebuild();
}

void SRbToolHub::ShowTab(ETab Tab)
{
	Current = Tab;
	Status = FText::GetEmpty();
	Rebuild();
}

// The open tool is shown by the accent fill, not by keyboard focus (which follows the user).
void SRbToolHub::RebuildTabs()
{
	if (!TabBar.IsValid()) return;
	const FText Names[] = {LOCTEXT("TabRegister", "Risk register"), LOCTEXT("TabKri", "KRIs"), LOCTEXT("TabLoss", "Loss scenarios"),
		LOCTEXT("TabCba", "Cost-benefit"), LOCTEXT("TabSens", "Sensitivity"), LOCTEXT("TabStress", "Stress test"),
		LOCTEXT("TabMc", "Monte Carlo"), LOCTEXT("TabActions", "Actions"), LOCTEXT("TabReport", "Report")};
	const bool bHadFocus = FirstTab.IsValid() && FirstTab->HasKeyboardFocus();
	TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);
	for (int32 i = 0; i < static_cast<int32>(ETab::Count); ++i)
	{
		TSharedPtr<SButton> Btn;
		const ETab Tab = static_cast<ETab>(i);
		Bar->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Space1, 0.0f)
		[
			MakeButton(Names[i], HubClick([this, Tab]() { ShowTab(Tab); }), &Btn, true, Tab == Current)
		];
		if (Tab == Current) FirstTab = Btn;
	}
	TabBar->SetContent(Bar);
	if (bHadFocus && FirstTab.IsValid()) FSlateApplication::Get().SetKeyboardFocus(FirstTab, EFocusCause::SetDirectly);
}

void SRbToolHub::Rebuild()
{
	if (!Tools.IsValid() || !Body.IsValid()) return;
	RebuildTabs();
	TSharedRef<SWidget> Content = SNullWidget::NullWidget;
	switch (Current)
	{
	case ETab::Register: Content = BuildRegister(); break;
	case ETab::Kri: Content = BuildKri(); break;
	case ETab::Loss: Content = BuildLoss(); break;
	case ETab::CostBenefit: Content = BuildCostBenefit(); break;
	case ETab::Sensitivity: Content = BuildSensitivity(); break;
	case ETab::Stress: Content = BuildStress(); break;
	case ETab::MonteCarlo: Content = BuildMonteCarlo(); break;
	case ETab::Actions: Content = BuildActions(); break;
	case ETab::Report: Content = BuildReport(); break;
	default: break;
	}
	Body->SetContent(Content);
}

TSharedRef<SWidget> SRbToolHub::NumberField(const FText& Label, double* Value, double Min, double Max, double Delta, int32 Fraction)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(230.0f)[MakeBody(Label, 14)]]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(170.0f)
			[
				SNew(SSpinBox<double>)
				.MinValue(Min).MaxValue(Max).Delta(Delta)
				.MinFractionalDigits(Fraction).MaxFractionalDigits(FMath::Max(Fraction, 0))
				.Font(FontW(14, EWeight::Regular))
				.Value_Lambda([Value]() { return *Value; })
				.OnValueChanged_Lambda([Value](double V) { *Value = V; })
				.OnValueCommitted_Lambda([this, Value](double V, ETextCommit::Type) { *Value = V; Rebuild(); })
			]
		];
}

TSharedRef<SWidget> SRbToolHub::TextField(const FText& Label, FString* Value, const FText& Hint)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(230.0f)[MakeBody(Label, 14)]]
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SEditableTextBox)
			.Font(FontW(14, EWeight::Regular))
			.HintText(Hint)
			.Text_Lambda([Value]() { return FText::FromString(*Value); })
			.OnTextChanged_Lambda([Value](const FText& T) { *Value = T.ToString(); })
		];
}

// ---------------------------------------------------------------- register

TSharedRef<SWidget> SRbToolHub::BuildRegister()
{
	URbToolsSubsystem* T = Tools.Get();
	TSharedRef<SVerticalBox> Left = SNew(SVerticalBox);
	// Filters.
	Left->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, Space2, 0.0f)
		[
			SNew(SEditableTextBox).Font(FontW(14, EWeight::Regular)).HintText(LOCTEXT("SearchHint", "Search id, title, cause, owner..."))
			.Text(FText::FromString(Filter.Search))
			.OnTextCommitted_Lambda([this](const FText& Txt, ETextCommit::Type) { Filter.Search = Txt.ToString(); Rebuild(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, Space2, 0.0f)
		[
			MakeButton(FText::Format(LOCTEXT("CategoryFilter", "Category: {0}"), Filter.Category.IsEmpty() ? LOCTEXT("Any", "Any") : FText::FromString(Filter.Category)),
				HubClick([this, T]()
				{
					const TArray<FString> Cats = T->GetCategories();
					const int32 Next = Cats.IndexOfByKey(Filter.Category) + 1;
					Filter.Category = Next < Cats.Num() ? Cats[Next] : FString();
					Rebuild();
				}))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SCheckBox).Style(&CheckStyle())
			.IsChecked(Filter.bOnlyOutsideAppetite ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { Filter.bOnlyOutsideAppetite = S == ECheckBoxState::Checked; Rebuild(); })
			[MakeBody(LOCTEXT("OutsideAppetite", "Outside appetite only"), 13)]
		]
	];
	// Table.
	TSharedRef<SVerticalBox> Table = SNew(SVerticalBox);
	Table->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space1)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[HubCell(LOCTEXT("ColId", "Id"), 64, TextMuted, true)]
		+ SHorizontalBox::Slot().AutoWidth()[HubCell(LOCTEXT("ColTitle", "Title"), 300, TextMuted, true)]
		+ SHorizontalBox::Slot().AutoWidth()[HubCell(LOCTEXT("ColCat", "Category"), 110, TextMuted, true)]
		+ SHorizontalBox::Slot().AutoWidth()[HubCell(LOCTEXT("ColRes", "Residual (ordinal)"), 170, TextMuted, true)]
		+ SHorizontalBox::Slot().AutoWidth()[HubCell(LOCTEXT("ColApp", "Appetite"), 140, TextMuted, true)]
		+ SHorizontalBox::Slot().AutoWidth()[HubCell(LOCTEXT("ColTreat", "Treatment"), 110, TextMuted, true)]
	];
	for (const FRbRegisterRow& R : T->GetRegister(Filter))
	{
		const FString Id = R.Id;
		Table->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[HubCell(FText::FromString(R.Id), 64)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[HubCell(FText::FromString(R.Title), 300)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[HubCell(FText::FromString(R.Category), 110, TextMuted)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				HubCell(FText::Format(LOCTEXT("ResidualFmt", "{0} x {1} = {2} {3}"), R.ResidualLikelihood, R.ResidualImpact, R.ResidualScore, FText::FromString(R.Rating)), 170, HubRatingColor(R.Rating))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[HubCell(FText::FromString(R.Appetite), 140, R.Appetite.StartsWith(TEXT("Exceeds")) ? Bad : TextMuted)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[HubCell(FText::FromString(R.Treatment), 110, TextMuted)]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				MakeButton(LOCTEXT("Edit", "Edit"), HubClick([this, Id]()
				{
					EditingId = Id;
					RiskForm = Tools->GetRiskInput(Id);
					Rebuild();
				}))
			]
		];
	}
	Left->AddSlot().AutoHeight()[HubCard(Table)];

	// Heatmap of residual positions.
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(2.0f));
	int32 Index = 0;
	const TArray<FRbMatrixCell> Cells = T->GetMatrix();
	const int32 N = FMath::Max(1, FMath::RoundToInt(FMath::Sqrt(static_cast<float>(Cells.Num()))));
	for (const FRbMatrixCell& C : Cells)
	{
		Grid->AddSlot(Index % N, Index / N)
		[
			SNew(SBox).WidthOverride(46.0f).HeightOverride(40.0f)
			[
				SNew(SBorder).BorderImage(CardBrush()).BorderBackgroundColor(HubRatingColor(C.Rating) * FLinearColor(1, 1, 1, C.Count ? 0.95f : 0.35f))
				.HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(C.Count ? FText::AsNumber(C.Count) : FText::GetEmpty()).Font(FontW(15, EWeight::Bold)).ColorAndOpacity(FLinearColor::White)
				]
			]
		];
		++Index;
	}
	TSharedRef<SWidget> Heat = HubCard(SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[MakeHeading(LOCTEXT("Heatmap", "Residual heatmap"), 15)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, Space2)[MakeBody(LOCTEXT("HeatHelp", "Likelihood 5 (top) to 1; impact 1 (left) to 5. Counts are open risks. Ordinal, not money."), 12, TextMuted)]
		+ SVerticalBox::Slot().AutoHeight()[Grid]);

	// Add / edit form.
	const bool bEditing = !EditingId.IsEmpty();
	TSharedRef<SVerticalBox> Form = SNew(SVerticalBox);
	Form->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)
	[
		MakeHeading(bEditing ? FText::Format(LOCTEXT("EditRisk", "Edit {0}"), FText::FromString(EditingId)) : LOCTEXT("NewRisk", "Add a risk"), 15)
	];
	Form->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[TextField(LOCTEXT("FTitle", "Title"), &RiskForm.Title)];
	Form->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[TextField(LOCTEXT("FOwner", "Owner"), &RiskForm.Owner)];
	Form->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[TextField(LOCTEXT("FCause", "Cause"), &RiskForm.Cause)];
	Form->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[TextField(LOCTEXT("FEvent", "Event"), &RiskForm.Event)];
	Form->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[TextField(LOCTEXT("FCons", "Consequence"), &RiskForm.Consequence)];
	Form->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(230.0f)[MakeBody(LOCTEXT("FCategory", "Category"), 14)]]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			MakeButton(FText::FromString(RiskForm.Category), HubClick([this, T]()
			{
				const TArray<FString> Cats = T->GetCategories();
				if (Cats.Num()) RiskForm.Category = Cats[(Cats.IndexOfByKey(RiskForm.Category) + 1) % Cats.Num()];
				Rebuild();
			}))
		]
	];
	auto Level = [this](const FText& Label, int32* Value)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(230.0f)[MakeBody(Label, 14)]];
		for (int32 L = 1; L <= 5; ++L)
		{
			Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Space3, 0.0f)
			[
				SNew(SCheckBox).Style(&RadioStyle())
				.IsChecked_Lambda([Value, L]() { return *Value == L ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([Value, L](ECheckBoxState) { *Value = L; })
				[MakeBody(FText::AsNumber(L), 14)]
			];
		}
		return Row;
	};
	Form->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[Level(LOCTEXT("FLikelihood", "Inherent likelihood"), &RiskForm.Likelihood)];
	Form->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[Level(LOCTEXT("FImpact", "Inherent impact"), &RiskForm.Impact)];
	Form->AddSlot().AutoHeight().Padding(0.0f, Space2, 0.0f, 0.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, Space2, 0.0f)
		[
			MakeButton(bEditing ? LOCTEXT("SaveRisk", "Save changes") : LOCTEXT("AddRisk", "Add to register"), HubClick([this, bEditing]()
			{
				FString Error;
				if (bEditing)
				{
					Status = Tools->UpdateRisk(EditingId, RiskForm, Error) ? FText::Format(LOCTEXT("Updated", "{0} updated."), FText::FromString(EditingId)) : FText::FromString(Error);
				}
				else
				{
					const FString Id = Tools->AddRisk(RiskForm, Error);
					Status = Id.IsEmpty() ? FText::FromString(Error) : FText::Format(LOCTEXT("Added", "{0} added to the register."), FText::FromString(Id));
					if (!Id.IsEmpty()) RiskForm = FRbRiskInput();
				}
				Rebuild();
			}), nullptr, true, true)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			MakeButton(LOCTEXT("NewForm", "New risk"), HubClick([this]() { EditingId.Empty(); RiskForm = FRbRiskInput(); Rebuild(); }))
		]
	];

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Left]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, Space3, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, Space3, 0.0f)[HubCard(Form)]
			+ SHorizontalBox::Slot().AutoWidth()[Heat]
		];
}

// ---------------------------------------------------------------- KRIs

TSharedRef<SWidget> SRbToolHub::BuildKri()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)
	[
		MakeBody(LOCTEXT("KriHelp", "Latest weekly reading, the trend since week -11 and the amber (dashed amber) and red guides. Amber three weeks running escalates; red always does."), 13, TextMuted)
	];
	for (const FRbKriView& K : Tools->GetKris())
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)
		[
			HubCard(SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[MakeBody(FText::FromString(K.Name), 15)]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeBody(FText::Format(LOCTEXT("KriThresh", "Amber {0}  /  Red {1}  /  {2}"), HubNum(K.Amber, 1), HubNum(K.Red, 1),
							K.bHigherIsWorse ? LOCTEXT("HigherWorse", "higher is worse") : LOCTEXT("LowerWorse", "lower is worse")), 12, TextMuted)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(Space3, 0.0f)
				[
					SNew(SBox).WidthOverride(190.0f)
					[
						SNew(STextBlock).Text(FText::Format(LOCTEXT("KriValue", "{0} {1}"), HubNum(K.Value, 1), FText::FromString(K.Unit)))
						.Font(FontW(18, EWeight::SemiBold)).ColorAndOpacity(HubStatusColor(K.Status))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(Space3, 0.0f)
				[
					SNew(SBox).WidthOverride(150.0f)
					[
						MakeBody(K.bEscalate ? FText::Format(LOCTEXT("Escalate", "{0}: ESCALATE"), FText::FromString(K.Status))
							: FText::Format(LOCTEXT("NoEscalate", "{0}, amber streak {1}"), FText::FromString(K.Status), K.AmberStreak), 13, HubStatusColor(K.Status))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SRbSparkline).Values(K.History).Amber(K.Amber).Red(K.Red)
				])
		];
	}
	return Box;
}

// ---------------------------------------------------------------- loss scenarios

TSharedRef<SWidget> SRbToolHub::BuildLoss()
{
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	Rows->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)
	[
		MakeBody(LOCTEXT("LossHelp", "Your own scenarios for one risk: annual probability (0 to 1, mutually exclusive) and loss if it happens. Expected loss is an average per year; the worst case is shown beside it."), 13, TextMuted)
	];
	for (int32 i = 0; i < Scenarios.Num(); ++i)
	{
		FRbScenarioInput* S = &Scenarios[i];
		Rows->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, Space2, 0.0f)
			[
				SNew(SEditableTextBox).Font(FontW(14, EWeight::Regular)).Text(FText::FromString(S->Name))
				.OnTextChanged_Lambda([S](const FText& T) { S->Name = T.ToString(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, Space2, 0.0f)
			[
				SNew(SBox).WidthOverride(120.0f)
				[
					SNew(SSpinBox<double>).MinValue(0.0).MaxValue(1.0).Delta(0.01).MinFractionalDigits(2).MaxFractionalDigits(3).Font(FontW(14, EWeight::Regular))
					.Value_Lambda([S]() { return S->AnnualProbability; })
					.OnValueChanged_Lambda([S](double V) { S->AnnualProbability = V; })
					.OnValueCommitted_Lambda([this, S](double V, ETextCommit::Type) { S->AnnualProbability = V; Rebuild(); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, Space2, 0.0f)
			[
				SNew(SBox).WidthOverride(170.0f)
				[
					SNew(SSpinBox<double>).MinValue(0.0).MaxValue(100000000.0).Delta(1000.0).Font(FontW(14, EWeight::Regular))
					.Value_Lambda([S]() { return S->Loss; })
					.OnValueChanged_Lambda([S](double V) { S->Loss = V; })
					.OnValueCommitted_Lambda([this, S](double V, ETextCommit::Type) { S->Loss = V; Rebuild(); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				MakeButton(LOCTEXT("Remove", "Remove"), HubClick([this, i]() { Scenarios.RemoveAt(i); Rebuild(); }))
			]
		];
	}
	const FRbLossView L = Tools->EvaluateScenarios(Scenarios);
	TSharedRef<SVerticalBox> Result = SNew(SVerticalBox);
	if (!L.bValid) Result->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)[MakeBody(FText::FromString(L.Problem), 13, Bad)];
	Result->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("EL", "Expected loss (average per year)"), Money(L.ExpectedLoss))];
	Result->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("Worst", "Worst single scenario (not the EL)"), Money(L.WorstCase), Warning)];
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[HubCard(Rows)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, Space2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, Space2, 0.0f)
			[
				MakeButton(LOCTEXT("AddScenario", "Add scenario"), HubClick([this]() { Scenarios.Add({TEXT("New scenario"), 0.05, 50000.0}); Rebuild(); }))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, Space2, 0.0f)
			[
				SNew(SBox).WidthOverride(120.0f)
				[
					SNew(SEditableTextBox).Font(FontW(14, EWeight::Regular)).Text(FText::FromString(QuantRiskId))
					.OnTextChanged_Lambda([this](const FText& T) { QuantRiskId = T.ToString(); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				MakeButton(LOCTEXT("SaveQuant", "Save to register"), HubClick([this]()
				{
					Status = Tools->SaveQuantification(QuantRiskId, Scenarios)
						? FText::Format(LOCTEXT("QuantSaved", "Scenarios saved to {0}; they appear in the executive report."), FText::FromString(QuantRiskId))
						: LOCTEXT("QuantFailed", "Not saved: check the risk id and that the scenarios are valid.");
					Rebuild();
				}), nullptr, true, true)
			]
		]
		+ SVerticalBox::Slot().AutoHeight()[HubCard(Result)];
}

// ---------------------------------------------------------------- cost-benefit and sensitivity

TSharedRef<SWidget> SRbToolHub::BuildCostBenefit()
{
	const FRbAppraisalView A = Tools->Appraise(Appraisal);
	TSharedRef<SVerticalBox> In = SNew(SVerticalBox);
	In->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[NumberField(LOCTEXT("ELBefore", "Expected loss before (per year)"), &Appraisal.ExpectedLossBefore, 0, 1e9, 1000)];
	In->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[NumberField(LOCTEXT("ELAfter", "Expected loss after (per year)"), &Appraisal.ExpectedLossAfter, 0, 1e9, 1000)];
	In->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[NumberField(LOCTEXT("OneOff", "One-off cost (year 0)"), &Appraisal.OneOffCost, 0, 1e9, 1000)];
	In->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[NumberField(LOCTEXT("Annual", "Annual running cost"), &Appraisal.AnnualCost, 0, 1e9, 500)];
	In->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(230.0f)[MakeBody(LOCTEXT("Years", "Years"), 14)]]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(170.0f)
			[
				SNew(SSpinBox<int32>).MinValue(1).MaxValue(30).Font(FontW(14, EWeight::Regular))
				.Value_Lambda([this]() { return Appraisal.Years; })
				.OnValueChanged_Lambda([this](int32 V) { Appraisal.Years = V; })
				.OnValueCommitted_Lambda([this](int32 V, ETextCommit::Type) { Appraisal.Years = V; Rebuild(); })
			]
		]
	];
	In->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[NumberField(LOCTEXT("Rate", "Discount rate"), &Appraisal.DiscountRate, 0, 0.5, 0.005, 3)];
	TSharedRef<SVerticalBox> Out = SNew(SVerticalBox);
	Out->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("Reduction", "Annual risk reduction"), Money(A.AnnualRiskReduction))];
	Out->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("PvBen", "PV of benefit"), Money(A.PvBenefit))];
	Out->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("PvCost", "PV of cost"), Money(A.PvCost))];
	Out->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("NpvK", "NPV"), Money(A.Npv), A.Npv > 0 ? Good : Bad)];
	Out->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("RoiK", "ROI"), HubPct(A.Roi))];
	Out->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("BcrK", "Benefit-cost ratio"), HubNum(A.BenefitCostRatio, 2))];
	Out->AddSlot().AutoHeight().Padding(0.0f, Space2, 0.0f, 0.0f)
	[
		MakeBody(LOCTEXT("CbaNote", "Benefit = discounted reduction in expected loss. Only as good as the probability estimates behind it."), 12, TextMuted)
	];
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, Space3, 0.0f)[HubCard(In)]
		+ SHorizontalBox::Slot().FillWidth(1.0f)[HubCard(Out)];
}

TSharedRef<SWidget> SRbToolHub::BuildSensitivity()
{
	const TArray<FRbSensitivityRowView> Rows = Tools->Tornado(Appraisal, SensitivityFraction);
	const double Base = Tools->Appraise(Appraisal).Npv;
	double Extent = 1.0;
	for (const FRbSensitivityRowView& R : Rows) Extent = FMath::Max(Extent, FMath::Max(FMath::Abs(R.OutputLow - Base), FMath::Abs(R.OutputHigh - Base)));
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight()[NumberField(LOCTEXT("Swing", "Move each input by +/- (fraction)"), &SensitivityFraction, 0.01, 0.9, 0.01, 2)];
	Box->AddSlot().AutoHeight().Padding(0.0f, Space2)
	[
		MakeBody(FText::Format(LOCTEXT("TornadoHelp", "NPV tornado using the Cost-benefit inputs. Base NPV {0}; widest bar = the input that matters most."), Money(Base)), 13, TextMuted)
	];
	for (const FRbSensitivityRowView& R : Rows)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[HubCell(FText::FromString(R.Name), 200)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, Space3, 0.0f)
			[
				SNew(SRbTornadoBar).Low(R.OutputLow).High(R.OutputHigh).Base(Base).Extent(Extent)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				HubCell(FText::Format(LOCTEXT("TornadoVals", "{0} to {1}  (swing {2})"), Money(R.OutputLow), Money(R.OutputHigh), Money(R.Swing)), 340, TextMuted)
			]
		];
	}
	return HubCard(Box);
}

// ---------------------------------------------------------------- stress

TSharedRef<SWidget> SRbToolHub::BuildStress()
{
	TSharedRef<SHorizontalBox> Presets = SNew(SHorizontalBox);
	for (const FRbStressPreset& P : Tools->GetStressPresets())
	{
		Presets->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Space2, 0.0f)
		[
			MakeButton(FText::FromString(P.Name), HubClick([this, P]() { Shock = P; Rebuild(); }), nullptr, true, Shock.Name == P.Name)
		];
	}
	const FRbStressView R = Tools->Stress(Scenarios, Shock, LossCapacity);
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)
	[
		MakeBody(LOCTEXT("StressHelp", "Shocks the scenarios from the Loss scenarios tab: probabilities are multiplied (capped at 1) and losses scaled. Presets are illustrative, not calibrated."), 13, TextMuted)
	];
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)[Presets];
	Box->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[NumberField(LOCTEXT("ProbMult", "Probability multiplier"), &Shock.ProbabilityMultiplier, 0.1, 10.0, 0.1, 2)];
	Box->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[NumberField(LOCTEXT("LossMult", "Loss multiplier"), &Shock.LossMultiplier, 0.1, 10.0, 0.1, 2)];
	Box->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[NumberField(LOCTEXT("Capacity", "Loss capacity (largest loss absorbable)"), &LossCapacity, 0, 1e10, 10000)];
	Box->AddSlot().AutoHeight().Padding(0.0f, Space3, 0.0f, 0.0f)[HubKeyValue(LOCTEXT("BaseEL", "Expected loss, base"), Money(R.BaseExpectedLoss))];
	Box->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("StressEL", "Expected loss, stressed"), Money(R.StressedExpectedLoss), Warning)];
	Box->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("StressWorst", "Worst case, stressed"), Money(R.StressedWorstCase), Warning)];
	Box->AddSlot().AutoHeight()
	[
		HubKeyValue(LOCTEXT("Breach", "Breaches loss capacity"), R.bBreachesCapacity ? LOCTEXT("YesBreach", "Yes") : LOCTEXT("NoBreach", "No"), R.bBreachesCapacity ? Bad : Good)
	];
	return HubCard(Box);
}

// ---------------------------------------------------------------- Monte Carlo

TSharedRef<SWidget> SRbToolHub::BuildMonteCarlo()
{
	TSharedRef<SVerticalBox> In = SNew(SVerticalBox);
	In->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[NumberField(LOCTEXT("Freq", "Events per year (average)"), &Mc.EventsPerYear, 0, 50, 0.1, 2)];
	In->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[NumberField(LOCTEXT("Median", "Single loss: median"), &Mc.MedianLoss, 1, 1e9, 1000)];
	In->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[NumberField(LOCTEXT("P90", "Single loss: 90th percentile"), &Mc.P90Loss, 1, 1e9, 1000)];
	In->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[NumberField(LOCTEXT("Control", "Control reduces each loss by"), &Mc.ControlSeverityReduction, 0, 1, 0.05, 2)];
	In->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[NumberField(LOCTEXT("Threshold", "Report P(annual loss > threshold)"), &Mc.LossThreshold, 0, 1e10, 10000)];
	auto IntField = [this](const FText& Label, int32* Value, int32 Min, int32 Max)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(230.0f)[MakeBody(Label, 14)]]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(170.0f)
				[
					SNew(SSpinBox<int32>).MinValue(Min).MaxValue(Max).Font(FontW(14, EWeight::Regular))
					.Value_Lambda([Value]() { return *Value; })
					.OnValueChanged_Lambda([Value](int32 V) { *Value = V; })
				]
			];
	};
	In->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[IntField(LOCTEXT("Trials", "Trials"), &Mc.Trials, 100, 200000)];
	In->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[IntField(LOCTEXT("Seed", "Seed (same seed = same result)"), &Mc.Seed, 0, 999999)];
	In->AddSlot().AutoHeight().Padding(0.0f, Space2, 0.0f, 0.0f)
	[
		MakeButton(LOCTEXT("RunMc", "Run simulation"), HubClick([this]() { McResult = Tools->MonteCarlo(Mc, 30); bMcRun = true; Rebuild(); }), nullptr, true, true)
	];
	TSharedRef<SVerticalBox> Out = SNew(SVerticalBox);
	if (!bMcRun)
	{
		Out->AddSlot().AutoHeight()[MakeBody(LOCTEXT("McIdle", "Set your frequency and severity estimates, then run. Severity is lognormal, fitted from your median and 90th percentile."), 13, TextMuted)];
	}
	else
	{
		const FRbMonteCarloView& R = McResult;
		Out->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("Fit", "Lognormal fit (mu, sigma)"), FText::Format(LOCTEXT("FitV", "{0}, {1}"), HubNum(R.Mu, 3), HubNum(R.Sigma, 3)))];
		Out->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("Mean", "Mean annual loss"), Money(R.Mean))];
		Out->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("Sd", "Standard deviation"), Money(R.StdDev))];
		Out->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("P50K", "P50 / P90"), FText::Format(LOCTEXT("P50V", "{0} / {1}"), Money(R.P50), Money(R.P90)))];
		Out->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("P95K", "P95 / P99"), FText::Format(LOCTEXT("P95V", "{0} / {1}"), Money(R.P95), Money(R.P99)), Warning)];
		Out->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("MaxK", "Worst trial"), Money(R.Max), Warning)];
		Out->AddSlot().AutoHeight()[HubKeyValue(LOCTEXT("Exceed", "P(annual loss > threshold)"), HubPct(R.ProbExceedThreshold))];
		const int32 HotFrom = R.BinWidth > 0.0 ? FMath::Clamp(FMath::FloorToInt(Mc.LossThreshold / R.BinWidth), 0, R.Histogram.Num()) : -1;
		Out->AddSlot().AutoHeight().Padding(0.0f, Space3, 0.0f, Space1)
		[
			MakeBody(FText::Format(LOCTEXT("HistHelp", "Annual loss distribution, {0} trials, bins of {1} up to P99 (amber: above your threshold)."), R.Trials, Money(R.BinWidth)), 12, TextMuted)
		];
		Out->AddSlot().AutoHeight()[SNew(SRbBarSeries).Counts(R.Histogram).HighlightFrom(HotFrom).Size(FVector2D(560.0, 150.0))];
	}
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, Space3, 0.0f)[HubCard(In)]
		+ SHorizontalBox::Slot().FillWidth(1.0f)[HubCard(Out)];
}

// ---------------------------------------------------------------- actions

TSharedRef<SWidget> SRbToolHub::BuildActions()
{
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (const FRbActionView& A : Tools->GetActions())
	{
		const FString Id = A.Id;
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[HubCell(FText::FromString(A.Id), 60)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[HubCell(FText::FromString(A.RiskId), 60, TextMuted)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[HubCell(FText::FromString(A.Title), 330)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[HubCell(FText::Format(LOCTEXT("Due", "due wk {0}"), A.DueWeek), 80, TextMuted)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[HubCell(FText::FromString(A.Status), 90, A.bDone ? Good : HubStatusColor(A.Status))];
		if (!A.bDone)
		{
			Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Space1, 0.0f)
			[
				MakeButton(LOCTEXT("Start", "Start"), HubClick([this, Id]() { Tools->StartAction(Id); Rebuild(); }))
			];
			Row->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, Space1, 0.0f)
			[
				SNew(SEditableTextBox).Font(FontW(13, EWeight::Regular)).HintText(LOCTEXT("EvidenceHint", "Evidence of completion"))
				.Text(FText::FromString(CloseEvidence.FindRef(Id)))
				.OnTextChanged_Lambda([this, Id](const FText& T) { CloseEvidence.Add(Id, T.ToString()); })
			];
			Row->AddSlot().AutoWidth()
			[
				MakeButton(LOCTEXT("CloseAction", "Close"), HubClick([this, Id]()
				{
					FString Error;
					Status = Tools->CloseAction(Id, CloseEvidence.FindRef(Id), Error) ? FText::Format(LOCTEXT("Closed", "{0} closed."), FText::FromString(Id)) : FText::FromString(Error);
					Rebuild();
				}))
			];
		}
		else
		{
			Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)[MakeBody(FText::FromString(A.Evidence), 12, TextMuted)];
		}
		List->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[Row];
	}
	if (Tools->GetActions().IsEmpty())
	{
		List->AddSlot().AutoHeight()[MakeBody(LOCTEXT("NoActions", "No actions yet. Submitting a treatment creates them, or add one below."), 13, TextMuted)];
	}
	TSharedRef<SVerticalBox> Form = SNew(SVerticalBox);
	Form->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)[MakeHeading(LOCTEXT("NewAction", "Add an action"), 15)];
	Form->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[TextField(LOCTEXT("ARisk", "Risk id"), &ActionRisk)];
	Form->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[TextField(LOCTEXT("ATitle", "Action"), &ActionTitle)];
	Form->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[TextField(LOCTEXT("AOwner", "Owner"), &ActionOwner)];
	Form->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[NumberField(LOCTEXT("ADue", "Due in weeks"), &ActionDue, 0, 52, 1)];
	Form->AddSlot().AutoHeight().Padding(0.0f, Space2, 0.0f, 0.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, Space2, 0.0f)
		[
			MakeButton(LOCTEXT("CreateAction", "Create action"), HubClick([this]()
			{
				FString Error;
				const FString Id = Tools->CreateAction(ActionRisk, ActionTitle, ActionOwner, FMath::RoundToInt(ActionDue), Error);
				Status = Id.IsEmpty() ? FText::FromString(Error) : FText::Format(LOCTEXT("ActionAdded", "{0} created."), FText::FromString(Id));
				if (!Id.IsEmpty()) ActionTitle.Empty();
				Rebuild();
			}), nullptr, true, true)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			MakeButton(LOCTEXT("AdvanceWeek", "Advance one week"), HubClick([this]()
			{
				const TArray<FString> Events = Tools->AdvanceWeek();
				Status = Events.IsEmpty() ? FText::Format(LOCTEXT("NowWeek", "Now week {0}."), Tools->GetWeek()) : FText::FromString(FString::Join(Events, TEXT("  ")));
				Rebuild();
			}))
		]
	];
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[HubCard(List)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, Space3, 0.0f, 0.0f)[HubCard(Form)];
}

// ---------------------------------------------------------------- report

TSharedRef<SWidget> SRbToolHub::BuildReport()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				MakeBody(ExportedPath.IsEmpty() ? LOCTEXT("ReportHelp", "Built from the current register, KRIs, actions, quantified risks and decisions.")
					: FText::Format(LOCTEXT("Exported", "Exported to {0}"), FText::FromString(ExportedPath)), 13, TextMuted)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				MakeButton(LOCTEXT("Export", "Export Markdown"), HubClick([this]()
				{
					ExportedPath = Tools->ExportReport();
					Status = ExportedPath.IsEmpty() ? LOCTEXT("ExportFail", "Export failed.") : LOCTEXT("ExportOk", "Report exported.");
					Rebuild();
				}), nullptr, true, true)
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			HubCard(SNew(STextBlock).Text(FText::FromString(Tools->GetReportMarkdown())).Font(FontW(13, EWeight::Regular)).ColorAndOpacity(TextPrimary).AutoWrapText(true))
		];
}

#undef LOCTEXT_NAMESPACE
