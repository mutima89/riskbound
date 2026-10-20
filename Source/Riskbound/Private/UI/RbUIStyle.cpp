#include "RbUIStyle.h"

#include "Brushes/SlateColorBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace RbUI
{
static float GTextScale = 1.0f;

const FLinearColor Accent(0.20f, 0.62f, 0.86f);
const FLinearColor TextPrimary(0.93f, 0.94f, 0.96f);
const FLinearColor TextMuted(0.62f, 0.66f, 0.72f);
const FLinearColor Good(0.35f, 0.80f, 0.45f);
const FLinearColor Bad(0.92f, 0.38f, 0.33f);

void SetTextScale(float Scale) { GTextScale = FMath::Clamp(Scale, 0.8f, 1.6f); }
float GetTextScale() { return GTextScale; }

FSlateFontInfo Font(int32 Size, bool bBold)
{
	return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FMath::RoundToInt(Size * GTextScale));
}

const FSlateBrush* PanelBrush()
{
	static const FSlateColorBrush Brush(FLinearColor(0.025f, 0.03f, 0.045f, 0.96f));
	return &Brush;
}

const FSlateBrush* BackdropBrush()
{
	static const FSlateColorBrush Brush(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	return &Brush;
}

const FSlateBrush* CardBrush()
{
	static const FSlateColorBrush Brush(FLinearColor(0.07f, 0.08f, 0.11f, 1.0f));
	return &Brush;
}

TSharedRef<SWidget> MakeButton(const FText& Label, FOnClicked OnClicked, TSharedPtr<SButton>* OutButton, TAttribute<bool> IsEnabled)
{
	TSharedRef<SButton> Button = SNew(SButton)
		.OnClicked(OnClicked)
		.IsEnabled(IsEnabled)
		.IsFocusable(true)
		.ContentPadding(FMargin(14.0f, 8.0f))
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(Font(15, true))
		];
	if (OutButton)
	{
		*OutButton = Button;
	}
	return Button;
}

TSharedRef<SWidget> MakeHeading(const FText& Text, int32 Size)
{
	return SNew(STextBlock).Text(Text).Font(Font(Size, true)).ColorAndOpacity(TextPrimary).AutoWrapText(true);
}

TSharedRef<SWidget> MakeBody(const FText& Text, int32 Size, const FLinearColor& Color)
{
	return SNew(STextBlock).Text(Text).Font(Font(Size)).ColorAndOpacity(Color).AutoWrapText(true);
}

TSharedRef<SWidget> MakeModalFrame(TSharedRef<SWidget> Content, float MaxWidth)
{
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage).Image(BackdropBrush())
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(40.0f)
		[
			SNew(SBox)
			.MaxDesiredWidth(MaxWidth)
			.MaxDesiredHeight(980.0f)
			[
				SNew(SBorder)
				.BorderImage(PanelBrush())
				.Padding(28.0f)
				[
					Content
				]
			]
		];
}

FText Money(double Amount)
{
	FNumberFormattingOptions Options;
	Options.MaximumFractionalDigits = 0;
	Options.UseGrouping = true;
	return FText::AsNumber(Amount, &Options);
}
} // namespace RbUI
