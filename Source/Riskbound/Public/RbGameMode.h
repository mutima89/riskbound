// Spawns the HQ graybox, the first-person character and the HUD; starts the self-test if requested.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RbGameMode.generated.h"

class ARbValtrenHQ;

UCLASS()
class RISKBOUND_API ARbGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARbGameMode();

	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;
	virtual void BeginPlay() override;

	ARbValtrenHQ* EnsureHQ();

private:
	UPROPERTY(Transient)
	TObjectPtr<ARbValtrenHQ> HQ;

	UPROPERTY(Transient)
	TObjectPtr<AActor> StartSpot;
};
