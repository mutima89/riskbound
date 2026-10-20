// Shared look for the code-built Slate UI: fonts honour the player's text-size setting.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Reply.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SWidget.h"

class SButton;

namespace RbUI
{
void SetTextScale(float Scale);
float GetTextScale();

FSlateFontInfo Font(int32 Size, bool bBold = false);

const FSlateBrush* PanelBrush();     // dark translucent panel
const FSlateBrush* BackdropBrush();  // full-screen dim behind modal screens
const FSlateBrush* CardBrush();      // slightly lighter section card

extern const FLinearColor Accent;
extern const FLinearColor TextPrimary;
extern const FLinearColor TextMuted;
extern const FLinearColor Good;
extern const FLinearColor Bad;

// Focusable text button. OutButton (optional) receives the widget for initial keyboard focus.
TSharedRef<SWidget> MakeButton(const FText& Label, FOnClicked OnClicked, TSharedPtr<SButton>* OutButton = nullptr,
	TAttribute<bool> IsEnabled = true);

TSharedRef<SWidget> MakeHeading(const FText& Text, int32 Size = 22);
TSharedRef<SWidget> MakeBody(const FText& Text, int32 Size = 14, const FLinearColor& Color = TextPrimary);

// Centred modal frame with a dim backdrop, used by every full screen.
TSharedRef<SWidget> MakeModalFrame(TSharedRef<SWidget> Content, float MaxWidth = 900.0f);

FText Money(double Amount);
} // namespace RbUI
