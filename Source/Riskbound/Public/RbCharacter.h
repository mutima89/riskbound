// First-person analyst. Enhanced Input actions and the mapping context are created in code,
// so there are no input assets to keep in sync.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RbCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class URbInteractionComponent;
struct FInputActionValue;

UCLASS()
class RISKBOUND_API ARbCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARbCharacter();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	URbInteractionComponent* GetInteraction() const { return Interaction; }

	// Physical use before a screen opens (Docs/GameDesign.md "Physical interaction"): the view turns to the
	// target; documents are lifted towards the camera, screens are leaned into. OnDone opens the screen.
	// Instant under Reduced motion. The lean eases back once no screen is open.
	void BeginUse(class ARbInteractable* Target, TFunction<void()> OnDone);
	bool IsUsing() const { return UseT >= 0.0f; }

	// Base look rate before the player's sensitivity multiplier.
	UPROPERTY(EditAnywhere, Category = "Riskbound")
	float LookRate = 0.5f;

private:
	void BuildInput();
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnInteract();
	void OnPause();
	void OnJournal();
	void OnInbox();
	void OnPhoto();
	void OnPadLook(const FInputActionValue& Value);
	void OnShot();
	virtual void Tick(float DeltaSeconds) override;
	void TickUse(float DeltaSeconds);
	float UseT = -1.0f;            // 0..1 while a use move plays
	float UseLength = 0.35f;
	FRotator UseFrom, UseTo;
	FVector LeanTarget = FVector::ZeroVector; // camera offset in pawn space at full lean
	float Lean = 0.0f;             // 0..1 current lean
	bool bLeaning = false;
	FTransform PropFrom;
	TFunction<void()> UseDone;
	UPROPERTY(Transient) TObjectPtr<class UStaticMeshComponent> HeldProp;
	float StrideDistance = 0.0f;
	static constexpr float StrideLength = 150.0f;
	float Sensitivity() const;

	UPROPERTY(VisibleAnywhere, Category = "Riskbound")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Riskbound")
	TObjectPtr<URbInteractionComponent> Interaction;

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PauseAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JournalAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> InboxAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PhotoAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PadLookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ShotAction;
};
