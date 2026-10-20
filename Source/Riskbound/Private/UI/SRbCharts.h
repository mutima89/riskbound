// Small painted charts for the tool hub: KRI sparkline with thresholds, bar series (histogram)
// and a signed bar row (tornado). Display only; values come from URbToolsSubsystem.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

// Line of values, oldest first, with amber/red threshold guides.
class SRbSparkline : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRbSparkline) : _Amber(0.0), _Red(0.0), _Size(FVector2D(220.0, 44.0)) {}
		SLATE_ARGUMENT(TArray<double>, Values)
		SLATE_ARGUMENT(double, Amber)
		SLATE_ARGUMENT(double, Red)
		SLATE_ARGUMENT(FVector2D, Size)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override { return Size; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out,
		int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	TArray<double> Values;
	double Amber = 0.0;
	double Red = 0.0;
	FVector2D Size;
};

// Vertical bars (histogram); the bar at HighlightFrom and beyond is drawn in the warning colour.
class SRbBarSeries : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRbBarSeries) : _HighlightFrom(-1), _Size(FVector2D(600.0, 160.0)) {}
		SLATE_ARGUMENT(TArray<int32>, Counts)
		SLATE_ARGUMENT(int32, HighlightFrom)
		SLATE_ARGUMENT(FVector2D, Size)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override { return Size; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out,
		int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	TArray<int32> Counts;
	int32 HighlightFrom = -1;
	FVector2D Size;
};

// One tornado row: a bar from Low to High around Base on a shared scale [-Extent, +Extent].
class SRbTornadoBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRbTornadoBar) : _Low(0.0), _High(0.0), _Base(0.0), _Extent(1.0), _Size(FVector2D(360.0, 18.0)) {}
		SLATE_ARGUMENT(double, Low)
		SLATE_ARGUMENT(double, High)
		SLATE_ARGUMENT(double, Base)
		SLATE_ARGUMENT(double, Extent)
		SLATE_ARGUMENT(FVector2D, Size)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override { return Size; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out,
		int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	double Low = 0.0;
	double High = 0.0;
	double Base = 0.0;
	double Extent = 1.0;
	FVector2D Size;
};
