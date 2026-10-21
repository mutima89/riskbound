#include "RbInteractionComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "RbHUD.h"
#include "RbInteractable.h"

URbInteractionComponent::URbInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URbInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		SetFocus(nullptr);
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector End = ViewLocation + ViewRotation.Vector() * Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RbInteract), false, GetOwner());
	FHitResult Hit;
	ARbInteractable* Candidate = nullptr;
	// A thin sphere sweep rather than a line: small props (binders, clipboards) are easy to target
	// without pixel-perfect aim, and walls still block it.
	if (GetWorld()->SweepSingleByChannel(Hit, ViewLocation, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(9.0f), Params))
	{
		Candidate = Cast<ARbInteractable>(Hit.GetActor());
	}
	SetFocus(Candidate);
}

void URbInteractionComponent::SetFocus(ARbInteractable* NewFocus)
{
	if (Focused.Get() == NewFocus) return;
	if (ARbInteractable* Old = Focused.Get())
	{
		Old->SetFocused(false);
	}
	Focused = NewFocus;
	if (NewFocus)
	{
		NewFocus->SetFocused(true);
	}
}

FText URbInteractionComponent::GetPromptText() const
{
	const ARbInteractable* Target = Focused.Get();
	return Target ? Target->GetPrompt() : FText::GetEmpty();
}

bool URbInteractionComponent::Interact()
{
	ARbInteractable* Target = Focused.Get();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	ARbHUD* Hud = PC ? Cast<ARbHUD>(PC->GetHUD()) : nullptr;
	if (!Target || !Hud) return false;
	Target->Interact(Hud);
	return true;
}
