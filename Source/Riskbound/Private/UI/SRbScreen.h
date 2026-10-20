// Root of every modal screen: catches Escape (and gamepad back) to run the screen's back action.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SRbScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRbScreen) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ARGUMENT(TFunction<void()>, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnBack = InArgs._OnBack;
		ChildSlot[InArgs._Content.Widget];
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		const FKey Key = InKeyEvent.GetKey();
		if ((Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right) && OnBack)
		{
			OnBack();
			return FReply::Handled();
		}
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

private:
	TFunction<void()> OnBack;
};
