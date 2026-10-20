#include "SRbHudOverlay.h"

#include "HAL/PlatformTime.h"
#include "RbUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Riskbound"

static constexpr double NoticeSeconds = 4.5;
static constexpr double NoticeFadeSeconds = 0.6;
static constexpr int32 MaxNotices = 3;

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
		.Padding(RbUI::Space6)
		[
			SNew(SBorder)
			.Visibility_Lambda(GameplayVisibility)
			.BorderImage(RbUI::PanelBrush())
			.Padding(FMargin(RbUI::Space4, RbUI::Space3))
			[
				SNew(SBox)
				.MaxDesiredWidth(520.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						RbUI::MakeEyebrow(LOCTEXT("ObjectiveHeader", "Objective   -   J journal   Esc pause"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, RbUI::Space1, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(InArgs._Objective)
						.Font(RbUI::FontW(16, RbUI::EWeight::Medium))
						.ColorAndOpacity(RbUI::TextPrimary)
						.AutoWrapText(true)
					]
				]
			]
		]
		// Crosshair: a small dot.
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Visibility_Lambda(GameplayVisibility)
			.Text(FText::FromString(TEXT("·")))
			.Font(RbUI::FontW(28, RbUI::EWeight::Bold))
			.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.85f))
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
			.Padding(FMargin(RbUI::Space4, RbUI::Space2))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, RbUI::Space2, 0.0f)
				[
					SNew(SBorder)
					.BorderImage(RbUI::FocusBrush())
					.Padding(FMargin(7.0f, 1.0f))
					[
						SNew(STextBlock).Text(LOCTEXT("KeyE", "E")).Font(RbUI::FontW(14, RbUI::EWeight::Bold)).ColorAndOpacity(RbUI::Accent)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([Prompt]() { return Prompt.Get(); })
					.Font(RbUI::FontW(15, RbUI::EWeight::Medium))
					.ColorAndOpacity(RbUI::TextPrimary)
				]
			]
		]
		// Notices: one queue, top right, clear of every modal's content area.
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(RbUI::Space6)
		[
			SNew(SBox)
			.WidthOverride(380.0f)
			[
				SAssignNew(NoticeBox, SVerticalBox)
			]
		]
	];
}

void SRbHudOverlay::AddNotice(const FString& Message)
{
	const double Now = FPlatformTime::Seconds();
	// Identical notices (e.g. repeated "Progress autosaved") refresh the existing one instead of stacking.
	for (FNotice& N : Notices)
	{
		if (N.Text == Message)
		{
			N.ExpiresAt = Now + NoticeSeconds;
			return;
		}
	}
	Notices.Add({Message, Now + NoticeSeconds});
	while (Notices.Num() > MaxNotices)
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

float SRbHudOverlay::NoticeAlpha(const FString& Message) const
{
	if (RbUI::IsReducedMotion()) return 1.0f;
	for (const FNotice& N : Notices)
	{
		if (N.Text == Message)
		{
			return static_cast<float>(FMath::Clamp((N.ExpiresAt - FPlatformTime::Seconds()) / NoticeFadeSeconds, 0.0, 1.0));
		}
	}
	return 0.0f;
}

void SRbHudOverlay::RebuildNotices()
{
	NoticeBox->ClearChildren();
	for (int32 i = 0; i < Notices.Num(); ++i)
	{
		const FString Message = Notices[i].Text;
		NoticeBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, RbUI::Space2)
		[
			SNew(SBorder)
			.BorderImage(RbUI::PanelBrush())
			.Padding(FMargin(RbUI::Space4, RbUI::Space2 + 2.0f))
			.ColorAndOpacity_Lambda([this, Message]() { return FLinearColor(1.0f, 1.0f, 1.0f, NoticeAlpha(Message)); })
			.BorderBackgroundColor_Lambda([this, Message]() { return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, NoticeAlpha(Message))); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, RbUI::Space3, 0.0f)
				[
					SNew(SBox).WidthOverride(3.0f).HeightOverride(18.0f)[SNew(SBorder).BorderImage(RbUI::FocusBrush())]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(Message)).Font(RbUI::FontW(14, RbUI::EWeight::Regular)).ColorAndOpacity(RbUI::TextPrimary).AutoWrapText(true)
				]
			]
		];
	}
}

#undef LOCTEXT_NAMESPACE
