// Graybox of Valtren Coldline Logistics HQ built from code: seven labelled rooms off a central corridor,
// lights, furniture, and the Mission 001 interactables placed per Docs/GameDesign.md.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RbValtrenHQ.generated.h"

class ARbInteractable;
class UStaticMesh;

UCLASS()
class RISKBOUND_API ARbValtrenHQ : public AActor
{
	GENERATED_BODY()

public:
	ARbValtrenHQ();

	// Builds geometry and spawns interactables. Safe to call once per world.
	void Build();

	FTransform GetPlayerStart() const;

	// Camera positions for the screenshot/performance tour: the corridor plus one view per room.
	void GetTourViewpoints(TArray<FString>& OutNames, TArray<FTransform>& OutViews) const;

	struct FRoom
	{
		FString Name;
		FBox2D Bounds;     // cm, floor plan
		bool bNorthSide;   // door faces the corridor at y = +CorridorHalfWidth (north) or -CorridorHalfWidth
	};
	static const TArray<FRoom>& Rooms();

private:
	void AddBox(const FVector& Center, const FVector& Size, const FLinearColor& Color);
	void AddWallX(float Y, float X0, float X1, const TArray<float>& DoorCentersX);
	void AddWallY(float X, float Y0, float Y1);
	void AddLabel(const FString& Text, const FVector& Location, float Yaw, float Size, const FColor& Color);
	void AddRoomLight(const FVector& Location);
	void BuildRooms();
	void BuildLighting();
	void PlaceInteractables();
	template <class T>
	T* SpawnInteractable(const FString& Id, const FVector& Location, float Yaw, UStaticMesh* Mesh, const FVector& Scale, const FLinearColor& Color, const FString& Label, float LabelHeight);

	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Cube;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Cylinder;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Sphere;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BaseMaterial;

	int32 ComponentCount = 0;
	bool bBuilt = false;
};
