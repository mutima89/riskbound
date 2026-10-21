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
	void ClearNotices();

private:
	void RebuildNotices();
	float NoticeAlpha(const FString& Message) const; // fades out during the last 0.6 s

	struct FNotice
	{
		FString Text;
		double ExpiresAt = 0.0;
	};
	TArray<FNotice> Notices;
	TSharedPtr<SVerticalBox> NoticeBox;        // top right, during gameplay
	TSharedPtr<SVerticalBox> CompactNoticeBox; // top centre, in the band modal screens leave free
	bool bCompact = false;
	TAttribute<bool> ShowGameplay;
};
