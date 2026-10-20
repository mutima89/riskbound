// Spawns the HQ graybox, the first-person character and the HUD; starts the self-test if requested.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RbGameMode.generated.h"

class ARbMeridianHQ;

UCLASS()
class RISKBOUND_API ARbGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARbGameMode();

	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;
	virtual void BeginPlay() override;

	ARbMeridianHQ* EnsureHQ();

private:
	UPROPERTY(Transient)
	TObjectPtr<ARbMeridianHQ> HQ;

	UPROPERTY(Transient)
	TObjectPtr<AActor> StartSpot;
};
