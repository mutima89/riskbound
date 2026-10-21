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
	void OnShot();
	virtual void Tick(float DeltaSeconds) override;
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
	UPROPERTY(Transient) TObjectPtr<UInputAction> ShotAction;
};
