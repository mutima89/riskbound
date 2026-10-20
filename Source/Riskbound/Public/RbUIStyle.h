// The one style module for the code-built Slate UI (Docs/ArtDirection.md "UI rules"):
// Inter typeface, colour tokens, panel and button styles, focus outline, motion.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Reply.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SWidget.h"

class SButton;

namespace RbUI
{
// ---- Settings that change the look ----
void SetTextScale(float Scale);
float GetTextScale();
void SetReducedMotion(bool bReduced);
bool IsReducedMotion();

// ---- Typography: Inter (Resources/Fonts, SIL OFL), Roboto fallback ----
enum class EWeight : uint8 { Regular, Medium, SemiBold, Bold };
FSlateFontInfo Font(int32 Size, bool bBold = false);
FSlateFontInfo FontW(int32 Size, EWeight Weight);

// ---- Colour tokens ----
extern const FLinearColor Accent;      // Valtren teal #2BA3A8
extern const FLinearColor TextPrimary; // #ECEEF0
extern const FLinearColor TextMuted;   // #9AA3AD
extern const FLinearColor Good;
extern const FLinearColor Warning;     // amber #E3A33B, risk states only
extern const FLinearColor Bad;         // red #D9534F, risk states only

// ---- Spacing (4 px grid) ----
constexpr float Space1 = 4.0f;
constexpr float Space2 = 8.0f;
constexpr float Space3 = 12.0f;
constexpr float Space4 = 16.0f;
constexpr float Space6 = 24.0f;

// ---- Surfaces ----
const FSlateBrush* PanelBrush();     // dark glass panel with hairline border, rounded
const FSlateBrush* BackdropBrush();  // full-screen dim behind modal screens
const FSlateBrush* CardBrush();      // section card inside a panel
const FSlateBrush* FocusBrush();     // teal outline drawn around the focused control

// 20 px check boxes and radio buttons: clear outline when off, teal fill when on.
const struct FCheckBoxStyle& CheckStyle();
const struct FCheckBoxStyle& RadioStyle();

// Focusable text button. bPrimary uses the accent fill. OutButton (optional) receives the widget for initial focus.
TSharedRef<SWidget> MakeButton(const FText& Label, FOnClicked OnClicked, TSharedPtr<SButton>* OutButton = nullptr,
	TAttribute<bool> IsEnabled = true, bool bPrimary = false);

TSharedRef<SWidget> MakeHeading(const FText& Text, int32 Size = 22);
TSharedRef<SWidget> MakeBody(const FText& Text, int32 Size = 14, const FLinearColor& Color = TextPrimary);
TSharedRef<SWidget> MakeEyebrow(const FText& Text); // small uppercase teal label above a heading

// Centred modal frame with a dim backdrop, used by every full screen.
TSharedRef<SWidget> MakeModalFrame(TSharedRef<SWidget> Content, float MaxWidth = 900.0f);

FText Money(double Amount);
} // namespace RbUI
