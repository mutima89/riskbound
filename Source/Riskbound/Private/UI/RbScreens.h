// Builders for the modal screens. Each returns the widget and the control that takes initial
// keyboard focus. Screens only display views and forward button presses to ARbHUD.
#pragma once

#include "CoreMinimal.h"
#include "RbTypes.h"
#include "RbUIStyle.h"

class ARbHUD;
class SWidget;

struct FRbScreenParts
{
	TSharedRef<SWidget> Widget;
	TSharedPtr<SWidget> Focus;
};

namespace RbScreens
{
FRbScreenParts MainMenu(ARbHUD* Hud);
FRbScreenParts DifficultyPicker(ARbHUD* Hud);
FRbScreenParts PauseMenu(ARbHUD* Hud);
FRbScreenParts Settings(ARbHUD* Hud, TFunction<void()> OnBack);
FRbScreenParts Journal(ARbHUD* Hud);
FRbScreenParts Workstation(ARbHUD* Hud);
FRbScreenParts Document(const FText& Title, const FText& Subtitle, const FText& Body, TFunction<void()> OnClose);
FRbScreenParts Dialogue(const FText& Speaker, const FText& Text, TFunction<void()> OnContinue);
FRbScreenParts Debrief(const FRbDebriefView& Debrief, const FRbCareerView& Career, const TArray<FString>& Achievements, TFunction<void()> OnClose);

FText BriefingText(bool bHints);
FText RegisterEntryText(const FRbRegisterEntryView& Entry);
} // namespace RbScreens
