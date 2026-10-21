#include "SRbInbox.h"

#include "RbCampaignSubsystem.h"
#include "RbUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Riskbound"

using namespace RbUI;

namespace
{
FOnClicked InboxClick(TFunction<void()> Fn)
{
	return FOnClicked::CreateLambda([Fn]() { Fn(); return FReply::Handled(); });
}

TSharedRef<SWidget> InboxText(const FString& Text, int32 Size = 14, const FLinearColor& Color = TextPrimary, EWeight Weight = EWeight::Regular)
{
	return SNew(STextBlock).Text(FText::FromString(Text)).Font(FontW(Size, Weight)).ColorAndOpacity(Color).AutoWrapText(true);
}

FLinearColor CategoryColor(const FString& Category)
{
	if (Category == TEXT("Incident")) return Bad;
	if (Category == TEXT("Regulator")) return Warning;
	if (Category == TEXT("Story")) return FLinearColor(0.70f, 0.55f, 0.95f);
	if (Category == TEXT("Personal")) return FLinearColor(0.95f, 0.62f, 0.55f);
	if (Category == TEXT("Politics")) return FLinearColor(0.85f, 0.75f, 0.45f);
	return Accent;
}

FString DeadlineText(int32 WeeksLeft)
{
	if (WeeksLeft <= 0) return TEXT("Reply this week");
	return WeeksLeft == 1 ? FString(TEXT("Reply within 1 week")) : FString::Printf(TEXT("Reply within %d weeks"), WeeksLeft);
}
} // namespace

void SRbInbox::Construct(const FArguments& InArgs)
{
	Campaign = InArgs._Campaign;
	OnClose = InArgs._OnClose;
	TSharedPtr<SButton> Close;
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[MakeEyebrow(LOCTEXT("InboxEyebrow", "Workstation  /  Inbox"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, Space1, 0.0f, Space3)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)[MakeHeading(LOCTEXT("InboxTitle", "Inbox"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(FontW(13, EWeight::Medium)).ColorAndOpacity(TextMuted)
				.Text_Lambda([this]()
				{
					if (!Campaign.IsValid()) return FText::GetEmpty();
					const FRbWorldView W = Campaign->GetWorldView();
					return FText::Format(LOCTEXT("InboxCounts", "Week {0}  |  {1} unread  |  {2} awaiting your reply"), W.Week, W.Unread, W.AwaitingReply);
				})
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.42f).Padding(0.0f, 0.0f, Space3, 0.0f)
			[
				SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(ListBox, SBox)]
			]
			+ SHorizontalBox::Slot().FillWidth(0.58f)
			[
				SNew(SBorder).BorderImage(CardBrush()).Padding(Space6)
				[
					SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(ReaderBox, SBox)]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, Space3, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text_Lambda([this]() { return Status; }).Font(FontW(13, EWeight::Medium)).ColorAndOpacity(Warning).AutoWrapText(true)
			]
			+ SHorizontalBox::Slot().AutoWidth()[MakeButton(LOCTEXT("InboxClose", "Close"), InboxClick([this]() { OnClose(); }), &Close)]
		]
	];
	FirstFocus = Close;
	if (!InArgs._OpenMessageId.IsEmpty())
	{
		Select(InArgs._OpenMessageId);
	}
	else if (Campaign.IsValid())
	{
		const TArray<FRbInboxMessageView> Inbox = Campaign->GetInbox();
		if (!Inbox.IsEmpty()) Select(Inbox[0].Id); // the most pressing message, so the reader is never empty
	}
	Rebuild();
}

void SRbInbox::Select(const FString& MessageId)
{
	Selected = MessageId;
	Status = FText::GetEmpty();
	if (Campaign.IsValid()) Campaign->MarkMessageRead(MessageId);
	Rebuild();
}

bool SRbInbox::Reply(const FString& ChoiceId)
{
	if (!Campaign.IsValid()) return false;
	FString Error;
	const bool bOk = Campaign->ReplyToMessage(Selected, ChoiceId, Error);
	Status = bOk ? FText::GetEmpty() : FText::FromString(Error);
	Rebuild();
	return bOk;
}

void SRbInbox::Rebuild()
{
	if (!ListBox.IsValid() || !ReaderBox.IsValid()) return;
	ListBox->SetContent(BuildList());
	ReaderBox->SetContent(BuildReader());
}

TSharedRef<SWidget> SRbInbox::BuildList()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	const TArray<FRbInboxMessageView> Inbox = Campaign.IsValid() ? Campaign->GetInbox() : TArray<FRbInboxMessageView>();
	if (Inbox.IsEmpty())
	{
		Box->AddSlot().AutoHeight()[InboxText(TEXT("No messages yet. People will write once your work starts to land."), 14, TextMuted)];
		return Box;
	}
	for (const FRbInboxMessageView& M : Inbox)
	{
		const FString Id = M.Id;
		const bool bSelected = Id == Selected;
		const FLinearColor Cat = CategoryColor(M.Category);
		FString Meta = FString::Printf(TEXT("%s  |  week %d"), *M.Category.ToUpper(), M.Week);
		if (M.bNeedsReply) Meta += TEXT("  |  ") + DeadlineText(M.WeeksLeft);
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)
		[
			SNew(SButton)
			.ButtonColorAndOpacity(bSelected ? FLinearColor(0.17f, 0.64f, 0.66f, 0.35f) : FLinearColor(1.0f, 1.0f, 1.0f, 0.06f))
			.ContentPadding(FMargin(Space3, Space2))
			.OnClicked(InboxClick([this, Id]() { Select(Id); }))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, Space3, 0.0f)
				[
					SNew(SBox).WidthOverride(4.0f)[SNew(SBorder).BorderImage(FocusBrush()).BorderBackgroundColor(Cat)]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						InboxText((M.bRead ? FString() : FString(TEXT("●  "))) + M.FromName, 13, M.bRead ? TextMuted : TextPrimary, EWeight::SemiBold)
					]
					+ SVerticalBox::Slot().AutoHeight()[InboxText(M.Subject, 14, TextPrimary, M.bRead ? EWeight::Regular : EWeight::Medium)]
					+ SVerticalBox::Slot().AutoHeight()[InboxText(Meta, 11, M.bNeedsReply ? Warning : TextMuted, EWeight::Medium)]
				]
			]
		];
	}
	return Box;
}

TSharedRef<SWidget> SRbInbox::BuildReader()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	const TArray<FRbInboxMessageView> Inbox = Campaign.IsValid() ? Campaign->GetInbox() : TArray<FRbInboxMessageView>();
	const FRbInboxMessageView* M = Inbox.FindByPredicate([this](const FRbInboxMessageView& V) { return V.Id == Selected; });
	if (!M)
	{
		Box->AddSlot().AutoHeight()[InboxText(TEXT("Select a message."), 14, TextMuted)];
		return Box;
	}
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space1)[InboxText(M->Category.ToUpper(), 11, CategoryColor(M->Category), EWeight::Bold)];
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space1)[InboxText(M->Subject, 20, TextPrimary, EWeight::SemiBold)];
	const FString From = M->FromRole.IsEmpty() ? M->FromName : FString::Printf(TEXT("%s, %s"), *M->FromName, *M->FromRole);
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space4)[InboxText(FString::Printf(TEXT("From %s  |  week %d"), *From, M->Week), 13, TextMuted)];
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space4)[InboxText(M->Body, 15)];
	if (M->bNeedsReply)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, Space2, 0.0f, Space2)[InboxText(DeadlineText(M->WeeksLeft) + TEXT(". If you do not reply, events will decide for you."), 13, Warning, EWeight::Medium)];
		bool bFirst = true;
		for (const FRbMissionOptionView& C : M->Choices)
		{
			const FString ChoiceId = C.Id;
			TSharedPtr<SButton> Button;
			Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)
			[
				MakeButton(FText::FromString(C.Label), InboxClick([this, ChoiceId]() { Reply(ChoiceId); }), &Button)
			];
			if (bFirst) FirstFocus = Button;
			bFirst = false;
		}
	}
	else if (!M->Outcome.IsEmpty())
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, Space2, 0.0f, Space1)
		[
			InboxText(M->bAutoResolved ? FString(TEXT("You did not reply in time")) : FString::Printf(TEXT("You replied: %s"), *M->ChosenLabel), 13, M->bAutoResolved ? Warning : Accent, EWeight::SemiBold)
		];
		Box->AddSlot().AutoHeight()[InboxText(M->Outcome, 14, TextMuted)];
	}
	return Box;
}

#undef LOCTEXT_NAMESPACE
