#include "RbInteractable.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "RbGameSubsystem.h"
#include "RbHUD.h"

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

ARbNpc::ARbNpc()
{
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Mesh);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetAbsolute(false, false, true);
}

void ARbNpc::SetBody(USkeletalMesh* BodyMesh, UAnimSequence* Idle, UMaterialInterface* Outfit, float MeshYaw)
{
	if (!BodyMesh) return;
	Mesh->SetVisibility(false); // the capsule stays for collision and the interaction trace
	Body->SetSkeletalMesh(BodyMesh);
	// The capsule is centred on the actor; the mannequin's origin is at its feet.
	Body->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -90.0f / FMath::Max(GetActorScale3D().Z, 0.01f)), FRotator(0.0f, MeshYaw, 0.0f));
	if (Outfit)
	{
		for (int32 i = 0; i < Body->GetNumMaterials(); ++i) Body->SetMaterial(i, Outfit);
	}
	if (Idle)
	{
		Body->PlayAnimation(Idle, true);
	}
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
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Mesh);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetAbsolute(false, false, true);
}

void ARbCaseMarker::SetBody(USkeletalMesh* BodyMesh, UAnimSequence* Idle, UMaterialInterface* Outfit, float MeshYaw)
{
	if (!BodyMesh) return;
	Mesh->SetVisibility(false);
	Body->SetSkeletalMesh(BodyMesh);
	Body->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -90.0f / FMath::Max(GetActorScale3D().Z, 0.01f)), FRotator(0.0f, MeshYaw, 0.0f));
	if (Outfit)
	{
		for (int32 i = 0; i < Body->GetNumMaterials(); ++i) Body->SetMaterial(i, Outfit);
	}
	if (Idle) Body->PlayAnimation(Idle, true);
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
