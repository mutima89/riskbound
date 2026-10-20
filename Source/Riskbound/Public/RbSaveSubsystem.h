// Wraps rb::SaveGameToFile / rb::LoadGameFromFile at Saved/SaveGames/<slot>.rbsave.
// Recovery rules (backup, .corrupt) live in RiskboundSim; this class only picks the path and reports.
#pragma once

#include "CoreMinimal.h"
#include "RbTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RbSaveSubsystem.generated.h"

UCLASS()
class RISKBOUND_API URbSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Default "slot1". Self-test and automation tests use their own slots so player saves are never touched.
	UFUNCTION(BlueprintCallable, Category = "Riskbound")
	void SetSlotName(const FString& InSlotName) { SlotName = InSlotName; }

	UFUNCTION(BlueprintPure, Category = "Riskbound")
	FString GetSlotPath() const;

	// True when the primary or its backup exists (Continue is worth offering).
	UFUNCTION(BlueprintPure, Category = "Riskbound")
	bool HasSave() const;

	UFUNCTION(BlueprintCallable, Category = "Riskbound")
	bool Save(FString& OutError);

	// Checkpoint save; reports success or failure through URbGameSubsystem::OnNotice.
	bool Autosave();

	UFUNCTION(BlueprintCallable, Category = "Riskbound")
	ERbLoadOutcome Load(FString& OutMessage);

private:
	FString SlotName = TEXT("slot1");
};
