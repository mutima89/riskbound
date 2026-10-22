// Anything the player can focus and use. Carries an id that maps to a sim call; no mission logic here.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RbCast.h"
#include "RbInteractable.generated.h"

class ARbHUD;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class URbPersonComponent;

// How using a thing looks before its screen opens (Docs/GameDesign.md "Physical interaction"):
// documents are picked up, screens are leaned into, people are turned to face.
enum class ERbUseStyle : uint8
{
	None,
	PickUp,
	Screen,
	Person
};

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

	virtual ERbUseStyle GetUseStyle() const { return ERbUseStyle::None; }
	// Where the camera looks when using it (a person's eyes, a screen's centre, a document).
	virtual FVector GetFocusPoint() const;
	// The mesh lifted towards the camera for PickUp (a file taken out of a cabinet is a folder).
	virtual UStaticMesh* GetPickUpMesh(FTransform& OutFrom) const;
	// The person behind it, if any (lip-sync, gestures and reactions during conversations).
	virtual URbPersonComponent* GetPerson() const { return nullptr; }

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
	virtual ERbUseStyle GetUseStyle() const override;
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

	// Interviews use the person's body; documents and the lectern use the static mesh.
	void SetBody(const RbCast::FLook& Look, UMaterialInterface* Outfit, const FString& PersonId);

	virtual FText GetPrompt() const override;
	virtual void Interact(ARbHUD* Hud) override;
	virtual ERbUseStyle GetUseStyle() const override;
	virtual FVector GetFocusPoint() const override;
	virtual URbPersonComponent* GetPerson() const override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Riskbound")
	TObjectPtr<URbPersonComponent> Body;
	bool bHasBody = false;
};

// The player's desk: inbox, risk register and the assessment form.
UCLASS()
class RISKBOUND_API ARbWorkstation : public ARbInteractable
{
	GENERATED_BODY()

public:
	virtual FText GetPrompt() const override;
	virtual void Interact(ARbHUD* Hud) override;
	virtual ERbUseStyle GetUseStyle() const override { return ERbUseStyle::Screen; }
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

	// Cast body and an outfit material for every slot (Outfit may be null).
	void SetBody(const RbCast::FLook& Look, UMaterialInterface* Outfit, const FString& PersonId);

	virtual FText GetPrompt() const override;
	virtual void Interact(ARbHUD* Hud) override;
	virtual ERbUseStyle GetUseStyle() const override { return ERbUseStyle::Person; }
	virtual FVector GetFocusPoint() const override;
	virtual URbPersonComponent* GetPerson() const override { return Body; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Riskbound")
	TObjectPtr<URbPersonComponent> Body;
};
