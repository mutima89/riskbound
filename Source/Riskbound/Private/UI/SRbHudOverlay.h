// Always-on HUD: current objective, crosshair, interaction prompt and short notices.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

class SRbHudOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRbHudOverlay) {}
		SLATE_ATTRIBUTE(FText, Objective)
		SLATE_ATTRIBUTE(FText, Prompt)
		SLATE_ATTRIBUTE(bool, ShowGameplay) // objective, crosshair and prompt
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void AddNotice(const FString& Message);

private:
	void RebuildNotices();

	struct FNotice
	{
		FString Text;
		double ExpiresAt = 0.0;
	};
	TArray<FNotice> Notices;
	TSharedPtr<SVerticalBox> NoticeBox;
	TAttribute<bool> ShowGameplay;
};
