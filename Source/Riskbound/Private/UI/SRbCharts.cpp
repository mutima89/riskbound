#include "SRbCharts.h"

#include "Brushes/SlateColorBrush.h"
#include "RbUIStyle.h"
#include "Rendering/DrawElements.h"

namespace
{
const FSlateBrush* Solid()
{
	static const FSlateColorBrush Brush(FLinearColor::White);
	return &Brush;
}

void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, float X, float Y, float W, float H, const FLinearColor& C)
{
	if (W <= 0.0f || H <= 0.0f) return;
	FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))), Solid(), ESlateDrawEffect::None, C);
}
} // namespace

void SRbSparkline::Construct(const FArguments& InArgs)
{
	Values = InArgs._Values;
	Amber = InArgs._Amber;
	Red = InArgs._Red;
	Size = InArgs._Size;
}

int32 SRbSparkline::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Culling, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2f S = FVector2f(G.GetLocalSize());
	Box(Out, LayerId, G, 0, 0, S.X, S.Y, FLinearColor(1, 1, 1, 0.03f));
	if (Values.Num() < 2) return LayerId + 1;
	double Lo = FMath::Min(Amber, Red);
	double Hi = FMath::Max(Amber, Red);
	for (double V : Values)
	{
		Lo = FMath::Min(Lo, V);
		Hi = FMath::Max(Hi, V);
	}
	const double Pad = FMath::Max(1e-6, (Hi - Lo) * 0.1);
	Lo -= Pad;
	Hi += Pad;
	auto Y = [&](double V) { return static_cast<float>(S.Y - (V - Lo) / (Hi - Lo) * S.Y); };
	for (const TPair<double, FLinearColor>& T : {TPair<double, FLinearColor>(Amber, RbUI::Warning), TPair<double, FLinearColor>(Red, RbUI::Bad)})
	{
		FSlateDrawElement::MakeLines(Out, LayerId + 1, G.ToPaintGeometry(), TArray<FVector2f>{FVector2f(0, Y(T.Key)), FVector2f(S.X, Y(T.Key))},
			ESlateDrawEffect::None, T.Value * FLinearColor(1, 1, 1, 0.55f), true, 1.0f);
	}
	TArray<FVector2f> Points;
	for (int32 i = 0; i < Values.Num(); ++i)
	{
		Points.Add(FVector2f(S.X * i / (Values.Num() - 1), Y(Values[i])));
	}
	FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, RbUI::Accent, true, 2.0f);
	return LayerId + 3;
}

void SRbBarSeries::Construct(const FArguments& InArgs)
{
	Counts = InArgs._Counts;
	HighlightFrom = InArgs._HighlightFrom;
	Size = InArgs._Size;
}

int32 SRbBarSeries::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Culling, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2f S = FVector2f(G.GetLocalSize());
	Box(Out, LayerId, G, 0, 0, S.X, S.Y, FLinearColor(1, 1, 1, 0.03f));
	int32 Max = 1;
	for (int32 C : Counts) Max = FMath::Max(Max, C);
	const float W = Counts.Num() ? S.X / Counts.Num() : S.X;
	for (int32 i = 0; i < Counts.Num(); ++i)
	{
		const float H = S.Y * Counts[i] / static_cast<float>(Max);
		const bool bHot = HighlightFrom >= 0 && i >= HighlightFrom;
		Box(Out, LayerId + 1, G, i * W + 1.0f, S.Y - H, W - 2.0f, H, bHot ? RbUI::Warning : RbUI::Accent);
	}
	return LayerId + 2;
}

void SRbTornadoBar::Construct(const FArguments& InArgs)
{
	Low = InArgs._Low;
	High = InArgs._High;
	Base = InArgs._Base;
	Extent = FMath::Max(1e-9, InArgs._Extent);
	Size = InArgs._Size;
}

int32 SRbTornadoBar::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Culling, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2f S = FVector2f(G.GetLocalSize());
	const float Mid = S.X * 0.5f;
	auto X = [&](double V) { return static_cast<float>(Mid + (V - Base) / Extent * Mid); };
	Box(Out, LayerId, G, Mid - 0.5f, 0, 1.0f, S.Y, FLinearColor(1, 1, 1, 0.3f));
	const float A = X(FMath::Min(Low, High));
	const float B = X(FMath::Max(Low, High));
	Box(Out, LayerId + 1, G, A, 2.0f, FMath::Max(1.0f, B - A), S.Y - 4.0f, RbUI::Accent);
	return LayerId + 2;
}
