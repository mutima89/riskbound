// Automation tests for the UE session layer. Run: Scripts\RunUnrealTests.bat (filter "Riskbound.").
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "RbGameSubsystem.h"
#include "RbSave.h"
#include "RbSaveSubsystem.h"
#include "RbSelfTest.h"

namespace
{
constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

// A standalone game instance with its own world, so subsystems initialise exactly as in a game.
struct FTestSession
{
	UGameInstance* Instance = nullptr;
	URbGameSubsystem* Game = nullptr;
	URbSaveSubsystem* Saves = nullptr;

	explicit FTestSession(const FString& Slot)
	{
		Instance = NewObject<UGameInstance>(GEngine);
		Instance->AddToRoot();
		Instance->InitializeStandalone();
		Game = Instance->GetSubsystem<URbGameSubsystem>();
		Saves = Instance->GetSubsystem<URbSaveSubsystem>();
		if (Saves) Saves->SetSlotName(Slot);
	}

	~FTestSession()
	{
		UWorld* World = Instance->GetWorld();
		Instance->Shutdown();
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		Instance->RemoveFromRoot();
	}
};

template <class TEnum>
bool TestEnum(FAutomationTestBase& Test, const TCHAR* What, TEnum Actual, TEnum Expected)
{
	return Test.TestEqual(What, static_cast<int32>(Actual), static_cast<int32>(Expected));
}

void DeleteSlot(const FString& Path)
{
	IFileManager& Files = IFileManager::Get();
	for (const TCHAR* Suffix : {TEXT(""), TEXT(".bak"), TEXT(".tmp"), TEXT(".corrupt")})
	{
		Files.Delete(*(Path + Suffix), false, true, true);
	}
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRbSessionPlaySaveLoadTest, "Riskbound.Session.PlaySaveLoadRoundTrip", TestFlags)
bool FRbSessionPlaySaveLoadTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("autotest_roundtrip");
	std::string Saved;
	FString Path;
	{
		FTestSession S(Slot);
		if (!TestNotNull(TEXT("game subsystem"), S.Game) || !TestNotNull(TEXT("save subsystem"), S.Saves)) return false;
		Path = S.Saves->GetSlotPath();
		DeleteSlot(Path);

		S.Game->NewGame(ERbDifficulty::Professional);
		TestFalse(TEXT("debrief hidden before submit"), S.Game->GetDebrief().bAvailable);
		RbSelfTest::PlayMission001(S.Game);

		TestTrue(TEXT("submitted"), S.Game->IsSubmitted());
		TestTrue(TEXT("debriefed"), S.Game->IsDebriefed());
		TestTrue(TEXT("passed M001"), S.Game->GetCareer().PassedMissions.Contains(TEXT("M001")));
		TestEqual(TEXT("career level"), S.Game->GetCareer().Level, 2);
		TestEqual(TEXT("R-014 treatment"), S.Game->GetRegisterEntry(TEXT("R-014")).Treatment, FString(TEXT("Reduce")));
		TestEqual(TEXT("decision quality"), S.Game->GetDebrief().OverallScore, 100);
		FString Error;
		TestTrue(TEXT("save"), S.Saves->Save(Error));
		Saved = rb::SerializeGame(S.Game->GetState());
	}
	{
		FTestSession Fresh(Slot);
		TestFalse(TEXT("fresh session has no game"), Fresh.Game->HasGame());
		FString Message;
		TestEnum(*this, TEXT("load outcome"), Fresh.Game->Continue(Message), ERbLoadOutcome::Ok);
		TestTrue(TEXT("state identical after reload"), rb::SerializeGame(Fresh.Game->GetState()) == Saved);
		TestTrue(TEXT("debrief recomputed after reload"), Fresh.Game->GetDebrief().bAvailable && Fresh.Game->GetDebrief().bPassed);
		TestEqual(TEXT("objective after reload"), Fresh.Game->GetCheckpoint(), FString(URbGameSubsystem::CheckpointDebriefed));
	}
	DeleteSlot(Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRbSaveRecoveryTest, "Riskbound.Save.RecoversFromCorruptPrimary", TestFlags)
bool FRbSaveRecoveryTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("autotest_recovery");
	std::string FirstSave;
	FString Path;
	{
		FTestSession S(Slot);
		Path = S.Saves->GetSlotPath();
		DeleteSlot(Path);
		S.Game->NewGame(ERbDifficulty::Analyst);
		S.Game->TalkToManager(); // checkpoint autosave -> primary
		FirstSave = rb::SerializeGame(S.Game->GetState());
		S.Game->ViewEvidence(TEXT("incident_report"));
		FString Error;
		TestTrue(TEXT("second save"), S.Saves->Save(Error)); // first save rotates to .bak
	}
	TestTrue(TEXT("backup exists"), IFileManager::Get().FileExists(*(Path + TEXT(".bak"))));
	TestTrue(TEXT("corrupt the primary"), FFileHelper::SaveStringToFile(TEXT("RISKBOUND-SAVE\nversion\t2\ngarbage"), *Path));
	{
		FTestSession Fresh(Slot);
		FString Message;
		TestEnum(*this, TEXT("load outcome"), Fresh.Game->Continue(Message), ERbLoadOutcome::RecoveredFromBackup);
		TestTrue(TEXT("player is told"), Message.Contains(TEXT("backup")));
		TestTrue(TEXT("backup state restored"), rb::SerializeGame(Fresh.Game->GetState()) == FirstSave);
		TestTrue(TEXT("manager met in restored state"), Fresh.Game->HasMetManager());
	}
	{
		// Both files damaged: Continue fails cleanly and loads nothing.
		FFileHelper::SaveStringToFile(TEXT("not a save"), *(Path + TEXT(".bak")));
		FFileHelper::SaveStringToFile(TEXT("not a save"), *Path);
		FTestSession Fresh(Slot);
		FString Message;
		TestEnum(*this, TEXT("both corrupt"), Fresh.Game->Continue(Message), ERbLoadOutcome::Corrupt);
		TestFalse(TEXT("no game after failed load"), Fresh.Game->HasGame());
	}
	DeleteSlot(Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRbSubmissionGatingTest, "Riskbound.Gameplay.SubmissionNeedsBriefingAndEvidence", TestFlags)
bool FRbSubmissionGatingTest::RunTest(const FString& Parameters)
{
	FTestSession S(TEXT("autotest_gating"));
	const FString Path = S.Saves->GetSlotPath();
	S.Game->NewGame(ERbDifficulty::Professional);
	const FRbSubmissionInput Answer = RbSelfTest::StrongAssessment();
	TestEnum(*this, TEXT("before briefing"), S.Game->SubmitAssessment(Answer), ERbSubmitResult::ManagerNotMet);
	TestEnum(*this, TEXT("first talk is the briefing"), S.Game->TalkToManager(), ERbManagerTalk::Briefing);
	TestEnum(*this, TEXT("then reminders"), S.Game->TalkToManager(), ERbManagerTalk::Reminder);
	TestEnum(*this, TEXT("needs incident report"), S.Game->SubmitAssessment(Answer), ERbSubmitResult::IncidentReportNotReviewed);
	TestTrue(TEXT("incident report"), S.Game->ViewEvidence(TEXT("incident_report")));
	TestFalse(TEXT("unknown evidence id"), S.Game->ViewEvidence(TEXT("no_such_doc")));
	TestEnum(*this, TEXT("needs register"), S.Game->SubmitAssessment(Answer), ERbSubmitResult::RegisterNotInspected);
	S.Game->ViewEvidence(TEXT("risk_register_R014"));
	FRbSubmissionInput Partial = Answer;
	Partial.ControlRatings.Reset();
	TestEnum(*this, TEXT("incomplete form"), S.Game->SubmitAssessment(Partial), ERbSubmitResult::IncompleteSubmission);
	TestFalse(TEXT("rubric still hidden"), S.Game->GetDebrief().bAvailable);
	TestEnum(*this, TEXT("valid submit"), S.Game->SubmitAssessment(Answer), ERbSubmitResult::Ok);
	TestEnum(*this, TEXT("no resubmit"), S.Game->SubmitAssessment(Answer), ERbSubmitResult::AlreadySubmitted);
	TestEnum(*this, TEXT("manager now debriefs"), S.Game->TalkToManager(), ERbManagerTalk::Debrief);
	DeleteSlot(Path);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
#include "RbCredits.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRbCreditsContactTest, "Riskbound.Credits.IncludeCreatorContactAndAttribution", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FRbCreditsContactTest::RunTest(const FString& Parameters)
{
	const FString Text = RbCredits::AsPlainText();
	TestTrue(TEXT("creator name"), Text.Contains(TEXT("M A Almahlaki")));
	TestTrue(TEXT("email, exact"), Text.Contains(TEXT("m.awwadh.k@gmail.com"), ESearchCase::CaseSensitive));
	TestTrue(TEXT("LinkedIn URL, exact"), Text.Contains(TEXT("https://www.linkedin.com/in/mohammed-almahlaki001/"), ESearchCase::CaseSensitive));
	TestTrue(TEXT("engine attribution"), Text.Contains(TEXT("Epic Games, Inc.")));
	TestTrue(TEXT("development assistance"), Text.Contains(TEXT("Claude Code by Anthropic")));
	TestTrue(TEXT("asset list from the manifest"), RbCredits::Assets().Num() >= 20 && Text.Contains(TEXT("CC0 1.0")) && Text.Contains(TEXT("SIL Open Font License")));
	return true;
}
#endif
