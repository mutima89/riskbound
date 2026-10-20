#include "RbGameMode.h"

#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "RbCharacter.h"
#include "RbHUD.h"
#include "RbValtrenHQ.h"
#include "RbSelfTest.h"
#include "TimerManager.h"

ARbGameMode::ARbGameMode()
{
	DefaultPawnClass = ARbCharacter::StaticClass();
	HUDClass = ARbHUD::StaticClass();
}

ARbValtrenHQ* ARbGameMode::EnsureHQ()
{
	if (HQ) return HQ;
	for (TActorIterator<ARbValtrenHQ> It(GetWorld()); It; ++It)
	{
		HQ = *It; // placed in the map by hand
	}
	if (!HQ)
	{
		HQ = GetWorld()->SpawnActor<ARbValtrenHQ>();
	}
	HQ->Build();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	StartSpot = GetWorld()->SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(), HQ->GetPlayerStart(), Params);
	return HQ;
}

AActor* ARbGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	EnsureHQ();
	return StartSpot ? StartSpot.Get() : Super::FindPlayerStart_Implementation(Player, IncomingName);
}

void ARbGameMode::BeginPlay()
{
	Super::BeginPlay();
	EnsureHQ();
	if (RbSelfTest::IsRequested())
	{
		FTimerHandle Handle;
		TWeakObjectPtr<UWorld> World(GetWorld());
		GetWorldTimerManager().SetTimer(Handle, [World]()
		{
			if (World.IsValid()) RbSelfTest::Run(World.Get());
		}, 1.0f, false);
	}
}
