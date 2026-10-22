#include "RbUIStyle.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/CompositeFont.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace RbUI
{
static float GTextScale = 1.0f;
static bool GReducedMotion = false;

const FLinearColor Accent = FLinearColor::FromSRGBColor(FColor(0x2B, 0xA3, 0xA8));
const FLinearColor TextPrimary = FLinearColor::FromSRGBColor(FColor(0xEC, 0xEE, 0xF0));
const FLinearColor TextMuted = FLinearColor::FromSRGBColor(FColor(0x9A, 0xA3, 0xAD));
FLinearColor Good = FLinearColor::FromSRGBColor(FColor(0x5C, 0xC4, 0x8A));
FLinearColor Warning = FLinearColor::FromSRGBColor(FColor(0xE3, 0xA3, 0x3B));
FLinearColor Bad = FLinearColor::FromSRGBColor(FColor(0xD9, 0x53, 0x4F));

void SetColourSafe(bool bSafe)
{
	// Okabe-Ito blue / vermillion / yellow when safe; the house green / red / amber otherwise.
	Good = FLinearColor::FromSRGBColor(bSafe ? FColor(0x56, 0xB4, 0xE9) : FColor(0x5C, 0xC4, 0x8A));
	Bad = FLinearColor::FromSRGBColor(bSafe ? FColor(0xE6, 0x9F, 0x00) : FColor(0xD9, 0x53, 0x4F));
	Warning = FLinearColor::FromSRGBColor(bSafe ? FColor(0xF0, 0xE4, 0x42) : FColor(0xE3, 0xA3, 0x3B));
}

static const FLinearColor PanelFill = FLinearColor::FromSRGBColor(FColor(0x0E, 0x11, 0x16, 235));
static const FLinearColor Hairline(1.0f, 1.0f, 1.0f, 0.08f);

void SetTextScale(float Scale) { GTextScale = FMath::Clamp(Scale, 0.8f, 1.6f); }
float GetTextScale() { return GTextScale; }
void SetReducedMotion(bool bReduced) { GReducedMotion = bReduced; }
bool IsReducedMotion() { return GReducedMotion; }

// Inter is loaded from loose TTF files staged with the game (Riskbound.Build.cs RuntimeDependencies).
static TSharedPtr<const FCompositeFont> InterFont()
{
	static TSharedPtr<FStandaloneCompositeFont> Font;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		const FString Dir = FPaths::ProjectDir() / TEXT("Resources/Fonts/");
		if (IFileManager::Get().FileExists(*(Dir + TEXT("Inter-Regular.ttf"))))
		{
			Font = MakeShared<FStandaloneCompositeFont>();
			for (const TCHAR* Face : {TEXT("Regular"), TEXT("Medium"), TEXT("SemiBold"), TEXT("Bold")})
			{
				Font->DefaultTypeface.AppendFont(Face, Dir + TEXT("Inter-") + Face + TEXT(".ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			}
		}
	}
	return Font;
}

FSlateFontInfo FontW(int32 Size, EWeight Weight)
{
	const int32 Scaled = FMath::RoundToInt(Size * GTextScale);
	static const FName Faces[] = {TEXT("Regular"), TEXT("Medium"), TEXT("SemiBold"), TEXT("Bold")};
	if (TSharedPtr<const FCompositeFont> Inter = InterFont())
	{
		return FSlateFontInfo(Inter, Scaled, Faces[static_cast<int32>(Weight)]);
	}
	return FCoreStyle::GetDefaultFontStyle(Weight >= EWeight::SemiBold ? "Bold" : "Regular", Scaled);
}

FSlateFontInfo Font(int32 Size, bool bBold)
{
	return FontW(Size, bBold ? EWeight::SemiBold : EWeight::Regular);
}

const FSlateBrush* PanelBrush()
{
	static const FSlateRoundedBoxBrush Brush(PanelFill, 8.0f, Hairline, 1.0f);
	return &Brush;
}

const FSlateBrush* BackdropBrush()
{
	static const FSlateColorBrush Brush(FLinearColor(0.0f, 0.005f, 0.01f, 0.6f));
	return &Brush;
}

const FSlateBrush* CardBrush()
{
	static const FSlateRoundedBoxBrush Brush(FLinearColor::FromSRGBColor(FColor(0x16, 0x1B, 0x23)), 6.0f, Hairline, 1.0f);
	return &Brush;
}

const FSlateBrush* FocusBrush()
{
	static const FSlateRoundedBoxBrush Brush(FLinearColor::Transparent, 7.0f, Accent, 2.0f);
	return &Brush;
}

const FSlateBrush* VignetteBrush()
{
	// White with alpha rising towards the edges (smoothstep from 55% to 100% of the half-diagonal);
	// a transient texture kept alive for the session.
	static FSlateBrush Brush;
	static bool bInit = false;
	if (!bInit)
	{
		bInit = true;
		constexpr int32 N = 256;
		UTexture2D* Tex = UTexture2D::CreateTransient(N, N, PF_B8G8R8A8);
		if (Tex)
		{
			Tex->AddToRoot();
			Tex->SRGB = false;
			FColor* Px = static_cast<FColor*>(Tex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
			for (int32 Y = 0; Y < N; ++Y)
			{
				for (int32 X = 0; X < N; ++X)
				{
					const float Dx = (X + 0.5f) / N * 2.0f - 1.0f, Dy = (Y + 0.5f) / N * 2.0f - 1.0f;
					const float D = FMath::Sqrt(Dx * Dx + Dy * Dy) / 1.4142f;
					const float T = FMath::Clamp((D - 0.55f) / 0.45f, 0.0f, 1.0f);
					Px[Y * N + X] = FColor(255, 255, 255, static_cast<uint8>(255.0f * T * T * (3.0f - 2.0f * T)));
				}
			}
			Tex->GetPlatformData()->Mips[0].BulkData.Unlock();
			Tex->UpdateResource();
			Brush.SetResourceObject(Tex);
			Brush.ImageSize = FVector2D(N, N);
			Brush.DrawAs = ESlateBrushDrawType::Image;
		}
	}
	return &Brush;
}

static FCheckBoxStyle MakeToggleStyle(float Radius)
{
	const FVector2f Size(20.0f, 20.0f);
	const FLinearColor Outline = TextMuted;
	const FSlateRoundedBoxBrush Off(FLinearColor(0, 0, 0, 0.25f), Radius, Outline, 1.5f, Size);
	const FSlateRoundedBoxBrush OffHover(FLinearColor(1, 1, 1, 0.06f), Radius, Accent, 1.5f, Size);
	const FSlateRoundedBoxBrush On(Accent, Radius, FLinearColor::White * FLinearColor(1, 1, 1, 0.85f), 2.0f, Size);
	const FSlateRoundedBoxBrush OnHover(Accent, Radius, FLinearColor::White, 2.0f, Size);
	FCheckBoxStyle S;
	S.SetCheckBoxType(ESlateCheckBoxType::CheckBox)
		.SetUncheckedImage(Off).SetUncheckedHoveredImage(OffHover).SetUncheckedPressedImage(OffHover)
		.SetCheckedImage(On).SetCheckedHoveredImage(OnHover).SetCheckedPressedImage(OnHover)
		.SetUndeterminedImage(Off).SetUndeterminedHoveredImage(OffHover).SetUndeterminedPressedImage(OffHover)
		.SetPadding(FMargin(Space2, 0.0f, 0.0f, 0.0f));
	return S;
}

const FCheckBoxStyle& CheckStyle()
{
	static const FCheckBoxStyle S = MakeToggleStyle(4.0f);
	return S;
}

const FCheckBoxStyle& RadioStyle()
{
	static const FCheckBoxStyle S = MakeToggleStyle(10.0f);
	return S;
}

static FButtonStyle& ButtonStyle(bool bPrimary)
{
	static FButtonStyle Normal;
	static FButtonStyle Primary;
	static bool bInit = false;
	if (!bInit)
	{
		bInit = true;
		const FLinearColor Base = FLinearColor::FromSRGBColor(FColor(0x1B, 0x21, 0x2A));
		const FLinearColor Hover = FLinearColor::FromSRGBColor(FColor(0x25, 0x2D, 0x39));
		Normal.SetNormal(FSlateRoundedBoxBrush(Base, 6.0f, Hairline, 1.0f))
			.SetHovered(FSlateRoundedBoxBrush(Hover, 6.0f, Accent * FLinearColor(1, 1, 1, 0.6f), 1.0f))
			.SetPressed(FSlateRoundedBoxBrush(Accent * FLinearColor(0.5f, 0.5f, 0.5f, 1.0f), 6.0f, Accent, 1.0f))
			.SetDisabled(FSlateRoundedBoxBrush(Base * FLinearColor(1, 1, 1, 0.5f), 6.0f, Hairline, 1.0f))
			.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
		Primary = Normal;
		Primary.SetNormal(FSlateRoundedBoxBrush(Accent * FLinearColor(0.75f, 0.75f, 0.75f, 1.0f), 6.0f, Accent, 1.0f))
			.SetHovered(FSlateRoundedBoxBrush(Accent, 6.0f, FLinearColor::White * FLinearColor(1, 1, 1, 0.5f), 1.0f));
	}
	return bPrimary ? Primary : Normal;
}

void SetClickSound(UObject* Sound)
{
	FSlateSound Click;
	Click.SetResourceObject(Sound);
	ButtonStyle(false).SetPressedSound(Click);
	ButtonStyle(true).SetPressedSound(Click);
}

TSharedRef<SWidget> MakeButton(const FText& Label, FOnClicked OnClicked, TSharedPtr<SButton>* OutButton, TAttribute<bool> IsEnabled, bool bPrimary)
{
	TSharedPtr<SButton> Button;
	TSharedRef<SWidget> Root = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SAssignNew(Button, SButton)
			.ButtonStyle(&ButtonStyle(bPrimary))
			.OnClicked(OnClicked)
			.IsEnabled(IsEnabled)
			.IsFocusable(true)
			.ContentPadding(FMargin(Space4, Space2 + 2.0f))
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FontW(15, EWeight::Medium))
				.ColorAndOpacity(TextPrimary)
			]
		]
		// Keyboard focus is always visible: a teal outline around the focused button.
		+ SOverlay::Slot()
		.Padding(-3.0f)
		[
			SNew(SImage)
			.Image(FocusBrush())
			.Visibility_Lambda([WeakButton = TWeakPtr<SButton>(Button)]()
			{
				const TSharedPtr<SButton> B = WeakButton.Pin();
				return B.IsValid() && B->HasKeyboardFocus() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
		];
	if (OutButton)
	{
		*OutButton = Button;
	}
	return Root;
}

TSharedRef<SWidget> MakeHeading(const FText& Text, int32 Size)
{
	return SNew(STextBlock).Text(Text).Font(FontW(Size, EWeight::SemiBold)).ColorAndOpacity(TextPrimary).AutoWrapText(true);
}

TSharedRef<SWidget> MakeBody(const FText& Text, int32 Size, const FLinearColor& Color)
{
	return SNew(STextBlock).Text(Text).Font(FontW(Size, EWeight::Regular)).ColorAndOpacity(Color).AutoWrapText(true).LineHeightPercentage(1.15f);
}

TSharedRef<SWidget> MakeEyebrow(const FText& Text)
{
	return SNew(STextBlock).Text(Text.ToUpper()).Font(FontW(11, EWeight::SemiBold)).ColorAndOpacity(Accent);
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
		.Padding(FMargin(40.0f, ModalTopBand, 40.0f, 24.0f)) // the top band holds notices (SRbHudOverlay)
		[
			SNew(SBox)
			.MaxDesiredWidth(MaxWidth)
			.MaxDesiredHeight(980.0f)
			[
				SNew(SBorder)
				.BorderImage(PanelBrush())
				.Padding(FMargin(Space6 + 4.0f))
				[
					Content
				]
			]
		];
}

TSharedRef<SWidget> MakeConversationFrame(TSharedRef<SWidget> Content, float MaxWidth)
{
	return SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(40.0f, ModalTopBand, 40.0f, 36.0f))
		[
			SNew(SBox)
			.MinDesiredWidth(MaxWidth) // a fixed width, so wrapped text never settles a few pixels short of its lines
			.MaxDesiredWidth(MaxWidth)
			.MaxDesiredHeight(500.0f)
			[
				SNew(SBorder)
				.BorderImage(PanelBrush())
				.Padding(FMargin(Space6))
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						Content
					]
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
