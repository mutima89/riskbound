// The inbox: messages from colleagues, incidents, the regulator, home and the newswire. Some need a
// reply before a deadline (in organisation weeks); if the player lets one lapse, its default outcome
// applies (rules in rb::UpdateWorld). This widget only displays views and forwards replies.
#pragma once

#include "CoreMinimal.h"
#include "RbCampaignTypes.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SWidget;
class URbCampaignSubsystem;

class SRbInbox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRbInbox) {}
		SLATE_ARGUMENT(TWeakObjectPtr<URbCampaignSubsystem>, Campaign)
		SLATE_ARGUMENT(TFunction<void()>, OnClose)
		SLATE_ARGUMENT(FString, OpenMessageId)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	TSharedPtr<SWidget> GetInitialFocus() const { return FirstFocus; }

	void Select(const FString& MessageId);
	bool Reply(const FString& ChoiceId); // used by the buttons and the self-test

private:
	void Rebuild();
	TSharedRef<SWidget> BuildList();
	TSharedRef<SWidget> BuildReader();

	TWeakObjectPtr<URbCampaignSubsystem> Campaign;
	TFunction<void()> OnClose;
	TSharedPtr<SBox> ListBox;
	TSharedPtr<SBox> ReaderBox;
	TSharedPtr<SWidget> FirstFocus;
	FString Selected;
	FText Status;
};
