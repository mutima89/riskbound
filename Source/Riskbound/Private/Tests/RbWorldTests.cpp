// Automation tests for the living world through the UE subsystems (Riskbound.World.*).
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "RbAudioSubsystem.h"
#include "RbCampaignSubsystem.h"
#include "RbGameSubsystem.h"

namespace
{
constexpr EAutomationTestFlags WorldFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

struct FWorldSession
{
	UGameInstance* Instance = nullptr;
	URbGameSubsystem* Game = nullptr;
	URbCampaignSubsystem* Campaign = nullptr;

	FWorldSession()
	{
		Instance = NewObject<UGameInstance>(GEngine);
		Instance->AddToRoot();
		Instance->InitializeStandalone();
		Game = Instance->GetSubsystem<URbGameSubsystem>();
		Campaign = Instance->GetSubsystem<URbCampaignSubsystem>();
		Game->NewGame(ERbDifficulty::Professional);
	}

	~FWorldSession()
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
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRbWorldContentTest, "Riskbound.World.ShippedContentLoadsAndInitialises", WorldFlags)
bool FRbWorldContentTest::RunTest(const FString& Parameters)
{
	FWorldSession S;
	if (!TestNotNull(TEXT("campaign subsystem"), S.Campaign)) return false;
	for (const FString& E : S.Campaign->GetLoadErrors()) AddError(E);
	TestTrue(TEXT("people loaded"), S.Campaign->GetPeople().Num() >= 12);
	const FRbWorldView W = S.Campaign->GetWorldView();
	TestTrue(TEXT("world view valid"), W.bValid);
	TestEqual(TEXT("stress starts at 20"), W.Stress, 20);
	TestEqual(TEXT("share index starts at 100"), W.ShareIndex, 100.0);
	TestEqual(TEXT("story has ten clues"), W.CluesTotal, 10);
	const TArray<FRbPersonView> People = S.Campaign->GetPeople();
	const FRbPersonView* Priya = People.FindByPredicate([](const FRbPersonView& P) { return P.Id == TEXT("priya"); });
	TestTrue(TEXT("Priya starts friendly"), Priya && Priya->Relation == 20 && Priya->RelationLabel == TEXT("Friendly"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRbWorldInboxTest, "Riskbound.World.InboxDeliversAndReplies", WorldFlags)
bool FRbWorldInboxTest::RunTest(const FString& Parameters)
{
	FWorldSession S;
	if (!TestNotNull(TEXT("campaign subsystem"), S.Campaign)) return false;
	rb::GameState& G = S.Game->EditState();
	G.Career.RecordMission("M001", true, 100);
	G.Career.RecordMission("M002", true, 100);
	S.Campaign->RunWorldUpdate({}, 1);
	const TArray<FRbInboxMessageView> Inbox = S.Campaign->GetInbox();
	TestTrue(TEXT("messages delivered"), Inbox.Num() >= 2);
	const FRbInboxMessageView* Dinner = Inbox.FindByPredicate([](const FRbInboxMessageView& M) { return M.Subject == TEXT("Thursday?"); });
	if (!TestNotNull(TEXT("dinner invitation"), Dinner)) return false;
	TestTrue(TEXT("needs a reply, sorted first"), Dinner->bNeedsReply && Inbox[0].bNeedsReply);
	TestTrue(TEXT("no-reply default is not offered as a button"), !Dinner->Choices.ContainsByPredicate([](const FRbMissionOptionView& C) { return C.Label.StartsWith(TEXT("(")); }));
	FString Error;
	TestFalse(TEXT("unknown choice refused"), S.Campaign->ReplyToMessage(Dinner->Id, TEXT("nope"), Error));
	TestTrue(TEXT("reply accepted"), S.Campaign->ReplyToMessage(Dinner->Id, TEXT("go"), Error));
	const TArray<FRbInboxMessageView> After = S.Campaign->GetInbox();
	const FRbInboxMessageView* Done = After.FindByPredicate([&](const FRbInboxMessageView& M) { return M.Id == Dinner->Id; });
	TestTrue(TEXT("reply recorded with outcome"), Done && !Done->bNeedsReply && Done->ChosenLabel == TEXT("Go, phone off") && !Done->Outcome.IsEmpty());
	const TArray<FRbPersonView> People = S.Campaign->GetPeople();
	const FRbPersonView* Sam = People.FindByPredicate([](const FRbPersonView& P) { return P.Id == TEXT("sam"); });
	TestTrue(TEXT("Sam appreciated it"), Sam && Sam->Relation == 50);
	TestTrue(TEXT("stress came down"), S.Campaign->GetWorldView().Stress < 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRbAudioTest, "Riskbound.World.AudioLoadsAndPicksMood", WorldFlags)
bool FRbAudioTest::RunTest(const FString& Parameters)
{
	FWorldSession S;
	URbAudioSubsystem* Audio = S.Instance->GetSubsystem<URbAudioSubsystem>();
	if (!TestNotNull(TEXT("audio subsystem"), Audio)) return false;
	TestTrue(TEXT("sound assets load"), Audio->LoadedSoundCount() >= 20);
	TestEqual(TEXT("everyday work is focus music"), Audio->DesiredMood(), ERbMusicMood::Focus);
	S.Game->EditState().World.Stress = 85;
	TestEqual(TEXT("high stress is tension"), Audio->DesiredMood(), ERbMusicMood::Tension);
	return true;
}

#endif
