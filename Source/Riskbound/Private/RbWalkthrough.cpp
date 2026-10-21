#include "RbWalkthrough.h"
#include "EngineUtils.h"

#include "Camera/PlayerCameraManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RbCampaignSubsystem.h"
#include "RbConvert.h"
#include "RbGameSubsystem.h"
#include "RbHUD.h"
#include "RbInteractable.h"
#include "RbInteractionComponent.h"
#include "RbLog.h"
#include "RbMission001.h"
#include "RbUIStyle.h"
#include "UnrealClient.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#include <algorithm>

namespace RbWalkthrough
{
namespace
{
using RbConvert::ToFString;

// A path into the widget tree: each hop finds a text, then climbs to the nearest ancestor of a type;
// the target is then a text (Label) or a widget type inside the last container.
struct FHop
{
	FString Anchor;
	bool bExact = true;
	FName Container;
};

struct FLoc
{
	TArray<FHop> Hops;
	FString Label;      // text inside the clickable widget (empty: use WantType)
	bool bExact = true;
	FName WantType;     // e.g. SEditableTextBox, SSlider (when Label is empty)
	FString Describe() const
	{
		FString S;
		for (const FHop& H : Hops) S += H.Anchor + TEXT(" > ");
		return S + (Label.IsEmpty() ? WantType.ToString() : Label);
	}
};

FLoc L(const FString& Label, bool bExact = true) { FLoc X; X.Label = Label; X.bExact = bExact; return X; }
FLoc In(const FString& Anchor, FName Container, const FString& Label, bool bAnchorExact = false, bool bExact = true)
{
	FLoc X;
	X.Hops.Add({Anchor, bAnchorExact, Container});
	X.Label = Label;
	X.bExact = bExact;
	return X;
}

const FName NSBorder(TEXT("SBorder"));
const FName NSHBox(TEXT("SHorizontalBox"));

bool IsClickable(const TSharedRef<SWidget>& W)
{
	static const FName Types[] = {TEXT("SButton"), TEXT("SCheckBox"), TEXT("SEditableTextBox"), TEXT("SMultiLineEditableTextBox"), TEXT("SSlider")};
	for (const FName& T : Types)
	{
		if (W->GetType() == T) return true;
	}
	return false;
}

FString TextOf(const TSharedRef<SWidget>& W)
{
	return W->GetType() == TEXT("STextBlock") ? StaticCastSharedRef<STextBlock>(W)->GetText().ToString() : FString();
}

void Collect(const TSharedRef<SWidget>& Root, TArray<TSharedRef<SWidget>>& Out)
{
	if (!Root->GetVisibility().IsVisible()) return;
	Out.Add(Root);
	if (FChildren* C = Root->GetChildren())
	{
		for (int32 i = 0; i < C->Num(); ++i) Collect(C->GetChildAt(i), Out);
	}
}

bool Matches(const FString& Text, const FString& Want, bool bExact)
{
	return bExact ? Text.TrimStartAndEnd() == Want : Text.Contains(Want);
}

TSharedPtr<SWidget> Ancestor(TSharedPtr<SWidget> W, FName Type, int32 MaxUp = 12)
{
	for (int32 i = 0; W.IsValid() && i < MaxUp; ++i)
	{
		if (W->GetType() == Type) return W;
		W = W->GetParentWidget();
	}
	return nullptr;
}

TSharedPtr<SWidget> ClickableAncestor(TSharedPtr<SWidget> W)
{
	for (int32 i = 0; W.IsValid() && i < 6; ++i)
	{
		if (IsClickable(W.ToSharedRef())) return W;
		W = W->GetParentWidget();
	}
	return nullptr;
}

TSharedPtr<SWindow> GameWindow()
{
	return GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
}

TSharedPtr<SWidget> Locate(const FLoc& Loc)
{
	TSharedPtr<SWindow> Window = GameWindow();
	if (!Window.IsValid()) return nullptr;
	TSharedRef<SWidget> Root = Window.ToSharedRef();
	for (const FHop& H : Loc.Hops)
	{
		TArray<TSharedRef<SWidget>> All;
		Collect(Root, All);
		TSharedPtr<SWidget> Next;
		for (const TSharedRef<SWidget>& W : All)
		{
			if (W->GetType() == TEXT("STextBlock") && Matches(TextOf(W), H.Anchor, H.bExact))
			{
				Next = Ancestor(W, H.Container);
				if (Next.IsValid()) break;
			}
		}
		if (!Next.IsValid()) return nullptr;
		Root = Next.ToSharedRef();
	}
	TArray<TSharedRef<SWidget>> All;
	Collect(Root, All);
	for (const TSharedRef<SWidget>& W : All)
	{
		if (Loc.Label.IsEmpty())
		{
			if (W->GetType() == Loc.WantType) return W;
			continue;
		}
		if (W->GetType() == TEXT("STextBlock") && Matches(TextOf(W), Loc.Label, Loc.bExact))
		{
			TSharedPtr<SWidget> C = ClickableAncestor(W);
			if (C.IsValid() && C->IsEnabled()) return C;
		}
	}
	return nullptr;
}

FVector2D Centre(const TSharedPtr<SWidget>& W)
{
	return FVector2D(W->GetTickSpaceGeometry().GetAbsolutePositionAtCoordinates(FVector2f(0.5f, 0.5f)));
}

void MouseMove(const FVector2D& Pos)
{
	FSlateApplication& App = FSlateApplication::Get();
	FPointerEvent Move(App.GetUserIndexForMouse(), FSlateApplicationBase::CursorPointerIndex, Pos, Pos, TSet<FKey>(), EKeys::Invalid, 0.0f, FModifierKeysState());
	App.ProcessMouseMoveEvent(Move);
}

void MouseClick(const FVector2D& Pos)
{
	FSlateApplication& App = FSlateApplication::Get();
	MouseMove(Pos);
	FPointerEvent Down(App.GetUserIndexForMouse(), FSlateApplicationBase::CursorPointerIndex, Pos, Pos, TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
	App.ProcessMouseButtonDownEvent(nullptr, Down);
	FPointerEvent Up(App.GetUserIndexForMouse(), FSlateApplicationBase::CursorPointerIndex, Pos, Pos, TSet<FKey>(), EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
	App.ProcessMouseButtonUpEvent(Up);
}

void KeyDown(const FKey& Key)
{
	FKeyEvent E(Key, FModifierKeysState(), FSlateApplication::Get().GetUserIndexForKeyboard(), false, 0, 0);
	FSlateApplication::Get().ProcessKeyDownEvent(E);
}

void KeyUp(const FKey& Key)
{
	FKeyEvent E(Key, FModifierKeysState(), FSlateApplication::Get().GetUserIndexForKeyboard(), false, 0, 0);
	FSlateApplication::Get().ProcessKeyUpEvent(E);
}

void TypeChar(TCHAR C)
{
	FCharacterEvent E(C, FModifierKeysState(), FSlateApplication::Get().GetUserIndexForKeyboard(), false);
	FSlateApplication::Get().ProcessKeyCharEvent(E);
}

FString FileSafe(const FString& Name)
{
	FString Out;
	for (TCHAR C : Name) Out.AppendChar(FChar::IsAlnum(C) ? C : TEXT('_'));
	return Out;
}

// ---------------------------------------------------------------------------------------- driver

class FDriver : public TSharedFromThis<FDriver>
{
public:
	using FStepFn = TFunction<bool(FDriver&)>; // true when the step is finished
	struct FStep
	{
		FString Name;
		FStepFn Fn;
		double Timeout = 20.0;
		bool bCritical = true;
	};

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ARbHUD> Hud;
	int32 Phase = 1;
	TArray<FStep> Queue;
	TArray<FString> Log;
	TArray<FString> Findings;
	TSet<FString> FindingKeys;
	double Start = 0.0;
	double StepStart = 0.0;
	int32 StepFrame = 0;
	int32 Sub = 0;
	TSharedPtr<SWidget> Target;
	FVector2D LastPos = FVector2D::ZeroVector;
	double LastProgressTime = 0.0;
	bool bHoldingW = false;
	double PxPerDegYaw = 1.0;
	double PxPerDegPitch = 1.0; // signed
	double CalibYaw = 0.0, CalibPitch = 0.0;
	int32 Shots = 0;
	double WaitUntil = 0.0;
	double FocusLostSince = 0.0;
	bool bFinished = false;
	// Campaign policy state.
	TSet<FString> FailedOnPurpose;
	bool bCrisisTriggered = false;
	bool bCeremonySeen = false;
	int32 WeakDecisions = 0;
	TMap<FString, int32> AttemptsSeen;
	TSet<const SWidget*> AuditSuspects; // text that looked clipped on the first look

	double Now() const { return FPlatformTime::Seconds(); }
	URbGameSubsystem* Game() const { return Hud.IsValid() ? Hud->Game() : nullptr; }
	URbCampaignSubsystem* Campaign() const { return Hud.IsValid() ? Hud->GetGameInstance()->GetSubsystem<URbCampaignSubsystem>() : nullptr; }
	APlayerController* PC() const { return World.IsValid() ? World->GetFirstPlayerController() : nullptr; }

	void Note(const FString& Line)
	{
		const FString Stamped = FString::Printf(TEXT("[%7.1fs] %s"), Now() - Start, *Line);
		Log.Add(Stamped);
		UE_LOG(LogRiskbound, Display, TEXT("Walkthrough: %s"), *Line);
	}

	void Finding(const FString& Severity, const FString& Area, const FString& Text)
	{
		const FString Key = Area + TEXT("|") + Text;
		if (FindingKeys.Contains(Key)) return;
		FindingKeys.Add(Key);
		Findings.Add(FString::Printf(TEXT("%s | %s | %s"), *Severity, *Area, *Text));
		Note(FString::Printf(TEXT("FINDING [%s] %s: %s"), *Severity, *Area, *Text));
	}

	void Add(const FString& Name, FStepFn Fn, double Timeout = 20.0, bool bCritical = true) { Queue.Add({Name, MoveTemp(Fn), Timeout, bCritical}); }

	// Inserts steps right after the current one (used by planning steps).
	void InsertNext(TArray<FStep>&& Steps)
	{
		for (int32 i = Steps.Num() - 1; i >= 0; --i) Queue.Insert(MoveTemp(Steps[i]), 1);
	}

	void WriteLog()
	{
		FString Text = FString::Printf(TEXT("%s\nphase=%d\nduration_s=%.0f\nscreenshots=%d\n"), Findings.ContainsByPredicate([](const FString& F) { return F.StartsWith(TEXT("Blocker")); }) ? TEXT("ISSUES") : TEXT("DONE"),
			Phase, Now() - Start, Shots);
		Text += TEXT("\n# Findings (severity | area | text)\n");
		for (const FString& F : Findings) Text += F + TEXT("\n");
		Text += TEXT("\n# Log\n");
		for (const FString& S : Log) Text += S + TEXT("\n");
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("RbWalkthrough_%d.txt"), Phase));
		FFileHelper::SaveStringToFile(Text, *Path);
	}

	void Finish(const FString& Why)
	{
		if (bFinished) return;
		bFinished = true;
		if (bHoldingW) KeyUp(EKeys::W);
		Note(TEXT("finished: ") + Why);
		WriteLog();
	}

	bool Tick()
	{
		if (bFinished) return false;
		if (!Hud.IsValid() || !World.IsValid())
		{
			Finish(TEXT("world or HUD gone"));
			return false;
		}
		// Safety: inject input only while the game window is the active application.
		if (!FSlateApplication::Get().IsActive())
		{
			if (bHoldingW) { KeyUp(EKeys::W); bHoldingW = false; }
			if (FocusLostSince == 0.0)
			{
				FocusLostSince = Now();
				Note(TEXT("game window not active: input paused"));
			}
			if (Now() - FocusLostSince > 60.0)
			{
				Finding(TEXT("Blocker"), TEXT("Harness"), TEXT("Game window lost focus for 60 s; walkthrough stopped so no input reaches other windows."));
				Finish(TEXT("focus lost"));
				FPlatformMisc::RequestExit(false, TEXT("RbWalkthrough"));
				return false;
			}
			return true;
		}
		if (FocusLostSince != 0.0)
		{
			Note(TEXT("game window active again: resuming"));
			FocusLostSince = 0.0;
			StepStart = Now(); // do not count the pause against the step
		}
		if (Now() < WaitUntil) return true;
		if (Queue.IsEmpty())
		{
			Finish(TEXT("all steps done"));
			FPlatformMisc::RequestExit(false, TEXT("RbWalkthrough"));
			return false;
		}
		FStep& S = Queue[0];
		if (StepFrame == 0)
		{
			StepStart = Now();
			Sub = 0;
			Target.Reset();
		}
		const bool bDone = S.Fn(*this);
		++StepFrame;
		if (bDone)
		{
			Note(FString::Printf(TEXT("ok   %s (%.1fs)"), *S.Name, Now() - StepStart));
			Queue.RemoveAt(0);
			StepFrame = 0;
		}
		else if (Now() - StepStart > S.Timeout)
		{
			Finding(S.bCritical ? TEXT("Blocker") : TEXT("Medium"), TEXT("Walkthrough step"), FString::Printf(TEXT("'%s' did not complete in %.0f s"), *S.Name, S.Timeout));
			FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Walk_P%d_%02d_FAILED_%s"), Phase, Shots++, *FileSafe(S.Name)), true, false);
			if (bHoldingW) { KeyUp(EKeys::W); bHoldingW = false; }
			if (S.bCritical)
			{
				Finish(TEXT("critical step failed: ") + S.Name);
				FPlatformMisc::RequestExit(false, TEXT("RbWalkthrough"));
				return false;
			}
			Queue.RemoveAt(0);
			StepFrame = 0;
		}
		return true;
	}

	// ------------------------------------------------------------------ step factories

	void Wait(double Seconds)
	{
		Add(FString::Printf(TEXT("wait %.1fs"), Seconds), [Seconds](FDriver& D)
		{
			return D.Now() - D.StepStart >= Seconds;
		}, Seconds + 5.0);
	}

	void Click(const FLoc& Loc, bool bCritical = true, double Timeout = 10.0)
	{
		Add(TEXT("click ") + Loc.Describe(), [Loc](FDriver& D)
		{
			if (D.Sub == 0)
			{
				D.Target = Locate(Loc);
				if (!D.Target.IsValid()) return false;
				if (TSharedPtr<SWidget> Scroll = Ancestor(D.Target, TEXT("SScrollBox"), 40))
				{
					StaticCastSharedPtr<SScrollBox>(Scroll)->ScrollDescendantIntoView(D.Target, false, EDescendantScrollDestination::Center);
				}
				D.Sub = 1;
				D.StepFrame = 1;
				D.LastProgressTime = D.Now();
				return false;
			}
			if (D.Now() - D.LastProgressTime < 0.15) return false; // let layout settle after scrolling
			if (!D.Target.IsValid()) { D.Sub = 0; return false; }
			const FVector2D Pos = Centre(D.Target);
			const FVector2D Size = FVector2D(D.Target->GetTickSpaceGeometry().GetAbsoluteSize());
			if (Size.X < 2.0 || Size.Y < 2.0) { D.Sub = 0; return false; }
			MouseClick(Pos);
			D.WaitUntil = D.Now() + 0.25;
			return true;
		}, Timeout, bCritical);
	}

	void Type(const FString& Text)
	{
		Add(TEXT("type text"), [Text](FDriver& D)
		{
			for (TCHAR C : Text) TypeChar(C);
			D.WaitUntil = D.Now() + 0.2;
			return true;
		});
	}

	void Press(const FKey& Key, double After = 0.4)
	{
		Add(TEXT("press ") + Key.ToString(), [Key, After](FDriver& D)
		{
			if (D.Sub == 0)
			{
				KeyDown(Key);
				D.Sub = 1;
				return false;
			}
			KeyUp(Key);
			D.WaitUntil = D.Now() + After;
			return true;
		});
	}

	void Shot(const FString& Name)
	{
		Add(TEXT("screenshot ") + Name, [Name](FDriver& D)
		{
			if (D.Sub == 0)
			{
				// Move the pointer off the content so hover states do not change the picture.
				MouseMove(FVector2D(5, 5));
				D.Sub = 1;
				return false;
			}
			if (D.Now() - D.StepStart < 0.35) return false;
			FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Walk_P%d_%02d_%s"), D.Phase, D.Shots++, *FileSafe(Name)), true, false);
			D.WaitUntil = D.Now() + 0.4;
			return true;
		}, 5.0, false);
	}

	// Looks for text that does not fit its space on the current screen.
	void Audit(const FString& Screen)
	{
		Add(TEXT("audit ") + Screen, [Screen](FDriver& D)
		{
			TSharedPtr<SWindow> Window = GameWindow();
			if (!Window.IsValid()) return true;
			const bool bConfirm = D.Sub == 1;
			if (D.Sub == 0) { D.AuditSuspects.Reset(); }
			else if (D.Now() - D.StepStart < 0.4) return false;
			TArray<TSharedRef<SWidget>> All;
			Collect(Window.ToSharedRef(), All);
			const FVector2D WinPos = Window->GetPositionInScreen();
			const FVector2D WinSize = Window->GetSizeInScreen();
			for (const TSharedRef<SWidget>& W : All)
			{
				if (W->GetType() != TEXT("STextBlock")) continue;
				const FString Text = TextOf(W);
				if (Text.TrimStartAndEnd().IsEmpty()) continue;
				const FGeometry G = W->GetTickSpaceGeometry();
				const FVector2D Size = FVector2D(G.GetLocalSize());
				const FVector2D Pos = FVector2D(G.GetAbsolutePosition());
				if (Size.X < 1.0 || Size.Y < 1.0) continue;
				// Only judge what is inside the window (scrolled-out content is not truncation).
				if (Pos.X > WinPos.X + WinSize.X || Pos.Y > WinPos.Y + WinSize.Y || Pos.Y < WinPos.Y) continue;
				// Text partly outside a scroll box's viewport is scrolled, not truncated.
				if (TSharedPtr<SWidget> Scroll = Ancestor(W, TEXT("SScrollBox"), 40))
				{
					const FGeometry SG = Scroll->GetTickSpaceGeometry();
					const FVector2D SPos = FVector2D(SG.GetAbsolutePosition());
					const FVector2D SSize = FVector2D(SG.GetAbsoluteSize());
					const FVector2D TSize = FVector2D(G.GetAbsoluteSize());
					if (Pos.Y < SPos.Y || Pos.Y + FMath::Max(TSize.Y, W->GetDesiredSize().Y) > SPos.Y + SSize.Y) continue;
				}
				const FVector2D Want = W->GetDesiredSize();
				if (Want.X > Size.X + 6.0 || Want.Y > Size.Y + 2.0)
				{
					if (!bConfirm) { D.AuditSuspects.Add(&W.Get()); continue; }
					if (!D.AuditSuspects.Contains(&W.Get())) continue;
					D.Finding(TEXT("Medium"), TEXT("Text truncation (") + Screen + TEXT(")"),
						FString::Printf(TEXT("\"%s\" needs %.0fx%.0f, has %.0fx%.0f"), *Text.Left(80), Want.X, Want.Y, Size.X, Size.Y));
				}
			}
			if (!bConfirm && D.AuditSuspects.Num() > 0) { D.Sub = 1; return false; }
			return true;
		}, 5.0, false);
	}

	void Check(const FString& What, TFunction<bool(FDriver&, FString&)> Fn, const FString& Severity = TEXT("High"))
	{
		Add(TEXT("check ") + What, [What, Fn, Severity](FDriver& D)
		{
			FString Detail;
			const bool bOk = Fn(D, Detail);
			D.Note(FString::Printf(TEXT("%s %s: %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *What, *Detail));
			if (!bOk) D.Finding(Severity, TEXT("Check"), What + TEXT(": ") + Detail);
			return true;
		}, 5.0, false);
	}

	// ------------------------------------------------------------------ looking and walking

	static double Norm(double A) { return FRotator::NormalizeAxis(A); }

	void Calibrate()
	{
		Add(TEXT("calibrate mouse look"), [](FDriver& D)
		{
			APlayerController* P = D.PC();
			if (!P || !P->GetPawn()) return false;
			const FRotator R = P->GetControlRotation();
			switch (D.Sub)
			{
			case 0: D.CalibYaw = R.Yaw; D.CalibPitch = R.Pitch; FSlateApplication::Get().OnRawMouseMove(40, 0); D.Sub = 1; D.LastProgressTime = D.Now(); return false;
			case 1:
				if (D.Now() - D.LastProgressTime < 0.3) return false;
				{
					const double DY = Norm(R.Yaw - D.CalibYaw);
					if (FMath::Abs(DY) < 0.05) { D.Finding(TEXT("Blocker"), TEXT("Input"), TEXT("Raw mouse movement did not turn the camera.")); return true; }
					D.PxPerDegYaw = 40.0 / DY;
				}
				D.CalibPitch = R.Pitch;
				FSlateApplication::Get().OnRawMouseMove(0, 20);
				D.Sub = 2;
				D.LastProgressTime = D.Now();
				return false;
			default:
				if (D.Now() - D.LastProgressTime < 0.3) return false;
				{
					const double DP = Norm(R.Pitch - D.CalibPitch);
					D.PxPerDegPitch = FMath::Abs(DP) < 0.05 ? 1.0 : 20.0 / DP;
				}
				D.Note(FString::Printf(TEXT("look calibration: %.2f px/deg yaw, %.2f px/deg pitch"), D.PxPerDegYaw, D.PxPerDegPitch));
				return true;
			}
		}, 10.0);
	}

	// Turns towards a yaw/pitch with real mouse deltas. True when within tolerance.
	bool AimAt(double WantYaw, double WantPitch, double Tol)
	{
		const FRotator R = PC()->GetControlRotation();
		const double EY = Norm(WantYaw - R.Yaw);
		const double EP = Norm(WantPitch - R.Pitch);
		if (FMath::Abs(EY) < Tol && FMath::Abs(EP) < Tol) return true;
		const int32 DX = FMath::Clamp(FMath::RoundToInt(EY * PxPerDegYaw * 0.6), -60, 60);
		const int32 DY = FMath::Clamp(FMath::RoundToInt(EP * PxPerDegPitch * 0.6), -40, 40);
		FSlateApplication::Get().OnRawMouseMove(DX != 0 ? DX : (EY > 0 ? 1 : -1) * (FMath::Abs(EY) >= Tol ? 1 : 0), DY);
		return false;
	}

	// Walks through waypoints (2D, cm) holding W, then looks at a point and optionally presses E.
	void WalkTo(const FString& Name, TArray<FVector2D> Points, FVector LookAt, bool bInteract)
	{
		Add(TEXT("walk to ") + Name, [Name, Points, LookAt, bInteract](FDriver& D)
		{
			APlayerController* P = D.PC();
			APawn* Pawn = P ? P->GetPawn() : nullptr;
			if (!Pawn) return false;
			const FVector Loc = Pawn->GetActorLocation();
			const FVector2D Here(Loc.X, Loc.Y);
			if (D.StepFrame == 0) { D.LastPos = Here; D.LastProgressTime = D.Now(); }
			if (D.Sub < Points.Num())
			{
				const FVector2D To = Points[D.Sub] - Here;
				const double Dist = To.Size();
				const double Tol = D.Sub == Points.Num() - 1 ? 25.0 : 45.0;
				if (Dist <= Tol)
				{
					if (D.bHoldingW) { KeyUp(EKeys::W); D.bHoldingW = false; }
					++D.Sub;
					return false;
				}
				const double WantYaw = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
				const double Err = Norm(WantYaw - P->GetControlRotation().Yaw);
				D.AimAt(WantYaw, 0.0, 2.0);
				const bool bGo = FMath::Abs(Err) < 25.0;
				if (bGo && !D.bHoldingW) { KeyDown(EKeys::W); D.bHoldingW = true; }
				if (!bGo && D.bHoldingW) { KeyUp(EKeys::W); D.bHoldingW = false; }
				if ((Here - D.LastPos).Size() > 15.0) { D.LastPos = Here; D.LastProgressTime = D.Now(); }
				else if (D.Now() - D.LastProgressTime > 8.0)
				{
					// Harness fallback, reported: the walker could not get past furniture or a wall.
					D.Finding(TEXT("Low"), TEXT("Navigation"), FString::Printf(TEXT("Walker blocked near (%.0f, %.0f) on the way to %s; moved to the next waypoint"), Here.X, Here.Y, *Name));
					if (D.bHoldingW) { KeyUp(EKeys::W); D.bHoldingW = false; }
					Pawn->SetActorLocation(FVector(Points[D.Sub].X, Points[D.Sub].Y, Loc.Z), false, nullptr, ETeleportType::TeleportPhysics);
					D.LastProgressTime = D.Now();
				}
				return false;
			}
			if (D.bHoldingW) { KeyUp(EKeys::W); D.bHoldingW = false; }
			const FVector Cam = P->PlayerCameraManager ? P->PlayerCameraManager->GetCameraLocation() : Loc;
			const FVector V = LookAt - Cam;
			// Like a player nudging the view until the prompt appears: small offsets around the target.
			static const FVector2D Nudges[] = {{0, 0}, {0, -2}, {0, 2}, {2, 0}, {-2, 0}, {0, -4}, {0, 4}, {3, -3}, {-3, -3}, {0, -7}};
			const int32 Nudge = D.Sub - Points.Num();
			if (Nudge >= 100)
			{
				KeyUp(EKeys::E);
				D.WaitUntil = D.Now() + 0.6;
				return true;
			}
			const FVector2D Off = Nudges[FMath::Clamp(Nudge, 0, UE_ARRAY_COUNT(Nudges) - 1)];
			const double WantYaw = FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)) + Off.X;
			const double WantPitch = FMath::RadiansToDegrees(FMath::Atan2(V.Z, FVector2D(V.X, V.Y).Size())) + Off.Y;
			if (!D.AimAt(WantYaw, WantPitch, 1.0)) return false;
			if (!bInteract) return true;
			const URbInteractionComponent* Interaction = Pawn->FindComponentByClass<URbInteractionComponent>();
			if (Interaction && Interaction->GetFocused())
			{
				if (Nudge > 0) D.Finding(TEXT("Low"), TEXT("Interaction"), FString::Printf(TEXT("%s: the prompt only appeared after nudging the view (%d tries); the target is small or partly hidden"), *Name, Nudge));
				KeyDown(EKeys::E);
				D.Sub = Points.Num() + 100;
				return false;
			}
			if (D.Now() - D.LastProgressTime > 0.4) // let the interaction trace update before judging
			{
				FHitResult Hit;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(RbWalk), false, Pawn);
				FVector ViewLoc; FRotator ViewRot;
				P->GetPlayerViewPoint(ViewLoc, ViewRot);
				const bool bHit = D.World->LineTraceSingleByChannel(Hit, ViewLoc, ViewLoc + ViewRot.Vector() * 300.0, ECC_Visibility, Q);
				D.Note(FString::Printf(TEXT("   %s: no prompt at nudge %d; view hits %s at %.0f cm"), *Name, Nudge,
					bHit && Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("nothing"), bHit ? Hit.Distance : 0.0));
				if (Nudge + 1 >= UE_ARRAY_COUNT(Nudges)) return false; // keep the last aim; the step will time out and report
				D.Sub = Points.Num() + Nudge + 1;
				D.LastProgressTime = D.Now();
			}
			return false;
		}, 90.0);
	}

	// ------------------------------------------------------------------ screens

	void ExpectScreen(const FString& Text, bool bExact = false)
	{
		Add(TEXT("see ") + Text, [Text, bExact](FDriver& D)
		{
			TSharedPtr<SWindow> Window = GameWindow();
			if (!Window.IsValid()) return false;
			TArray<TSharedRef<SWidget>> All;
			Collect(Window.ToSharedRef(), All);
			return All.ContainsByPredicate([&](const TSharedRef<SWidget>& W) { return W->GetType() == TEXT("STextBlock") && Matches(TextOf(W), Text, bExact); });
		}, 10.0);
	}
};

// ---------------------------------------------------------------------------------------- the walkthrough

// Waypoints between rooms (doors are at each room's centre on the corridor walls, y = +-200).
const FVector2D Corridor(0, 0);
TArray<FVector2D> ToJuniorDesk(const FVector2D& FromCorridorX)
{
	return {FVector2D(FromCorridorX.X, 0), FVector2D(1500, 0), FVector2D(1500, 280), FVector2D(1350, 420), FVector2D(1350, 950), FVector2D(1410, 965)};
}
TArray<FVector2D> JuniorDeskToCorridor()
{
	return {FVector2D(1350, 950), FVector2D(1350, 420), FVector2D(1500, 280), FVector2D(1500, 0)};
}
const FVector WorkstationScreen(1500, 1098, 112);
const FVector PriyaChest(2330, 880, 135);

void AddWorkstation(FDriver& D, TArray<FVector2D> Path)
{
	D.WalkTo(TEXT("your workstation"), MoveTemp(Path), WorkstationScreen, true);
	D.ExpectScreen(TEXT("Workstation:"));
}

void AddLogOff(FDriver& D)
{
	D.Click(L(TEXT("Log off")));
	D.Wait(0.5);
}

void AddMission001(FDriver& D)
{
	D.Calibrate();
	D.Shot(TEXT("Start_reception"));
	D.WalkTo(TEXT("Priya Okafor"), {FVector2D(500, 300), FVector2D(500, 0), FVector2D(2500, 0), FVector2D(2500, 300), FVector2D(2400, 650)}, PriyaChest, true);
	D.ExpectScreen(TEXT("Continue"), true);
	D.Shot(TEXT("M001_briefing"));
	D.Audit(TEXT("M001 briefing"));
	D.Click(L(TEXT("Continue")));
	D.Wait(0.5);

	TArray<FVector2D> ToDesk = {FVector2D(2500, 300), FVector2D(2500, 0)};
	ToDesk.Append(ToJuniorDesk(FVector2D(2500, 0)));
	AddWorkstation(D, ToDesk);
	D.Shot(TEXT("M001_workstation"));
	D.Audit(TEXT("Workstation"));
	D.Click(L(TEXT("Inbox:"), false));
	D.ExpectScreen(TEXT("Close"), true);
	D.Shot(TEXT("M001_incident_report"));
	D.Audit(TEXT("Document"));
	D.Click(L(TEXT("Close")));
	D.Click(L(TEXT("Risk register: R-014"), false));
	D.Click(L(TEXT("Close")));

	// Risk tools: every tab, and the inputs that make each one do something.
	D.Click(L(TEXT("Risk tools:"), false));
	const TCHAR* Tabs[] = {TEXT("Risk register"), TEXT("KRIs"), TEXT("Loss scenarios"), TEXT("Cost-benefit"), TEXT("Sensitivity"), TEXT("Stress test"), TEXT("Monte Carlo"), TEXT("Actions"), TEXT("Report")};
	for (const TCHAR* Tab : Tabs)
	{
		D.Click(L(Tab));
		D.Wait(0.4);
		if (FCString::Strcmp(Tab, TEXT("Risk register")) == 0)
		{
			FLoc Title;
			Title.Hops = {{TEXT("Add a risk"), true, NSBorder}, {TEXT("Title"), true, NSHBox}};
			Title.WantType = TEXT("SEditableTextBox");
			D.Click(Title);
			D.Type(TEXT("Agency driver induction gaps"));
			FLoc Owner = Title;
			Owner.Hops[1].Anchor = TEXT("Owner");
			D.Click(Owner);
			D.Type(TEXT("Fleet Manager"));
			D.Click(L(TEXT("Add to register")));
			D.Check(TEXT("risk added through the register form"), [](FDriver& Dr, FString& Out)
			{
				const int32 N = static_cast<int32>(Dr.Game()->GetState().Register.Entries().size());
				Out = FString::Printf(TEXT("%d register entries"), N);
				return N >= 2;
			});
		}
		if (FCString::Strcmp(Tab, TEXT("Monte Carlo")) == 0)
		{
			D.Click(L(TEXT("Run simulation")));
			D.Wait(0.6);
		}
		if (FCString::Strcmp(Tab, TEXT("Report")) == 0)
		{
			D.Click(L(TEXT("Export Markdown")));
			D.ExpectScreen(TEXT("Exported to"), false);
		}
		D.Shot(FString(TEXT("Tools_")) + Tab);
		D.Audit(FString(TEXT("Risk tools: ")) + Tab);
	}
	D.Click(L(TEXT("Back to workstation")));
	AddLogOff(D);

	// Evidence around the office.
	TArray<FVector2D> P = JuniorDeskToCorridor();
	P.Append({FVector2D(3500, 0), FVector2D(3500, 300), FVector2D(3445, 500)});
	D.WalkTo(TEXT("maintenance log (meeting room)"), P, FVector(3440, 690, 78), true);
	D.ExpectScreen(TEXT("Close"), true);
	D.Shot(TEXT("M001_evidence_maintenance_log"));
	D.Click(L(TEXT("Close")));
	D.WalkTo(TEXT("insurance schedule (manager's office)"), {FVector2D(3500, 300), FVector2D(3500, 0), FVector2D(2500, 0), FVector2D(2500, 300), FVector2D(2800, 500), FVector2D(2880, 880)}, FVector(2930, 1050, 45), true);
	D.ExpectScreen(TEXT("Close"), true);
	D.Click(L(TEXT("Close")));
	D.WalkTo(TEXT("client contract (boardroom)"), {FVector2D(2800, 500), FVector2D(2500, 300), FVector2D(2500, 0), FVector2D(3000, 0), FVector2D(3000, -300), FVector2D(3055, -495)}, FVector(3000, -650, 76), true);
	D.ExpectScreen(TEXT("Close"), true);
	D.Click(L(TEXT("Close")));
	D.WalkTo(TEXT("alarm terminal (monitoring room)"), {FVector2D(3000, -300), FVector2D(3000, 0), FVector2D(1500, 0), FVector2D(1500, -300), FVector2D(1400, -500), FVector2D(1400, -930)}, FVector(1500, -1078, 112), true);
	D.ExpectScreen(TEXT("Close"), true);
	D.Shot(TEXT("M001_evidence_alarm_log"));
	D.Click(L(TEXT("Close")));
	D.WalkTo(TEXT("temperature checks (training area)"), {FVector2D(1400, -500), FVector2D(1500, -300), FVector2D(1500, 0), FVector2D(500, 0), FVector2D(500, -300), FVector2D(640, -500), FVector2D(640, -880)}, FVector(520, -880, 77), true);
	D.ExpectScreen(TEXT("Close"), true);
	D.Click(L(TEXT("Close")));
	D.Check(TEXT("all seven M001 documents read"), [](FDriver& Dr, FString& Out)
	{
		int32 Viewed = 0;
		for (const FRbEvidenceView& E : Dr.Game()->GetEvidence()) Viewed += E.bViewed ? 1 : 0;
		Out = FString::Printf(TEXT("%d of 7"), Viewed);
		return Viewed == 7;
	}, TEXT("Blocker"));

	// The assessment form, filled field by field.
	AddWorkstation(D, {FVector2D(640, -500), FVector2D(500, -300), FVector2D(500, 0), FVector2D(1500, 0), FVector2D(1500, 280), FVector2D(1350, 420), FVector2D(1350, 950), FVector2D(1410, 965)});
	D.Click(L(TEXT("Assessment form: R-014")));
	D.ExpectScreen(TEXT("1. Missing information"));
	D.Shot(TEXT("M001_assessment_form"));
	D.Audit(TEXT("Assessment form"));
	URbGameSubsystem* G = D.Game();
	auto LabelOf = [](const TArray<FRbOptionView>& Options, const FString& Id) { const FRbOptionView* O = Options.FindByPredicate([&](const FRbOptionView& X) { return X.Id == Id; }); return O ? O->Label : Id; };
	for (const TCHAR* Id : {TEXT("compressor_failure_history"), TEXT("after_hours_escalation"), TEXT("alarm_threshold_config")})
	{
		D.Click(In(TEXT("1. Missing information"), NSBorder, LabelOf(G->GetMissingInfoOptions(), Id), true));
	}
	auto Level = [&D](const TCHAR* Section, const TCHAR* Row, int32 Value)
	{
		FLoc X;
		X.Hops = {{Section, true, NSBorder}, {Row, true, NSHBox}};
		X.Label = FString::FromInt(Value);
		D.Click(X);
	};
	Level(TEXT("2. Inherent risk (before controls)"), TEXT("Likelihood"), 4);
	Level(TEXT("2. Inherent risk (before controls)"), TEXT("Impact"), 4);
	const TArray<FString> Controls = G->GetControlNames();
	const TCHAR* Ratings[] = {TEXT("Ineffective"), TEXT("Partially effective"), TEXT("Ineffective")};
	for (int32 i = 0; i < Controls.Num() && i < 3; ++i)
	{
		FLoc X;
		X.Hops = {{TEXT("3. Control effectiveness"), true, NSBorder}, {Controls[i], true, NSHBox}};
		X.Label = Ratings[i];
		D.Click(X);
	}
	Level(TEXT("4. Residual risk (after controls)"), TEXT("Likelihood"), 4);
	Level(TEXT("4. Residual risk (after controls)"), TEXT("Impact"), 3);
	D.Click(In(TEXT("5. Recommended treatment"), NSBorder, LabelOf(G->GetTreatmentOptions(), TEXT("repair_and_escalation")), true));
	D.Click(In(TEXT("6. Action owner"), NSBorder, LabelOf(G->GetActionOwnerOptions(), TEXT("ops_site_manager")), true));
	for (const TCHAR* Id : {TEXT("alarm_ignored"), TEXT("service_overdue"), TEXT("penalty_exposure"), TEXT("outside_appetite")})
	{
		D.Click(In(TEXT("7. Rationale"), NSBorder, LabelOf(G->GetRationaleTags(), Id), true));
	}
	FLoc Rationale;
	Rationale.Hops = {{TEXT("7. Rationale"), true, NSBorder}};
	Rationale.WantType = TEXT("SMultiLineEditableTextBox");
	D.Click(Rationale);
	D.Type(TEXT("The alarm is silenced without action and servicing is overdue: controls exist on paper but do not operate."));
	D.ExpectScreen(TEXT("7 / 7 sections complete"));
	D.Shot(TEXT("M001_assessment_complete"));
	D.Click(L(TEXT("Submit assessment")));
	D.Wait(1.0);
	D.Check(TEXT("assessment submitted through the form"), [](FDriver& Dr, FString& Out) { Out = Dr.Game()->GetCheckpoint(); return Dr.Game()->IsSubmitted(); }, TEXT("Blocker"));
	D.Add(TEXT("close any open screen"), [](FDriver& Dr) { if (Dr.Hud->IsScreenOpen()) Dr.Hud->CloseScreen(); return true; });

	TArray<FVector2D> ToPriya = JuniorDeskToCorridor();
	ToPriya.Append({FVector2D(2500, 0), FVector2D(2500, 300), FVector2D(2400, 650)});
	D.WalkTo(TEXT("Priya for the debrief"), ToPriya, PriyaChest, true);
	D.ExpectScreen(TEXT("Close"), true);
	D.Shot(TEXT("M001_debrief"));
	D.Audit(TEXT("M001 debrief"));
	D.Click(L(TEXT("Close")));
	D.Check(TEXT("Mission 001 passed and debriefed"), [](FDriver& Dr, FString& Out)
	{
		Out = FString::Printf(TEXT("score %d, objective '%s'"), Dr.Game()->GetDebrief().OverallScore, *Dr.Game()->GetCurrentObjectiveText());
		return Dr.Game()->GetDebrief().bPassed && Dr.Game()->IsDebriefed();
	}, TEXT("Blocker"));
}

// ---------------------------------------------------------------------------------------- campaign

enum class EPolicy { Best, Worst, Weak };

TArray<FString> ChooseAnswers(const rb::MissionQuestionDef& Q, EPolicy Policy, bool& bUsedWeak)
{
	TArray<FString> Out;
	if (Q.Kind == rb::EQuestionKind::Multi)
	{
		for (const rb::MissionOptionDef& O : Q.Options)
		{
			if ((Policy == EPolicy::Worst) != O.bCorrect) Out.Add(ToFString(O.Label));
		}
		return Out;
	}
	const rb::MissionOptionDef* Pick = &Q.Options[0];
	for (const rb::MissionOptionDef& O : Q.Options)
	{
		if (Policy == EPolicy::Worst ? O.Score < Pick->Score : O.Score > Pick->Score) Pick = &O;
	}
	if (Policy == EPolicy::Weak && !bUsedWeak)
	{
		// The weak choice: an option that schedules an incident able to open a crisis.
		for (const rb::MissionOptionDef& O : Q.Options)
		{
			const bool bOpensCrisis = std::any_of(O.Schedules.begin(), O.Schedules.end(), [](const rb::ConsequenceDef& C) { return !C.FlagIfOccurs.empty(); });
			if (bOpensCrisis && O.Score < Pick->Score)
			{
				Pick = &O;
				bUsedWeak = true;
				break;
			}
		}
	}
	Out.Add(ToFString(Pick->Label));
	return Out;
}

void AddMission(FDriver& D, const rb::MissionDef& M, EPolicy Policy, bool bScreens, bool bReview = false)
{
	const FString Id = ToFString(M.Id);
	const FString Title = ToFString(M.Title);
	const TCHAR* PolicyName = Policy == EPolicy::Best ? TEXT("best") : (Policy == EPolicy::Worst ? TEXT("deliberately weak (to fail)") : TEXT("one weak decision"));
	D.Add(TEXT("begin ") + Id, [Id, Title, PolicyName](FDriver& Dr) { Dr.Note(FString::Printf(TEXT("== %s %s: %s answers"), *Id, *Title, PolicyName)); return true; });
	D.Click(In(Title, NSBorder, bReview ? TEXT("Review case") : TEXT("Open case"), true));
	D.ExpectScreen(TEXT("Evidence"), true);
	if (bScreens) D.Shot(Id + TEXT("_briefing"));
	D.Audit(Id + TEXT(" mission"));
	bool bInPerson = false;
	for (const rb::MissionEvidenceDef& E : M.Evidence)
	{
		if (E.At.empty())
		{
			D.Click(In(TEXT("Evidence"), NSBorder, ToFString(E.Title), true, false));
			continue;
		}
		bInPerson = true;
	}
	if (bInPerson)
	{
		// In-person evidence: leave the terminal and use the case markers in the office (the walk itself is
		// covered by the Mission 001 route; here the marker is used directly).
		D.Click(L(TEXT("Back to workstation")));
		AddLogOff(D);
		for (const rb::MissionEvidenceDef& E : M.Evidence)
		{
			if (E.At.empty()) continue;
			const FString EvId = ToFString(E.Id);
			D.Add(TEXT("in person ") + EvId, [EvId](FDriver& Dr)
			{
				for (TActorIterator<ARbCaseMarker> It(Dr.World.Get()); It; ++It)
				{
					if (It->EvidenceId == EvId)
					{
						It->Interact(Dr.Hud.Get());
						Dr.WaitUntil = Dr.Now() + 0.6;
						return true;
					}
				}
				return Dr.Now() > Dr.StepStart + 5.0 ? (Dr.Finding(TEXT("High"), TEXT("Casework"), TEXT("No case marker for ") + EvId), true) : false;
			});
			D.Audit(TEXT("In-person ") + EvId);
			D.Press(EKeys::Escape);
		}
		D.Add(TEXT("back to the workstation"), [](FDriver& Dr) { Dr.Hud->OpenWorkstation(); return true; });
		D.Click(L(TEXT("Case files: your assignments"), false));
		D.Click(In(Title, NSBorder, TEXT("Resume"), true));
		D.ExpectScreen(TEXT("Evidence"), true);
	}
	bool bUsedWeak = false;
	for (const rb::MissionQuestionDef& Q : M.Questions)
	{
		const FString Anchor = ToFString(Q.Prompt).Left(60);
		if (Q.Kind == rb::EQuestionKind::Numeric)
		{
			// Click into the figure box and type, as a player would (worst play: three times the answer).
			FLoc Box = In(Anchor, NSBorder, FString(), false, true);
			Box.WantType = TEXT("SEditableText");
			D.Click(Box);
			D.Type(FString::Printf(TEXT("%.10g"), Policy == EPolicy::Worst ? Q.Answer * 3.0 : Q.Answer));
			continue;
		}
		if (Q.Kind == rb::EQuestionKind::Rating)
		{
			static const TCHAR* const Likelihood[] = {TEXT("Rare"), TEXT("Unlikely"), TEXT("Possible"), TEXT("Likely"), TEXT("Almost certain")};
			static const TCHAR* const Impact[] = {TEXT("Minimal"), TEXT("Minor"), TEXT("Moderate"), TEXT("Major"), TEXT("Severe")};
			const int32 L = Policy == EPolicy::Worst ? 1 : Q.TargetLikelihood;
			const int32 I = Policy == EPolicy::Worst ? 1 : Q.TargetImpact;
			D.Click(In(Anchor, NSBorder, FString::Printf(TEXT("%d %s"), L, Likelihood[L - 1]), false, true));
			D.Click(In(Anchor, NSBorder, FString::Printf(TEXT("%d %s"), I, Impact[I - 1]), false, true));
			continue;
		}
		for (const FString& Label : ChooseAnswers(Q, Policy, bUsedWeak))
		{
			D.Click(In(Anchor, NSBorder, Label, false, true));
		}
	}
	if (!M.PresentAt.empty())
	{
		// Board papers are presented at the boardroom lectern; the drafted answers carry over.
		D.ExpectScreen(TEXT("presented in person"));
		D.Click(L(TEXT("Back to workstation")));
		AddLogOff(D);
		D.Add(TEXT("lectern ") + Id, [](FDriver& Dr)
		{
			for (TActorIterator<ARbCaseMarker> It(Dr.World.Get()); It; ++It)
			{
				if (It->Kind == ERbCaseMarkerKind::Lectern)
				{
					It->Interact(Dr.Hud.Get());
					return true;
				}
			}
			return Dr.Now() > Dr.StepStart + 5.0 ? (Dr.Finding(TEXT("Blocker"), TEXT("Casework"), TEXT("No lectern in the boardroom")), true) : false;
		});
		if (bScreens) D.Shot(Id + TEXT("_lectern"));
	}
	D.ExpectScreen(TEXT("Ready to submit"));
	D.Click(L(TEXT("Submit")));
	D.ExpectScreen(TEXT("Debrief"), false);
	if (bScreens) D.Shot(Id + TEXT("_debrief"));
	D.Audit(Id + TEXT(" debrief"));
	D.Add(TEXT("record ") + Id, [Id](FDriver& Dr)
	{
		const FRbMissionDebriefView V = Dr.Campaign()->GetDebrief(Id);
		Dr.Note(FString::Printf(TEXT("   %s score %d (%s)%s%s"), *Id, V.Score, V.bPassed ? TEXT("passed") : TEXT("FAILED"),
			V.bPromoted ? *FString::Printf(TEXT(", promoted to %s"), *V.LevelTitle) : TEXT(""),
			V.Consequences.IsEmpty() ? TEXT("") : *(TEXT("; events: ") + FString::Join(V.Consequences, TEXT(" / ")))));
		return true;
	});
	D.Click(L(TEXT("Back to case files")));
	// Every promotion gets its ceremony once the player leaves the screens.
	D.Add(TEXT("promotion ceremony ") + Id, [Id](FDriver& Dr)
	{
		const FRbMissionDebriefView V = Dr.Campaign()->GetDebrief(Id);
		ARbHUD* H = Dr.Hud.Get();
		if (!V.bPromoted || !H) return true;
		if (H->IsCeremonyPlaying())
		{
			if (!Dr.bCeremonySeen) Dr.Note(TEXT("PASS promotion ceremony: ") + V.LevelTitle);
			Dr.bCeremonySeen = true;
			return false;
		}
		if (Dr.bCeremonySeen || Dr.Now() > Dr.StepStart + 4.0)
		{
			if (!Dr.bCeremonySeen) Dr.Finding(TEXT("High"), TEXT("Promotion"), FString::Printf(TEXT("No ceremony for the promotion to %s (level %d)"), *V.LevelTitle, V.Level));
			Dr.bCeremonySeen = false;
			H->OpenCaseFiles();
			return true;
		}
		if (H->IsScreenOpen()) H->CloseScreen();
		return false;
	}, 15.0, false);
}

// Plans the next mission from what the case list shows, the way a player would pick.
void AddCampaignLoop(FDriver& D, int32 StopAtLevel)
{
	D.Add(TEXT("plan next case"), [StopAtLevel](FDriver& Dr)
	{
		URbCampaignSubsystem* C = Dr.Campaign();
		URbGameSubsystem* G = Dr.Game();
		const FRbCareerView Career = G->GetCareer();
		if (Career.Level >= StopAtLevel || G->GetState().Career.HasPassed("CRO"))
		{
			Dr.Note(FString::Printf(TEXT("campaign leg finished at level %d (%s)"), Career.Level, *Career.Title));
			return true;
		}
		const TArray<FRbMissionSummary> List = C->GetMissions();
		const FRbMissionSummary* Next = List.FindByPredicate([](const FRbMissionSummary& M) { return M.bAvailable || M.bActive; });
		if (!Next)
		{
			Dr.Finding(TEXT("Blocker"), TEXT("Campaign"), FString::Printf(TEXT("No case available at level %d before the end of the campaign"), Career.Level));
			return true;
		}
		const rb::MissionDef* Def = C->GetCatalogue().Find(RbConvert::ToStd(Next->Id));
		bool bCrisisOpen = false;
		for (const FRbMissionSummary& M : List)
		{
			const rb::MissionDef* MD = C->GetCatalogue().Find(RbConvert::ToStd(M.Id));
			if (MD && !MD->RequiresFlags.empty() && (M.bAvailable || M.Attempts > 0)) bCrisisOpen = true;
		}
		Dr.bCrisisTriggered = Dr.bCrisisTriggered || bCrisisOpen;
		EPolicy Policy = EPolicy::Best;
		const bool bFirstAttempt = Next->Attempts == 0;
		if (Next->Id == TEXT("M003") && bFirstAttempt) Policy = EPolicy::Worst; // fail it on purpose, retry next
		else if (!Dr.bCrisisTriggered && Def->Kind == rb::EMissionKind::Mission) Policy = EPolicy::Weak;
		const bool bScreens = bFirstAttempt ? (Def->Kind != rb::EMissionKind::Mission || Next->Id == TEXT("M002") || Next->Id == TEXT("M003") ||
			Next->Id == TEXT("M007") || Next->Id == TEXT("M013") || Next->Id == TEXT("M019") || Next->Id == TEXT("M023")) : true;
		if (Next->bReview)
		{
			Dr.Note(FString::Printf(TEXT("stuck at level %d: every case passed, evidence below the bar; reviewing %s"), Career.Level, *Next->Id));
			if (!Dr.FindingKeys.Contains(TEXT("review-seen")))
			{
				Dr.FindingKeys.Add(TEXT("review-seen"));
				Dr.Note(FString::Printf(TEXT("promotion gaps: %s"), *FString::Join(Career.UnmetPromotionGates, TEXT("; "))));
			}
		}
		FDriver Plan;
		AddMission(Plan, *Def, Next->bReview ? EPolicy::Best : Policy, bScreens || Next->bReview, Next->bReview);
		AddCampaignLoop(Plan, StopAtLevel);
		Dr.InsertNext(MoveTemp(Plan.Queue));
		return true;
	}, 5.0);
}

void AddSettingsSaveQuit(FDriver& D)
{
	D.Click(L(TEXT("Back to workstation")));
	AddLogOff(D);
	D.Press(EKeys::Escape);
	D.ExpectScreen(TEXT("Paused"), true);
	D.Shot(TEXT("Pause_menu"));
	D.Click(L(TEXT("Settings")));
	D.ExpectScreen(TEXT("Text size"), true);
	FLoc Slider;
	Slider.Hops = {{TEXT("Text size"), true, NSHBox}};
	Slider.WantType = TEXT("SSlider");
	D.Click(Slider); // middle of the track: 1.2
	D.Click(L(TEXT("Reduced motion (no motion blur)")));
	D.Wait(0.3);
	D.Shot(TEXT("Settings_changed"));
	D.Audit(TEXT("Settings"));
	D.Check(TEXT("settings changed through the controls"), [](FDriver& Dr, FString& Out)
	{
		const FRbSettingsView S = Dr.Game()->GetSettings();
		Out = FString::Printf(TEXT("text %.2f, reduced motion %d"), S.TextScale, S.bReducedMotion ? 1 : 0);
		return S.TextScale > 1.05f && S.bReducedMotion;
	});
	D.Click(L(TEXT("Back")));
	D.ExpectScreen(TEXT("Paused"), true);
	D.Click(L(TEXT("Save game")));
	D.Wait(0.8);
	D.Add(TEXT("write phase 1 log"), [](FDriver& Dr)
	{
		const rb::GameState& S = Dr.Game()->GetState();
		Dr.Note(FString::Printf(TEXT("saved: level %d, %d campaign attempts, %d achievements, text %.2f"), S.Career.Level, static_cast<int32>(S.Campaign.Records.size()),
			static_cast<int32>(S.Achievements.size()), S.Settings.TextScale));
		Dr.Note(TEXT("phase 1 complete; quitting through the pause menu"));
		Dr.WriteLog(); // the next click ends the process
		return true;
	});
	D.Click(L(TEXT("Quit to desktop")), false);
}

void Build(FDriver& D)
{
	D.Add(TEXT("show main menu"), [](FDriver& Dr) { Dr.Hud->ShowMainMenu(); return true; });
	D.Wait(3.0);
	D.Shot(TEXT("Main_menu"));
	D.Audit(TEXT("Main menu"));
	if (D.Phase == 1)
	{
		D.Click(L(TEXT("New game")));
		D.ExpectScreen(TEXT("Choose difficulty"), true);
		D.Shot(TEXT("Difficulty"));
		D.Click(L(TEXT("Professional")));
		D.ExpectScreen(TEXT("Where do you come from?"), true);
		D.Shot(TEXT("Background"));
		D.Audit(TEXT("Background"));
		D.Click(L(TEXT("Ex-operations")));
		D.Wait(2.0);
		AddMission001(D);
		AddWorkstation(D, {FVector2D(2400, 650), FVector2D(2500, 300), FVector2D(2500, 0), FVector2D(1500, 0), FVector2D(1500, 280), FVector2D(1350, 420), FVector2D(1350, 950), FVector2D(1410, 965)});
		D.Click(L(TEXT("Case files: your assignments")));
		D.ExpectScreen(TEXT("Case files"), true);
		D.Shot(TEXT("Case_files_first_visit"));
		D.Audit(TEXT("Case files"));
		AddCampaignLoop(D, 5);
		D.Shot(TEXT("Case_files_at_team_lead"));
		AddSettingsSaveQuit(D);
		return;
	}
	D.Check(TEXT("main menu offers Continue"), [](FDriver& Dr, FString& Out) { Out = TEXT("save present"); return Locate(L(TEXT("Continue"))).IsValid(); }, TEXT("Blocker"));
	D.Click(L(TEXT("Continue")));
	D.Wait(2.0);
	D.Shot(TEXT("Continued"));
	D.Check(TEXT("save, quit and Continue restored the career and settings"), [](FDriver& Dr, FString& Out)
	{
		const rb::GameState& S = Dr.Game()->GetState();
		Out = FString::Printf(TEXT("level %d, %d campaign attempts, text %.2f (UI %.2f), reduced motion %d"), S.Career.Level, static_cast<int32>(S.Campaign.Records.size()),
			S.Settings.TextScale, RbUI::GetTextScale(), S.Settings.bReducedMotion ? 1 : 0);
		return S.Career.Level >= 5 && S.Settings.TextScale > 1.05 && S.Settings.bReducedMotion && FMath::IsNearlyEqual(RbUI::GetTextScale(), static_cast<float>(S.Settings.TextScale), 0.01f);
	}, TEXT("Blocker"));
	D.Calibrate();
	AddWorkstation(D, {FVector2D(500, 300), FVector2D(500, 0), FVector2D(1500, 0), FVector2D(1500, 280), FVector2D(1350, 420), FVector2D(1350, 950), FVector2D(1410, 965)});
	D.Click(L(TEXT("Case files: your assignments")));
	D.ExpectScreen(TEXT("Case files"), true);
	AddCampaignLoop(D, 99);
	D.Shot(TEXT("Case_files_complete"));
	D.Audit(TEXT("Case files at the end"));
	D.Check(TEXT("CRO capstone passed"), [](FDriver& Dr, FString& Out)
	{
		const rb::GameState& S = Dr.Game()->GetState();
		Out = FString::Printf(TEXT("level %d, %d achievements"), S.Career.Level, static_cast<int32>(S.Achievements.size()));
		return S.Career.Level == 10 && S.Career.HasPassed("CRO");
	}, TEXT("Blocker"));
	D.Click(L(TEXT("Back to workstation")));
	AddLogOff(D);
	D.Press(EKeys::J);
	D.ExpectScreen(TEXT("Journal"), true);
	D.Shot(TEXT("Journal_end"));
	D.Audit(TEXT("Journal"));
	D.Add(TEXT("scroll the journal to the end"), [](FDriver& Dr)
	{
		TArray<TSharedRef<SWidget>> All;
		if (TSharedPtr<SWindow> W = GameWindow()) Collect(W.ToSharedRef(), All);
		for (const TSharedRef<SWidget>& X : All)
		{
			if (X->GetType() == TEXT("SScrollBox")) StaticCastSharedRef<SScrollBox>(X)->ScrollToEnd();
		}
		Dr.WaitUntil = Dr.Now() + 0.5;
		return true;
	});
	D.Shot(TEXT("Journal_end_scrolled"));
	D.Audit(TEXT("Journal (scrolled)"));
	D.Click(L(TEXT("Close")));
	D.Add(TEXT("summary"), [](FDriver& Dr)
	{
		const rb::GameState& S = Dr.Game()->GetState();
		Dr.Note(FString::Printf(TEXT("end: level %d, %d campaign attempts, %d achievements, org week %d, cash %.0f, reputation %.0f"), S.Career.Level,
			static_cast<int32>(S.Campaign.Records.size()), static_cast<int32>(S.Achievements.size()), S.Org.Week,
			S.Org.Get(rb::EOrgMetric::Cash), S.Org.Get(rb::EOrgMetric::Reputation)));
		return true;
	});
}
} // namespace

void Start(UWorld* World, ARbHUD* Hud, int32 Phase)
{
	TSharedRef<FDriver> D = MakeShared<FDriver>();
	D->World = World;
	D->Hud = Hud;
	D->Phase = Phase;
	D->Start = FPlatformTime::Seconds();
	D->Note(FString::Printf(TEXT("walkthrough phase %d started"), Phase));
	Build(*D);
	// Core ticker: menus pause the world, and world timers stop while paused.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([D](float) { return D->Tick(); }), 0.0f);
}
} // namespace RbWalkthrough
