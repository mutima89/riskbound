// Traces from the player's view each frame, tracks the focused interactable and its prompt.
#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "RbInteractionComponent.generated.h"

class ARbInteractable;

UCLASS(ClassGroup = (Riskbound), meta = (BlueprintSpawnableComponent))
class RISKBOUND_API URbInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URbInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Riskbound")
	float Range = 300.0f;

	ARbInteractable* GetFocused() const { return Focused.Get(); }

	// Empty when nothing is in focus.
	UFUNCTION(BlueprintPure, Category = "Riskbound")
	FText GetPromptText() const;

	// Uses the focused interactable. Returns false when nothing is focused.
	UFUNCTION(BlueprintCallable, Category = "Riskbound")
	bool Interact();

private:
	void SetFocus(ARbInteractable* NewFocus);

	TWeakObjectPtr<ARbInteractable> Focused;
};
