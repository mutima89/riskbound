// Automation tests for the recorded colleague voices (Riskbound.Voice.*): Scripts/BuildVoices.py and the
// import produced a clip for the lines the game speaks, under the key the game looks up.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RbAudioSubsystem.h"
#include "RbConvert.h"
#include "RbVoice.h"
#include "Sound/SoundWave.h"
#include "UI/RbScreens.h"

namespace
{
constexpr EAutomationTestFlags VoiceFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

USoundWave* VoiceFor(const TCHAR* Person, const FString& Line)
{
	return URbAudioSubsystem::FindVoice(RbConvert::ToFString(rb::VoiceLineKey(RbConvert::ToStd(Person), RbConvert::ToStd(Line))));
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRbVoiceLinesTest, "Riskbound.Voice.FixedLinesHaveClips", VoiceFlags)
bool FRbVoiceLinesTest::RunTest(const FString& Parameters)
{
	// Lines written in the game code are copied into Data/VoiceCast.json "fixed"; this catches a drift.
	for (const bool bHints : {false, true})
	{
		USoundWave* Briefing = VoiceFor(TEXT("priya"), RbScreens::BriefingText(bHints).ToString());
		if (TestNotNull(bHints ? TEXT("briefing with hints is voiced") : TEXT("briefing is voiced"), Briefing))
		{
			TestTrue(TEXT("briefing clip is a real recording"), Briefing->GetDuration() > 20.0f);
		}
	}
	TestNotNull(TEXT("the busy line is voiced"), VoiceFor(TEXT("callum"), TEXT("You're busy. It's in your inbox.")));
	TestNotNull(TEXT("approach lines are voiced"), VoiceFor(TEXT("celeste"), TEXT("Have you got a minute? Coffee?")));
	TestNull(TEXT("an unknown line has no clip"), VoiceFor(TEXT("priya"), TEXT("A line nobody recorded.")));
	return true;
}

#endif
