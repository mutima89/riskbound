#include "RbInteractable.h"

#include "Animation/AnimSequence.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "RbGameSubsystem.h"
#include "RbHUD.h"
#include "RbPerson.h"

#define LOCTEXT_NAMESPACE "Riskbound"

static UMaterialInterface* HighlightMaterial()
{
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (!Cached.IsValid())
	{
		Cached = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Riskbound/Materials/Instances/MI_Highlight.MI_Highlight"));
	}
	return Cached.Get();
}

ARbInteractable::ARbInteractable()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Mesh;
}

void ARbInteractable::SetAppearance(UStaticMesh* InMesh, const FVector& Scale, UMaterialInterface* Material)
{
	Mesh->SetStaticMesh(InMesh);
	Mesh->SetWorldScale3D(Scale);
	if (Material)
	{
		for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i) Mesh->SetMaterial(i, Material);
	}
}

void ARbInteractable::SetFocused(bool bFocused)
{
	UMaterialInterface* Overlay = bFocused ? HighlightMaterial() : nullptr;
	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* M : Meshes)
	{
		if (M->IsVisible()) M->SetOverlayMaterial(Overlay);
	}
}

FVector ARbInteractable::GetFocusPoint() const
{
	return Mesh ? Mesh->Bounds.Origin : GetActorLocation();
}

UStaticMesh* ARbInteractable::GetPickUpMesh(FTransform& OutFrom) const
{
	UStaticMesh* M = Mesh ? Mesh->GetStaticMesh() : nullptr;
	if (!M) return nullptr;
	OutFrom = Mesh->GetComponentTransform();
	// Anything bigger than a binder (a filing cabinet) hands over a folder from its top drawer.
	if (Mesh->Bounds.BoxExtent.GetMax() > 40.0f)
	{
		static TWeakObjectPtr<UStaticMesh> Folder;
		if (!Folder.IsValid()) Folder = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Riskbound/Models/office_notepads/SM_office_notepads.SM_office_notepads"));
		if (!Folder.IsValid()) return nullptr;
		const FBoxSphereBounds& B = Mesh->Bounds;
		OutFrom = FTransform(GetActorRotation(), FVector(B.Origin.X, B.Origin.Y, B.Origin.Z + B.BoxExtent.Z - 6.0f), FVector(0.3f));
		return Folder.Get();
	}
	return M;
}

ERbUseStyle ARbEvidenceProp::GetUseStyle() const
{
	// Terminals (a thin screen slab) are leaned into; everything else is picked up.
	const FVector S = Mesh ? Mesh->GetComponentScale() : FVector::OneVector;
	return S.X < 0.02f && S.Y > 0.3f ? ERbUseStyle::Screen : ERbUseStyle::PickUp;
}

FText ARbEvidenceProp::GetPrompt() const
{
	FString Title = InteractableId;
	if (const URbGameSubsystem* Game = GetGameInstance()->GetSubsystem<URbGameSubsystem>())
	{
		Title = Game->GetEvidenceItem(InteractableId).Title;
	}
	return FText::Format(LOCTEXT("ExaminePrompt", "Examine: {0}"), FText::FromString(Title));
}

void ARbEvidenceProp::Interact(ARbHUD* Hud)
{
	Hud->OpenEvidence(InteractableId);
}

FText ARbWorkstation::GetPrompt() const
{
	return LOCTEXT("WorkstationPrompt", "Use workstation: inbox, risk register, assessment");
}

void ARbWorkstation::Interact(ARbHUD* Hud)
{
	Hud->OpenWorkstation();
}

// Shared by Priya and interviewees: a person standing in the invisible capsule.
static void DressBody(AActor* Actor, UStaticMeshComponent* Capsule, URbPersonComponent* Body, const RbCast::FLook& Look, UMaterialInterface* Outfit, const FString& PersonId)
{
	Capsule->SetVisibility(false); // the capsule stays for collision and the interaction trace
	// The capsule is centred on the actor; the body's origin is at its feet.
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f / FMath::Max(Actor->GetActorScale3D().Z, 0.01f)));
	Body->Setup(Look, PersonId, Actor->GetActorRotation().Yaw, GetTypeHash(PersonId) ^ GetTypeHash(Actor->GetActorLocation()));
	if (Outfit)
	{
		for (int32 i = 0; i < Body->GetNumMaterials(); ++i) Body->SetMaterial(i, Outfit);
	}
}

ARbNpc::ARbNpc()
{
	Body = CreateDefaultSubobject<URbPersonComponent>(TEXT("Body"));
	Body->SetupAttachment(Mesh);
	Body->SetAbsolute(false, false, true);
}

void ARbNpc::SetBody(const RbCast::FLook& Look, UMaterialInterface* Outfit, const FString& PersonId)
{
	if (!Look.Mesh) return;
	DressBody(this, Mesh, Body, Look, Outfit, PersonId);
}

FVector ARbNpc::GetFocusPoint() const
{
	return Body && Body->GetSkinnedAsset() ? Body->GetEyeLocation() : GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
}

FText ARbNpc::GetPrompt() const
{
	return FText::Format(LOCTEXT("TalkPrompt", "Talk to {0}"), FText::FromString(DisplayName));
}

void ARbNpc::Interact(ARbHUD* Hud)
{
	Hud->TalkToManager();
}

ARbCaseMarker::ARbCaseMarker()
{
	Body = CreateDefaultSubobject<URbPersonComponent>(TEXT("Body"));
	Body->SetupAttachment(Mesh);
	Body->SetAbsolute(false, false, true);
}

void ARbCaseMarker::SetBody(const RbCast::FLook& Look, UMaterialInterface* Outfit, const FString& PersonId)
{
	if (!Look.Mesh) return;
	bHasBody = true;
	DressBody(this, Mesh, Body, Look, Outfit, PersonId);
}

ERbUseStyle ARbCaseMarker::GetUseStyle() const
{
	switch (Kind)
	{
	case ERbCaseMarkerKind::Interview: return ERbUseStyle::Person;
	case ERbCaseMarkerKind::Lectern: return ERbUseStyle::Screen;
	default: return ERbUseStyle::PickUp;
	}
}

FVector ARbCaseMarker::GetFocusPoint() const
{
	if (Kind == ERbCaseMarkerKind::Interview && bHasBody) return Body->GetEyeLocation();
	if (Kind == ERbCaseMarkerKind::Lectern) return GetActorLocation() + FVector(0.0f, 0.0f, 50.0f); // the papers on the slope
	return ARbInteractable::GetFocusPoint();
}

URbPersonComponent* ARbCaseMarker::GetPerson() const
{
	return bHasBody ? Body.Get() : nullptr;
}

FText ARbCaseMarker::GetPrompt() const
{
	switch (Kind)
	{
	case ERbCaseMarkerKind::Interview: return FText::Format(LOCTEXT("CaseTalk", "Talk to {0} (case {1})"), FText::FromString(Title), FText::FromString(MissionId));
	case ERbCaseMarkerKind::Lectern: return FText::Format(LOCTEXT("CasePresentPrompt", "Present to the board: {0}"), FText::FromString(Title));
	default: return FText::Format(LOCTEXT("CaseExamine", "Examine: {0} (case {1})"), FText::FromString(Title), FText::FromString(MissionId));
	}
}

void ARbCaseMarker::Interact(ARbHUD* Hud)
{
	if (Kind == ERbCaseMarkerKind::Lectern) Hud->OpenCaseFiles(MissionId, false, Where);
	else Hud->ExamineInPerson(MissionId, EvidenceId, Where);
}

#undef LOCTEXT_NAMESPACE
