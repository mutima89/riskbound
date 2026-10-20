#include "RbInteractable.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RbGameSubsystem.h"
#include "RbHUD.h"

#define LOCTEXT_NAMESPACE "Riskbound"

ARbInteractable::ARbInteractable()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Mesh;

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Mesh);
	Label->SetAbsolute(false, false, true); // keep text readable regardless of mesh scale
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(9.0f);
	Label->SetTextRenderColor(FColor(240, 240, 240));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARbInteractable::SetAppearance(UStaticMesh* InMesh, const FVector& Scale, const FLinearColor& Color, const FString& LabelText, float LabelHeight)
{
	BaseColor = Color;
	Mesh->SetStaticMesh(InMesh);
	Mesh->SetWorldScale3D(Scale);
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		Material = Mesh->CreateDynamicMaterialInstance(0, Base);
		Material->SetVectorParameterValue(TEXT("Color"), BaseColor);
	}
	Label->SetText(FText::FromString(LabelText));
	Label->SetRelativeLocation(FVector(0.0f, 0.0f, LabelHeight / FMath::Max(Scale.Z, 0.01f)));
}

void ARbInteractable::SetFocused(bool bFocused)
{
	if (Material)
	{
		const FLinearColor Highlight = BaseColor * 0.4f + FLinearColor(1.0f, 0.85f, 0.3f) * 0.6f;
		Material->SetVectorParameterValue(TEXT("Color"), bFocused ? Highlight : BaseColor);
	}
	Label->SetTextRenderColor(bFocused ? FColor(255, 220, 90) : FColor(240, 240, 240));
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

FText ARbNpc::GetPrompt() const
{
	return FText::Format(LOCTEXT("TalkPrompt", "Talk to {0}"), FText::FromString(DisplayName));
}

void ARbNpc::Interact(ARbHUD* Hud)
{
	Hud->TalkToManager();
}

#undef LOCTEXT_NAMESPACE
