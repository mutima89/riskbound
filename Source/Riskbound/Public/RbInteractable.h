// Anything the player can focus and use. Carries an id that maps to a sim call; no mission logic here.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RbInteractable.generated.h"

class ARbHUD;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(Abstract)
class RISKBOUND_API ARbInteractable : public AActor
{
	GENERATED_BODY()

public:
	ARbInteractable();

	// Mesh, scale and colour for the graybox; label floats above the object.
	void SetAppearance(UStaticMesh* Mesh, const FVector& Scale, const FLinearColor& Color, const FString& LabelText, float LabelHeight);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Riskbound")
	FString InteractableId;

	virtual FText GetPrompt() const PURE_VIRTUAL(ARbInteractable::GetPrompt, return FText::GetEmpty(););
	virtual void Interact(ARbHUD* Hud) PURE_VIRTUAL(ARbInteractable::Interact, );

	void SetFocused(bool bFocused);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Riskbound")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Riskbound")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	FLinearColor BaseColor = FLinearColor::White;
};

// A document or log the player examines: ViewEvidence(InteractableId).
UCLASS()
class RISKBOUND_API ARbEvidenceProp : public ARbInteractable
{
	GENERATED_BODY()

public:
	virtual FText GetPrompt() const override;
	virtual void Interact(ARbHUD* Hud) override;
};

// The player's desk: inbox, risk register and the assessment form.
UCLASS()
class RISKBOUND_API ARbWorkstation : public ARbInteractable
{
	GENERATED_BODY()

public:
	virtual FText GetPrompt() const override;
	virtual void Interact(ARbHUD* Hud) override;
};

// Priya Okafor: briefing (MeetManager), reminders, and the debrief.
UCLASS()
class RISKBOUND_API ARbNpc : public ARbInteractable
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Riskbound")
	FString DisplayName = TEXT("Priya Okafor");

	virtual FText GetPrompt() const override;
	virtual void Interact(ARbHUD* Hud) override;
};
