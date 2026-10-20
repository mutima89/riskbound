#include "SRbHudOverlay.h"

#include "HAL/PlatformTime.h"
#include "RbUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Riskbound"

static constexpr double NoticeSeconds = 5.0;

void SRbHudOverlay::Construct(const FArguments& InArgs)
{
	ShowGameplay = InArgs._ShowGameplay;
	const TAttribute<FText> Prompt = InArgs._Prompt;
	auto GameplayVisibility = [this]() { return ShowGameplay.Get() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; };

	SetVisibility(EVisibility::HitTestInvisible);
	ChildSlot
	[
		SNew(SOverlay)
		// Objective, top left.
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(24.0f)
		[
			SNew(SBorder)
			.Visibility_Lambda(GameplayVisibility)
			.BorderImage(RbUI::PanelBrush())
			.Padding(FMargin(14.0f, 10.0f))
			[
				SNew(SBox)
				.MaxDesiredWidth(520.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ObjectiveHeader", "OBJECTIVE   (J: journal, Esc: pause)"))
						.Font(RbUI::Font(11, true))
						.ColorAndOpacity(RbUI::Accent)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(InArgs._Objective)
						.Font(RbUI::Font(15))
						.ColorAndOpacity(RbUI::TextPrimary)
						.AutoWrapText(true)
					]
				]
			]
		]
		// Crosshair.
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Visibility_Lambda(GameplayVisibility)
			.Text(FText::FromString(TEXT("+")))
			.Font(RbUI::Font(18, true))
			.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.8f))
		]
		// Interaction prompt, below the crosshair.
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(0.0f, 140.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
			.Visibility_Lambda([this, Prompt]()
			{
				return ShowGameplay.Get() && !Prompt.Get().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			.BorderImage(RbUI::PanelBrush())
			.Padding(FMargin(14.0f, 8.0f))
			[
				SNew(STextBlock)
				.Text_Lambda([Prompt]() { return FText::Format(LOCTEXT("PromptFmt", "[E]  {0}"), Prompt.Get()); })
				.Font(RbUI::Font(15, true))
				.ColorAndOpacity(RbUI::TextPrimary)
			]
		]
		// Notices, bottom centre.
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(0.0f, 0.0f, 0.0f, 48.0f)
		[
			SAssignNew(NoticeBox, SVerticalBox)
		]
	];
}

void SRbHudOverlay::AddNotice(const FString& Message)
{
	Notices.Add({Message, FPlatformTime::Seconds() + NoticeSeconds});
	while (Notices.Num() > 4)
	{
		Notices.RemoveAt(0);
	}
	RebuildNotices();
}

void SRbHudOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	const double Now = FPlatformTime::Seconds();
	if (Notices.RemoveAll([Now](const FNotice& N) { return N.ExpiresAt < Now; }) > 0)
	{
		RebuildNotices();
	}
}

void SRbHudOverlay::RebuildNotices()
{
	NoticeBox->ClearChildren();
	for (const FNotice& N : Notices)
	{
		NoticeBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 3.0f)
		[
			SNew(SBorder)
			.BorderImage(RbUI::PanelBrush())
			.Padding(FMargin(14.0f, 7.0f))
			[
				SNew(STextBlock).Text(FText::FromString(N.Text)).Font(RbUI::Font(14)).ColorAndOpacity(RbUI::TextPrimary)
			]
		];
	}
}

#undef LOCTEXT_NAMESPACE
