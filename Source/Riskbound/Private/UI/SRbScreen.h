// Root of every modal screen: catches Escape (and gamepad back) to run the screen's back action.
#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "RbUIStyle.h"
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
		// 150 ms fade-in, skipped with Reduced Motion (Docs/ArtDirection.md "UI rules").
		StartTime = FPlatformTime::Seconds();
		SetRenderOpacity(RbUI::IsReducedMotion() ? 1.0f : 0.0f);
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		if (GetRenderOpacity() < 1.0f)
		{
			const float T = static_cast<float>((FPlatformTime::Seconds() - StartTime) / 0.15);
			SetRenderOpacity(RbUI::IsReducedMotion() ? 1.0f : FMath::Clamp(FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(T, 0.0f, 1.0f), 2.0f), 0.0f, 1.0f));
		}
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		// Clicking the backdrop keeps keyboard focus inside the screen so Escape still works.
		return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
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
	double StartTime = 0.0;
};
