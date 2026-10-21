// Anything the player can focus and use. Carries an id that maps to a sim call; no mission logic here.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RbInteractable.generated.h"

class ARbHUD;
class UAnimSequence;
class UMaterialInterface;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(Abstract)
class RISKBOUND_API ARbInteractable : public AActor
{
	GENERATED_BODY()

public:
	ARbInteractable();

	// Mesh, scale and optional material override (nullptr keeps the mesh's own materials).
	void SetAppearance(UStaticMesh* InMesh, const FVector& Scale, UMaterialInterface* Material = nullptr);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Riskbound")
	FString InteractableId;

	virtual FText GetPrompt() const PURE_VIRTUAL(ARbInteractable::GetPrompt, return FText::GetEmpty(););
	virtual void Interact(ARbHUD* Hud) PURE_VIRTUAL(ARbInteractable::Interact, );

	// Focus feedback: a fresnel overlay on every mesh of the actor.
	void SetFocused(bool bFocused);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Riskbound")
	TObjectPtr<UStaticMeshComponent> Mesh;
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

// In-person casework for the active campaign mission (spawned and removed by ARbValtrenHQ):
// a document to examine or a person to interview at a location, or the boardroom lectern where
// board papers are presented.
UENUM()
enum class ERbCaseMarkerKind : uint8
{
	Document,
	Interview,
	Lectern
};

UCLASS()
class RISKBOUND_API ARbCaseMarker : public ARbInteractable
{
	GENERATED_BODY()

public:
	ARbCaseMarker();

	ERbCaseMarkerKind Kind = ERbCaseMarkerKind::Document;
	FString MissionId;
	FString EvidenceId;
	FString Where;
	FString Title;      // document title, person's name, or mission title (lectern)

	// Interviews use the NPC body; documents and the lectern use the static mesh.
	void SetBody(USkeletalMesh* BodyMesh, UAnimSequence* Idle, UMaterialInterface* Outfit, float MeshYaw);

	virtual FText GetPrompt() const override;
	virtual void Interact(ARbHUD* Hud) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Riskbound")
	TObjectPtr<USkeletalMeshComponent> Body;
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

// Priya Okafor: briefing (MeetManager), reminders, and the debrief. The static mesh is an invisible
// collision capsule for the interaction trace; the visible body is a skeletal mesh playing an idle.
UCLASS()
class RISKBOUND_API ARbNpc : public ARbInteractable
{
	GENERATED_BODY()

public:
	ARbNpc();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Riskbound")
	FString DisplayName = TEXT("Priya Okafor");

	// Body mesh, looping idle and an outfit material for every slot (any may be null).
	void SetBody(USkeletalMesh* BodyMesh, UAnimSequence* Idle, UMaterialInterface* Outfit, float MeshYaw);

	virtual FText GetPrompt() const override;
	virtual void Interact(ARbHUD* Hud) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Riskbound")
	TObjectPtr<USkeletalMeshComponent> Body;
};
