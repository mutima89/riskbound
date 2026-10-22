// Valtren Coldline Logistics head office, built from code: seven rooms off a central corridor,
// glazed facades with a city view, PBR materials, ceiling light panels, furniture and the
// Mission 001 interactables placed per Docs/GameDesign.md. Art rules: Docs/ArtDirection.md.
// Falls back to plain shapes if the imported art (Scripts/ImportAssets.bat) is missing.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RbCast.h"
#include "RbValtrenHQ.generated.h"

class ARbInteractable;
class UMaterialInterface;
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

	// Colleagues who come to find the player (Docs/GameDesign.md "Colleagues come to you"): walks PersonId
	// in from the corridor to talking distance, then calls OnArrive. Null when the cast is missing.
	class URbPersonComponent* SendColleague(const FString& PersonId, TFunction<void(class URbPersonComponent*)> OnArrive);
	// They walk back out (or vanish at once) when the talk is over.
	void DismissColleague(bool bImmediately = false);
	// Red alert light in the monitoring room and corridor when a crisis opens.
	void StartAlarm(float Seconds);
	// Walking route between two floor points through the room doors and the corridor.
	TArray<FVector> RouteBetween(const FVector& From, const FVector& To) const;

	struct FRoom
	{
		FString Name;
		FBox2D Bounds;     // cm, floor plan
		bool bNorthSide;   // door faces the corridor at y = +CorridorHalfWidth (north) or -CorridorHalfWidth
	};
	static const TArray<FRoom>& Rooms();

private:
	// Primitives
	UMaterialInterface* Mat(const TCHAR* Name);
	UStaticMesh* Model(const TCHAR* Id);
	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, const FVector& Location, float Yaw, const FVector& Scale, UMaterialInterface* Override = nullptr, bool bCollision = true);
	bool AddInstance(UStaticMesh* Mesh, const FVector& Location, float Yaw, const FVector& Scale, UMaterialInterface* Override = nullptr, bool bCollision = true);
	UStaticMeshComponent* AddBox(const FVector& Center, const FVector& Size, UMaterialInterface* Material, float Yaw = 0.0f, bool bCollision = true);
	class UTextRenderComponent* AddLabel(const FString& Text, const FVector& Location, float Yaw, float Size, const FColor& Color, float Roll = 0.0f, float Pitch = 0.0f);
	// The office reacts to the career: share price board and headline in reception, a news crawl in the
	// monitoring room, ambience (Docs/GameDesign.md "Living world"). Refreshed on a timer.
	void BuildLivingWorld();
	void UpdateLivingWorld();
	UPROPERTY(Transient) TObjectPtr<class UTextRenderComponent> ShareBoard;
	UPROPERTY(Transient) TObjectPtr<class UTextRenderComponent> ShareChange;
	UPROPERTY(Transient) TObjectPtr<class UTextRenderComponent> ReceptionHeadline;
	UPROPERTY(Transient) TObjectPtr<class UTextRenderComponent> NewsCrawl;
	FTimerHandle LivingTimer;
	int32 CrawlOffset = 0;
	// In-person casework for the active mission: document props, interviewees and the lectern.
	void RefreshCaseMarkers();
	FString MarkerMission;    // mission the markers were built for
	int32 MarkerViewed = -1;  // evidence viewed count when built (markers for seen evidence go)
	bool bMarkerCall = false; // built after the crisis board call started
	UPROPERTY(Transient) TArray<TObjectPtr<class ARbCaseMarker>> CaseMarkers;
	UPROPERTY(Transient) TArray<TObjectPtr<class UTextRenderComponent>> CaseLabels;
	UPROPERTY(Transient) TArray<TObjectPtr<class UStaticMeshComponent>> CaseStands;
	// Priya stands aside while her own interview marker takes her place, so she is never shown twice.
	UPROPERTY(Transient) TObjectPtr<class ARbNpc> PriyaNpc;
	// Living office: light follows the working week and the weather, staff numbers follow morale,
	// and the player's desk shows their title and trophies.
	void UpdateDaylight(float DeltaSeconds);
	void RefreshStaff(int32 Wanted);
	void RefreshOwnDesk(int32 Level, const FString& Title, int32 Trophies);
	UPROPERTY(Transient) TObjectPtr<class UDirectionalLightComponent> SunLight;
	UPROPERTY(Transient) TObjectPtr<class USkyLightComponent> SkyLightComp;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> SkyMaterial;
	UPROPERTY(Transient) TArray<TObjectPtr<class URbPersonComponent>> Staff;
	// Board members at the boardroom table while a case is to be presented there.
	void RefreshBoard(bool bInSession);
	UPROPERTY(Transient) TArray<TObjectPtr<class URbPersonComponent>> Board;
	bool bBoardSeated = false;
	class URbPersonComponent* AddPerson(const RbCast::FLook& Look, const FString& PersonId, const FVector& At, float FacingYaw, int32 Seed, UMaterialInterface* Outfit);
	void Commute(class URbPersonComponent* Person, const FVector& Home, const FVector& Away, bool bOutbound);
	UPROPERTY(Transient) TWeakObjectPtr<class URbPersonComponent> Messenger;
	FVector MessengerStart = FVector::ZeroVector;
	UPROPERTY(Transient) TArray<TObjectPtr<class UPointLightComponent>> AlarmLights;
	FTimerHandle AlarmTimer;
	float AlarmLeft = 0.0f;
	void TickAlarm();
	UPROPERTY(Transient) TArray<TObjectPtr<class USceneComponent>> DeskDressing;
	int32 StaffShown = -1;
	int32 DeskLevel = -1;
	int32 DeskTrophies = -1;
	float DayBlend = 1.0f;     // 0..1 progress towards the target light
	int32 DayWeek = -1;
	FRotator SunFrom, SunTo;
	float SunLuxFrom = 0.0f, SunLuxTo = 0.0f, SkyFrom = 0.0f, SkyTo = 0.0f;
	FLinearColor SunColorFrom, SunColorTo;
	void AddPanelLight(const FVector& Location, float Lumens, bool bShadows, const FLinearColor& Tint = FLinearColor(0.92f, 0.96f, 1.0f));

	// Architecture
	void BuildShell();
	void AddWallX(float Y, float X0, float X1, const TArray<float>& DoorCentersX, bool bGlass);
	void AddWallY(float X, float Y0, float Y1, bool bGlass);
	void AddFacadeX(float Y, float X0, float X1);
	void AddFacadeY(float X, float Y0, float Y1);
	void AddDoorSign(const FString& Text, float DoorX, bool bNorthSide);
	void BuildLighting();

	// Furniture
	void Desk(const FVector& Center, float Yaw, const FVector& Size, UMaterialInterface* Top);
	void Monitor(const FVector& Base, float Yaw, UMaterialInterface* Screen);
	void Chair(const FVector& Location, float Yaw, const TCHAR* ModelId = TEXT("modern_arm_chair_01"));
	void Plant(const FVector& Location, bool bLarge);
	void TaskChair(const FVector& Location, float Yaw);
	void Workplace(const FVector& DeskCenter, float Yaw, UMaterialInterface* Screen, bool bLamp, int32 Seed);
	void DeskLamp(const FVector& Base, float Yaw);
	void Shelf(const FVector& Center, float Yaw, float Width, int32 Seed);
	void ArtPrint(const FVector& WallPoint, float Yaw, const TCHAR* Print, const FVector2D& Size);
	void SmallProps(const FVector& Location, float Yaw, int32 Kind); // 0 bin, 1 coat stand, 2 printer, 3 water cooler
	void FurnishRooms();
	void AddEasterEggs(); // Docs/GameDesign.md "Easter eggs"; no prompts, not part of any mission
	void PlaceInteractables();
	template <class T>
	T* SpawnInteractable(const FString& Id, const FVector& Location, float Yaw, UStaticMesh* Mesh, const FVector& Scale, UMaterialInterface* Material);

	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Cube;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Cylinder;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Sphere;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(Transient) TMap<FString, TObjectPtr<class UInstancedStaticMeshComponent>> Pools;
	int32 ComponentCount = 0;
	int32 InstanceCount = 0;
	int32 LightCount = 0;
	bool bBuilt = false;
	bool bHasArt = false;
};
