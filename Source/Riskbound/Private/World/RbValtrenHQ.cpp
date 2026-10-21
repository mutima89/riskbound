#include "RbValtrenHQ.h"

#include "Animation/AnimSequence.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "RbCredits.h"
#include "RbInteractable.h"
#include "RbLog.h"

namespace
{
constexpr float BuildingMinX = 0.0f;
constexpr float BuildingMaxX = 4000.0f;
constexpr float BuildingHalfY = 1200.0f;
constexpr float CorridorHalfY = 200.0f;
constexpr float WallThickness = 16.0f;
constexpr float CeilingHeight = 300.0f;
constexpr float DoorWidth = 220.0f;
constexpr float DoorHeight = 230.0f;
constexpr float DeskHeight = 74.0f;
// Poly Haven models face +Y in UE after import; this turns "facing yaw" into mesh yaw.
constexpr float ModelYawOffset = -90.0f;

// Exposure is locked to a narrow band so rooms do not pump when looking at the windows.
constexpr float ExposureMinEV100 = 6.0f;
constexpr float ExposureMaxEV100 = 7.2f;
constexpr float SunLux = 1600.0f;
constexpr float SkyIntensity = 18.0f;
constexpr float PanelLumens = 4200.0f;
} // namespace

const TArray<ARbValtrenHQ::FRoom>& ARbValtrenHQ::Rooms()
{
	static const TArray<FRoom> R = {
		{TEXT("Reception"), FBox2D(FVector2D(0, 200), FVector2D(1000, 1200)), true},
		{TEXT("Junior analyst workspace"), FBox2D(FVector2D(1000, 200), FVector2D(2000, 1200)), true},
		{TEXT("Manager's office"), FBox2D(FVector2D(2000, 200), FVector2D(3000, 1200)), true},
		{TEXT("Meeting room"), FBox2D(FVector2D(3000, 200), FVector2D(4000, 1200)), true},
		{TEXT("Training area"), FBox2D(FVector2D(0, -1200), FVector2D(1000, -200)), false},
		{TEXT("Risk monitoring room"), FBox2D(FVector2D(1000, -1200), FVector2D(2000, -200)), false},
		{TEXT("Executive boardroom"), FBox2D(FVector2D(2000, -1200), FVector2D(4000, -200)), false},
	};
	return R;
}

ARbValtrenHQ::ARbValtrenHQ()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

FTransform ARbValtrenHQ::GetPlayerStart() const
{
	// Reception, facing the door to the corridor.
	return FTransform(FRotator(0.0f, -90.0f, 0.0f), FVector(500.0f, 760.0f, 100.0f));
}

void ARbValtrenHQ::GetTourViewpoints(TArray<FString>& OutNames, TArray<FTransform>& OutViews) const
{
	const float Eye = 165.0f;
	auto Add = [&](const FString& Name, const FVector& From, const FVector& To)
	{
		OutNames.Add(Name);
		OutViews.Add(FTransform((To - From).Rotation(), From));
	};
	Add(TEXT("Corridor"), FVector(120.0f, 0.0f, Eye), FVector(4000.0f, 0.0f, 120.0f));
	for (const FRoom& Room : Rooms())
	{
		const FVector2D C = Room.Bounds.GetCenter();
		const FVector2D S = Room.Bounds.GetSize();
		const float Dir = Room.bNorthSide ? 1.0f : -1.0f; // into the room from the corridor
		const float DoorY = Room.bNorthSide ? CorridorHalfY : -CorridorHalfY;
		Add(Room.Name, FVector(C.X - 0.36f * S.X, DoorY + Dir * 90.0f, Eye),
			FVector(C.X + 0.2f * S.X, C.Y + Dir * 0.2f * S.Y, 90.0f));
	}
	// Reverse view of the monitoring room: operator rows, acoustic panels, door wall.
	Add(TEXT("Risk monitoring room (operators)"), FVector(1950.0f, -1120.0f, Eye), FVector(1150.0f, -380.0f, 100.0f));
}

// ---------------------------------------------------------------------------- primitives

UMaterialInterface* ARbValtrenHQ::Mat(const TCHAR* Name)
{
	if (TObjectPtr<UMaterialInterface>* Found = Materials.Find(Name)) return *Found;
	const FString Path = FString::Printf(TEXT("/Game/Riskbound/Materials/Instances/%s.%s"), Name, Name);
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *Path);
	Materials.Add(Name, M);
	return M;
}

UStaticMesh* ARbValtrenHQ::Model(const TCHAR* Id)
{
	const FString Path = FString::Printf(TEXT("/Game/Riskbound/Models/%s/SM_%s.SM_%s"), Id, Id, Id);
	return LoadObject<UStaticMesh>(nullptr, *Path);
}

UStaticMeshComponent* ARbValtrenHQ::AddMesh(UStaticMesh* Mesh, const FVector& Location, float Yaw, const FVector& Scale, UMaterialInterface* Override, bool bCollision)
{
	if (!Mesh) return nullptr;
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, NAME_None);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(RootComponent);
	C->SetRelativeLocation(Location);
	C->SetRelativeRotation(FRotator(0.0f, Yaw, 0.0f));
	C->SetRelativeScale3D(Scale);
	if (!bCollision) C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Override)
	{
		for (int32 i = 0; i < C->GetNumMaterials(); ++i) C->SetMaterial(i, Override);
	}
	C->RegisterComponent();
	++ComponentCount;
	return C;
}

bool ARbValtrenHQ::AddInstance(UStaticMesh* Mesh, const FVector& Location, float Yaw, const FVector& Scale, UMaterialInterface* Override, bool bCollision)
{
	// One instanced component per (mesh, material, collision): a few hundred draws instead of thousands.
	if (!Mesh) return false;
	const FString Key = Mesh->GetPathName() + TEXT("|") + (Override ? Override->GetPathName() : FString()) + (bCollision ? TEXT("|c") : TEXT("|n"));
	TObjectPtr<UInstancedStaticMeshComponent>& Pool = Pools.FindOrAdd(Key);
	if (!Pool)
	{
		Pool = NewObject<UInstancedStaticMeshComponent>(this, NAME_None);
		Pool->SetMobility(EComponentMobility::Movable);
		Pool->SetStaticMesh(Mesh);
		Pool->SetupAttachment(RootComponent);
		if (!bCollision) Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Override)
		{
			for (int32 i = 0; i < Pool->GetNumMaterials(); ++i) Pool->SetMaterial(i, Override);
		}
		Pool->RegisterComponent();
		++ComponentCount;
	}
	Pool->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f), Location, Scale));
	++InstanceCount;
	return true;
}

UStaticMeshComponent* ARbValtrenHQ::AddBox(const FVector& Center, const FVector& Size, UMaterialInterface* Material, float Yaw, bool bCollision)
{
	AddInstance(Cube, Center, Yaw, Size / 100.0f, Material, bCollision); // the basic cube is 100 cm
	return nullptr;
}

void ARbValtrenHQ::AddLabel(const FString& Text, const FVector& Location, float Yaw, float Size, const FColor& Color, float Roll, float Pitch)
{
	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, NAME_None);
	Label->SetupAttachment(RootComponent);
	Label->SetRelativeLocation(Location);
	Label->SetRelativeRotation(FRotator(Pitch, Yaw, Roll));
	Label->SetText(FText::FromString(Text));
	Label->SetWorldSize(Size);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetTextRenderColor(Color);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->RegisterComponent();
	++ComponentCount;
}

void ARbValtrenHQ::AddPanelLight(const FVector& Location, float Lumens, bool bShadows, const FLinearColor& Tint)
{
	// Recessed linear LED: a slim emissive strip with a matching rect light.
	AddBox(FVector(Location.X, Location.Y, CeilingHeight - 0.5f), FVector(180.0f, 10.0f, 1.0f), Mat(TEXT("MI_LedStrip")), 0.0f, false);
	AddBox(FVector(Location.X, Location.Y, CeilingHeight + 0.2f), FVector(190.0f, 18.0f, 1.0f), Mat(TEXT("MI_Graphite")), 0.0f, false);
	URectLightComponent* Light = NewObject<URectLightComponent>(this, NAME_None);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(Location.X, Location.Y, CeilingHeight - 4.0f));
	Light->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	Light->SetIntensityUnits(ELightUnits::Lumens);
	Light->SetIntensity(Lumens);
	Light->SetSourceWidth(180.0f);
	Light->SetSourceHeight(10.0f);
	Light->SetAttenuationRadius(1100.0f);
	Light->SetLightColor(Tint);
	Light->SetCastShadows(bShadows);
	Light->RegisterComponent();
	++ComponentCount;
	++LightCount;
}

// ---------------------------------------------------------------------------- build

void ARbValtrenHQ::Build()
{
	if (bBuilt) return;
	bBuilt = true;
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!Cube || !Cylinder || !Sphere)
	{
		UE_LOG(LogRiskbound, Error, TEXT("Valtren HQ: engine basic shapes missing; is /Engine/BasicShapes cooked?"));
		return;
	}
	bHasArt = Mat(TEXT("MI_WallPlaster")) != nullptr;
	if (!bHasArt)
	{
		UE_LOG(LogRiskbound, Warning, TEXT("Valtren HQ: imported art missing (run Scripts/ImportAssets.bat); building plain shapes"));
	}
	BuildShell();
	BuildLighting();
	FurnishRooms();
	AddEasterEggs();
	PlaceInteractables();
	UE_LOG(LogRiskbound, Display, TEXT("Valtren HQ built: %d components, %d instances, %d lights, %d rooms, art %s"), ComponentCount, InstanceCount, LightCount, Rooms().Num(),
		bHasArt ? TEXT("yes") : TEXT("missing"));
}

void ARbValtrenHQ::AddWallX(float Y, float X0, float X1, const TArray<float>& DoorCentersX, bool bGlass)
{
	TArray<float> Doors = DoorCentersX;
	Doors.Sort();
	auto Segment = [this, Y, bGlass](float A, float B)
	{
		if (B - A < 1.0f) return;
		const float Mid = (A + B) * 0.5f;
		if (!bGlass)
		{
			AddBox(FVector(Mid, Y, CeilingHeight * 0.5f), FVector(B - A, WallThickness, CeilingHeight), Mat(TEXT("MI_WallPlaster")));
			for (float Side : {-1.0f, 1.0f})
			{
				AddBox(FVector(Mid, Y + Side * (WallThickness * 0.5f + 0.6f), 5.0f), FVector(B - A, 1.2f, 10.0f), Mat(TEXT("MI_Skirting")), 0.0f, false);
			}
			return;
		}
		// Glazed partition: frameless glass, slim graphite mullions, frosted manifestation band.
		AddBox(FVector(Mid, Y, CeilingHeight * 0.5f), FVector(B - A, 2.0f, CeilingHeight), Mat(TEXT("MI_Glass")));
		AddBox(FVector(Mid, Y, 140.0f), FVector(B - A, 2.4f, 18.0f), Mat(TEXT("MI_GlassFrosted")), 0.0f, false);
		AddBox(FVector(Mid, Y, 2.5f), FVector(B - A, 6.0f, 5.0f), Mat(TEXT("MI_Graphite")), 0.0f, false);
		for (float X = A; X <= B + 0.5f; X += FMath::Max(1.0f, (B - A) / FMath::Max(1.0f, FMath::RoundToFloat((B - A) / 150.0f))))
		{
			AddBox(FVector(X, Y, CeilingHeight * 0.5f), FVector(4.0f, 6.0f, CeilingHeight), Mat(TEXT("MI_Graphite")), 0.0f, false);
		}
	};
	float Start = X0;
	for (float Door : Doors)
	{
		Segment(Start, Door - DoorWidth * 0.5f);
		// Header above the opening and a slim frame.
		AddBox(FVector(Door, Y, (DoorHeight + CeilingHeight) * 0.5f), FVector(DoorWidth, WallThickness, CeilingHeight - DoorHeight),
			Mat(bGlass ? TEXT("MI_Graphite") : TEXT("MI_WallPlaster")));
		for (float Side : {-1.0f, 1.0f})
		{
			AddBox(FVector(Door + Side * (DoorWidth * 0.5f - 3.0f), Y, DoorHeight * 0.5f), FVector(6.0f, WallThickness + 4.0f, DoorHeight), Mat(TEXT("MI_Graphite")));
		}
		Start = Door + DoorWidth * 0.5f;
	}
	Segment(Start, X1);
}

void ARbValtrenHQ::AddWallY(float X, float Y0, float Y1, bool bGlass)
{
	const float Mid = (Y0 + Y1) * 0.5f;
	if (bGlass)
	{
		AddBox(FVector(X, Mid, CeilingHeight * 0.5f), FVector(2.0f, Y1 - Y0, CeilingHeight), Mat(TEXT("MI_Glass")));
		AddBox(FVector(X, Mid, 140.0f), FVector(2.4f, Y1 - Y0, 18.0f), Mat(TEXT("MI_GlassFrosted")), 0.0f, false);
		return;
	}
	AddBox(FVector(X, Mid, CeilingHeight * 0.5f), FVector(WallThickness, Y1 - Y0, CeilingHeight), Mat(TEXT("MI_WallPlaster")));
	for (float Side : {-1.0f, 1.0f})
	{
		AddBox(FVector(X + Side * (WallThickness * 0.5f + 0.6f), Mid, 5.0f), FVector(1.2f, Y1 - Y0, 10.0f), Mat(TEXT("MI_Skirting")), 0.0f, false);
	}
}

void ARbValtrenHQ::AddFacadeX(float Y, float X0, float X1)
{
	// Floor-to-ceiling curtain wall: low sill, glass, mullions every ~1.5 m, transom at door height.
	const float Mid = (X0 + X1) * 0.5f;
	AddBox(FVector(Mid, Y, 20.0f), FVector(X1 - X0, 24.0f, 40.0f), Mat(TEXT("MI_Graphite")));
	AddBox(FVector(Mid, Y, 170.0f), FVector(X1 - X0, 3.0f, 260.0f), Mat(TEXT("MI_Glass")));
	AddBox(FVector(Mid, Y, DoorHeight + 20.0f), FVector(X1 - X0, 10.0f, 5.0f), Mat(TEXT("MI_Graphite")), 0.0f, false);
	const int32 Bays = FMath::Max(1, FMath::RoundToInt((X1 - X0) / 150.0f));
	for (int32 i = 0; i <= Bays; ++i)
	{
		AddBox(FVector(X0 + (X1 - X0) * i / Bays, Y, CeilingHeight * 0.5f), FVector(6.0f, 14.0f, CeilingHeight), Mat(TEXT("MI_Graphite")), 0.0f, false);
	}
}

void ARbValtrenHQ::AddFacadeY(float X, float Y0, float Y1)
{
	const float Mid = (Y0 + Y1) * 0.5f;
	AddBox(FVector(X, Mid, 20.0f), FVector(24.0f, Y1 - Y0, 40.0f), Mat(TEXT("MI_Graphite")));
	AddBox(FVector(X, Mid, 170.0f), FVector(3.0f, Y1 - Y0, 260.0f), Mat(TEXT("MI_Glass")));
	AddBox(FVector(X, Mid, DoorHeight + 20.0f), FVector(10.0f, Y1 - Y0, 5.0f), Mat(TEXT("MI_Graphite")), 0.0f, false);
	const int32 Bays = FMath::Max(1, FMath::RoundToInt((Y1 - Y0) / 150.0f));
	for (int32 i = 0; i <= Bays; ++i)
	{
		AddBox(FVector(X, Y0 + (Y1 - Y0) * i / Bays, CeilingHeight * 0.5f), FVector(14.0f, 6.0f, CeilingHeight), Mat(TEXT("MI_Graphite")), 0.0f, false);
	}
}

void ARbValtrenHQ::AddDoorSign(const FString& Text, float DoorX, bool bNorthSide)
{
	// Small graphite plate with a teal edge beside the door, on the corridor side.
	const float WallFace = (bNorthSide ? CorridorHalfY : -CorridorHalfY) + (bNorthSide ? -1.0f : 1.0f) * (WallThickness * 0.5f + 1.5f);
	const float X = DoorX + DoorWidth * 0.5f + 70.0f;
	const float Yaw = bNorthSide ? -90.0f : 90.0f;
	AddBox(FVector(X, WallFace, 170.0f), FVector(110.0f, 2.0f, 28.0f), Mat(TEXT("MI_DeviceBlack")), 0.0f, false);
	AddBox(FVector(X - 53.0f, WallFace + (bNorthSide ? -1.2f : 1.2f), 170.0f), FVector(4.0f, 1.0f, 28.0f), Mat(TEXT("MI_TealAccent")), 0.0f, false);
	AddLabel(Text, FVector(X + 2.0f, WallFace + (bNorthSide ? -1.6f : 1.6f), 170.0f), Yaw, 7.5f, FColor(236, 238, 240));
}

void ARbValtrenHQ::BuildShell()
{
	const float Width = BuildingMaxX - BuildingMinX;
	// Structural slab, ceiling, and a floor finish per room.
	AddBox(FVector(Width * 0.5f, 0.0f, -10.0f), FVector(Width + 60.0f, 2.0f * BuildingHalfY + 60.0f, 20.0f), Mat(TEXT("MI_FloorPolished")));
	AddBox(FVector(Width * 0.5f, 0.0f, CeilingHeight + 5.0f), FVector(Width + 60.0f, 2.0f * BuildingHalfY + 60.0f, 10.0f), Mat(TEXT("MI_CeilingSoft")));
	const TMap<FString, const TCHAR*> Floors = {
		{TEXT("Reception"), TEXT("MI_FloorTerrazzo")}, {TEXT("Junior analyst workspace"), TEXT("MI_Carpet")},
		{TEXT("Manager's office"), TEXT("MI_FloorParquet")}, {TEXT("Meeting room"), TEXT("MI_Carpet")},
		{TEXT("Training area"), TEXT("MI_Carpet")}, {TEXT("Risk monitoring room"), TEXT("MI_FloorPolished")},
		{TEXT("Executive boardroom"), TEXT("MI_FloorParquet")}};

	TArray<float> NorthDoors;
	TArray<float> SouthDoors;
	for (const FRoom& Room : Rooms())
	{
		const FVector2D C = Room.Bounds.GetCenter();
		const FVector2D S = Room.Bounds.GetSize();
		AddBox(FVector(C.X, C.Y, 0.5f), FVector(S.X - WallThickness, S.Y - WallThickness, 1.0f), Mat(Floors.FindRef(Room.Name)), 0.0f, false);
		(Room.bNorthSide ? NorthDoors : SouthDoors).Add(C.X);
		AddDoorSign(Room.Name, C.X, Room.bNorthSide);
	}

	// Corridor walls: glass into the open-plan and meeting spaces, solid elsewhere.
	struct FSeg { float X0, X1; bool bGlass; };
	const FSeg North[] = {{0, 1000, false}, {1000, 2000, true}, {2000, 3000, true}, {3000, 4000, true}};
	const FSeg South[] = {{0, 1000, false}, {1000, 2000, false}, {2000, 4000, true}};
	for (const FSeg& S : North)
	{
		TArray<float> Doors;
		for (float D : NorthDoors) if (D > S.X0 && D < S.X1) Doors.Add(D);
		AddWallX(CorridorHalfY, S.X0, S.X1, Doors, S.bGlass);
	}
	for (const FSeg& S : South)
	{
		TArray<float> Doors;
		for (float D : SouthDoors) if (D > S.X0 && D < S.X1) Doors.Add(D);
		AddWallX(-CorridorHalfY, S.X0, S.X1, Doors, S.bGlass);
	}
	// Room dividers. The manager's office is glazed towards the analysts.
	AddWallY(1000.0f, CorridorHalfY, BuildingHalfY, false);
	AddWallY(2000.0f, CorridorHalfY, BuildingHalfY, true);
	AddWallY(3000.0f, CorridorHalfY, BuildingHalfY, false);
	AddWallY(1000.0f, -BuildingHalfY, -CorridorHalfY, false);
	AddWallY(2000.0f, -BuildingHalfY, -CorridorHalfY, false);

	// Glazed facades on all four sides.
	AddFacadeX(BuildingHalfY, BuildingMinX, BuildingMaxX);
	AddFacadeX(-BuildingHalfY, BuildingMinX, BuildingMaxX);
	AddFacadeY(BuildingMinX, -BuildingHalfY, BuildingHalfY);
	AddFacadeY(BuildingMaxX, -BuildingHalfY, BuildingHalfY);

	// Suspended ceiling grid: thin T-bars every 120 cm, a shade darker than the self-lit tiles so the grid reads softly.
	UMaterialInterface* GridMat = Mat(TEXT("MI_CeilingGrid"));
	for (float X = 60.0f; X < Width; X += 120.0f)
	{
		AddBox(FVector(X, 0.0f, CeilingHeight - 0.2f), FVector(0.8f, 2.0f * BuildingHalfY, 0.4f), GridMat, 0.0f, false);
	}
	for (float Y = -BuildingHalfY + 60.0f; Y < BuildingHalfY; Y += 120.0f)
	{
		AddBox(FVector(Width * 0.5f, Y, CeilingHeight - 0.2f), FVector(Width, 0.8f, 0.4f), GridMat, 0.0f, false);
	}
	// Feature: acoustic panels in the meeting room and boardroom, a teal strip along the corridor.
	AddBox(FVector(3000.0f + WallThickness * 0.5f + 2.0f, 700.0f, 150.0f), FVector(4.0f, 600.0f, 180.0f), Mat(TEXT("MI_AcousticPanel")), 0.0f, false);
	AddBox(FVector(2000.0f + WallThickness * 0.5f + 2.0f, -700.0f, 160.0f), FVector(4.0f, 700.0f, 160.0f), Mat(TEXT("MI_AcousticPanel")), 0.0f, false);
	AddBox(FVector(Width * 0.5f, 0.0f, CeilingHeight - 3.0f), FVector(Width - 40.0f, 8.0f, 2.0f), Mat(TEXT("MI_TealAccent")), 0.0f, false);
}

void ARbValtrenHQ::BuildLighting()
{
	// Late-morning sun through the facades.
	UDirectionalLightComponent* Sun = NewObject<UDirectionalLightComponent>(this, TEXT("Sun"));
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetupAttachment(RootComponent);
	Sun->SetRelativeRotation(FRotator(-32.0f, 62.0f, 0.0f));
	Sun->SetIntensity(SunLux);
	Sun->SetLightColor(FLinearColor(1.0f, 0.95f, 0.88f));
	Sun->RegisterComponent();

	// City HDRI: an unlit inside-out sphere for the window view, and the sky light's cubemap.
	if (UStaticMeshComponent* Sky = AddMesh(Sphere, FVector(2000.0f, 0.0f, 0.0f), 0.0f, FVector(800.0f), Mat(TEXT("MI_Sky")), false))
	{
		Sky->SetCastShadow(false);
		Sky->bAffectDistanceFieldLighting = false;
		Sky->bVisibleInRayTracing = false;
	}
	USkyLightComponent* SkyLight = NewObject<USkyLightComponent>(this, TEXT("SkyLight"));
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SetupAttachment(RootComponent);
	if (UTextureCube* SkyCube = LoadObject<UTextureCube>(nullptr, TEXT("/Game/Riskbound/HDRI/T_CanaryWharfSky.T_CanaryWharfSky")))
	{
		SkyLight->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
		SkyLight->SetCubemap(SkyCube);
	}
	SkyLight->SetIntensity(SkyIntensity);
	SkyLight->RegisterComponent();
	SkyLight->RecaptureSky();

	// Ceiling panels: a grid per room, one shadowed light per room keeps the cost down.
	for (const FRoom& Room : Rooms())
	{
		const FVector2D C = Room.Bounds.GetCenter();
		const FVector2D S = Room.Bounds.GetSize();
		const bool bDim = Room.Name == TEXT("Risk monitoring room");
		const int32 Cols = S.X > 1500.0f ? 3 : 2;
		for (int32 ix = 0; ix < Cols; ++ix)
		{
			for (int32 iy = 0; iy < 2; ++iy)
			{
				const FVector P(C.X + (ix - (Cols - 1) * 0.5f) * S.X / Cols, C.Y + (iy - 0.5f) * S.Y / 2.0f, 0.0f);
				AddPanelLight(P, bDim ? PanelLumens * 0.35f : PanelLumens, false, bDim ? FLinearColor(0.82f, 0.9f, 1.0f) : FLinearColor(0.93f, 0.96f, 1.0f));
			}
		}
	}
	for (float X = 400.0f; X < BuildingMaxX; X += 800.0f)
	{
		AddPanelLight(FVector(X, 0.0f, 0.0f), PanelLumens * 0.7f, false);
	}

	// Post process: locked exposure band, gentle bloom and vignette.
	UPostProcessComponent* Post = NewObject<UPostProcessComponent>(this, TEXT("Post"));
	Post->SetupAttachment(RootComponent);
	Post->bUnbound = true;
	FPostProcessSettings& P = Post->Settings;
	P.bOverride_AutoExposureMethod = true;
	P.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
	P.bOverride_AutoExposureMinBrightness = true;
	P.AutoExposureMinBrightness = ExposureMinEV100;
	P.bOverride_AutoExposureMaxBrightness = true;
	P.AutoExposureMaxBrightness = ExposureMaxEV100;
	P.bOverride_AutoExposureSpeedUp = true;
	P.AutoExposureSpeedUp = 2.0f;
	P.bOverride_AutoExposureSpeedDown = true;
	P.AutoExposureSpeedDown = 1.5f;
	// Trace reflections only on genuinely glossy surfaces (glass, screens, polished wood); cheaper elsewhere.
	P.bOverride_LumenMaxRoughnessToTraceReflections = true;
	P.LumenMaxRoughnessToTraceReflections = 0.35f;
	P.bOverride_LumenReflectionQuality = true;
	P.LumenReflectionQuality = 0.75f;
	P.bOverride_IndirectLightingIntensity = true;
	P.IndirectLightingIntensity = 1.4f; // stronger bounce off the pale walls and ceiling
	P.bOverride_BloomIntensity = true;
	P.BloomIntensity = 0.45f;
	P.bOverride_VignetteIntensity = true;
	P.VignetteIntensity = 0.3f;
	P.bOverride_LensFlareIntensity = true;
	P.LensFlareIntensity = 0.0f;
	P.bOverride_MotionBlurAmount = true;
	P.MotionBlurAmount = 0.2f;
	Post->RegisterComponent();
}

// ---------------------------------------------------------------------------- furniture

void ARbValtrenHQ::Desk(const FVector& Center, float Yaw, const FVector& Size, UMaterialInterface* Top)
{
	const FRotator R(0.0f, Yaw, 0.0f);
	auto At = [&](float X, float Y, float Z) { return Center + R.RotateVector(FVector(X, Y, 0.0f)) + FVector(0.0f, 0.0f, Z); };
	AddBox(At(0, 0, Size.Z - 1.5f), FVector(Size.X, Size.Y, 3.0f), Top, Yaw);
	for (float Side : {-1.0f, 1.0f})
	{
		AddBox(At(Side * (Size.X * 0.5f - 6.0f), 0, (Size.Z - 3.0f) * 0.5f), FVector(4.0f, Size.Y - 10.0f, Size.Z - 3.0f), Mat(TEXT("MI_Graphite")), Yaw);
	}
	AddBox(At(0, Size.Y * 0.5f - 8.0f, Size.Z * 0.6f), FVector(Size.X - 16.0f, 1.5f, Size.Z * 0.45f), Mat(TEXT("MI_Graphite")), Yaw, false);
}

void ARbValtrenHQ::Monitor(const FVector& Base, float Yaw, UMaterialInterface* Screen)
{
	// Screen faces local +X; Base is the point on the desk under the monitor. Mounted on a slim arm.
	const FRotator R(0.0f, Yaw, 0.0f);
	auto At = [&](float X, float Y, float Z) { return Base + R.RotateVector(FVector(X, Y, 0.0f)) + FVector(0.0f, 0.0f, Z); };
	AddBox(At(-14.0f, 0, 1.0f), FVector(12.0f, 12.0f, 2.0f), Mat(TEXT("MI_Graphite")), Yaw, false);
	AddBox(At(-14.0f, 0, 18.0f), FVector(3.0f, 3.0f, 34.0f), Mat(TEXT("MI_Chrome")), Yaw, false);
	AddBox(At(-7.0f, 0, 34.0f), FVector(14.0f, 3.0f, 3.0f), Mat(TEXT("MI_Chrome")), Yaw, false);
	AddBox(At(0, 0, 38.0f), FVector(2.5f, 62.0f, 36.0f), Mat(TEXT("MI_DeviceBlack")), Yaw);
	if (Screen)
	{
		AddBox(At(1.4f, 0, 38.5f), FVector(0.3f, 59.0f, 33.0f), Screen, Yaw, false);
	}
}

void ARbValtrenHQ::Chair(const FVector& Location, float Yaw, const TCHAR* ModelId)
{
	// The arm chair model's forward axis is opposite to the dining chair's.
	const float Extra = FCString::Strcmp(ModelId, TEXT("modern_arm_chair_01")) == 0 ? 180.0f : 0.0f;
	if (!AddInstance(Model(ModelId), Location, Yaw + ModelYawOffset + Extra, FVector(1.0f)))
	{
		TaskChair(Location, Yaw);
	}
}

void ARbValtrenHQ::TaskChair(const FVector& Location, float Yaw)
{
	// Five-star base, gas lift, upholstered seat and back; faces local +X.
	const FRotator R(0.0f, Yaw, 0.0f);
	auto At = [&](float X, float Y, float Z) { return Location + R.RotateVector(FVector(X, Y, 0.0f)) + FVector(0.0f, 0.0f, Z); };
	for (int32 i = 0; i < 5; ++i)
	{
		const float A = Yaw + i * 72.0f;
		const FVector Dir = FRotator(0.0f, A, 0.0f).Vector();
		AddBox(Location + Dir * 15.0f + FVector(0, 0, 6.0f), FVector(30.0f, 4.0f, 3.0f), Mat(TEXT("MI_Graphite")), A, false);
		AddBox(Location + Dir * 29.0f + FVector(0, 0, 2.5f), FVector(5.0f, 5.0f, 5.0f), Mat(TEXT("MI_DeviceBlack")), A, false);
	}
	AddInstance(Cylinder, At(0, 0, 23.0f), 0.0f, FVector(0.05f, 0.05f, 0.34f), Mat(TEXT("MI_Chrome")), false);
	AddBox(At(0, 0, 44.0f), FVector(48.0f, 48.0f, 8.0f), Mat(TEXT("MI_FabricDark")), Yaw);
	AddBox(At(-22.0f, 0, 72.0f), FVector(5.0f, 44.0f, 48.0f), Mat(TEXT("MI_FabricDark")), Yaw);
	AddBox(At(-24.0f, 0, 60.0f), FVector(3.0f, 10.0f, 30.0f), Mat(TEXT("MI_Graphite")), Yaw, false);
	for (float Side : {-1.0f, 1.0f})
	{
		AddBox(At(0, Side * 25.0f, 58.0f), FVector(26.0f, 4.0f, 3.0f), Mat(TEXT("MI_Graphite")), Yaw, false);
		AddBox(At(0, Side * 25.0f, 52.0f), FVector(3.0f, 3.0f, 10.0f), Mat(TEXT("MI_Graphite")), Yaw, false);
	}
}

void ARbValtrenHQ::Workplace(const FVector& DeskCenter, float Yaw, UMaterialInterface* Screen, bool bLamp, int32 Seed)
{
	// A lived-in desk; local +X points from the chair to the screen.
	const FRotator R(0.0f, Yaw, 0.0f);
	auto At = [&](float X, float Y, float Z) { return DeskCenter + R.RotateVector(FVector(X, Y, 0.0f)) + FVector(0.0f, 0.0f, Z); };
	Desk(DeskCenter, Yaw + 90.0f, FVector(160.0f, 80.0f, DeskHeight), Mat(TEXT("MI_WoodOak")));
	if (Screen) Monitor(At(18.0f, 0, DeskHeight), Yaw + 180.0f, Screen);
	AddBox(At(-12.0f, 0, DeskHeight + 1.0f), FVector(15.0f, 44.0f, 2.0f), Mat(TEXT("MI_DeviceBlack")), Yaw, false);
	AddBox(At(-14.0f, 34.0f, DeskHeight + 1.5f), FVector(10.0f, 6.0f, 3.0f), Mat(TEXT("MI_DeviceBlack")), Yaw, false);
	const float Side = (Seed % 2) ? 1.0f : -1.0f;
	AddInstance(Cylinder, At(-5.0f, Side * 58.0f, DeskHeight + 5.0f), 0.0f, FVector(0.08f, 0.08f, 0.1f), Mat(TEXT("MI_Ceramic")), false);
	AddBox(At(-6.0f, -Side * 52.0f, DeskHeight + 0.6f), FVector(30.0f, 21.0f, 1.2f), Mat(TEXT("MI_Paper")), Yaw + Seed * 7.0f, false);
	AddBox(At(-4.0f, -Side * 50.0f, DeskHeight + 1.6f), FVector(30.0f, 21.0f, 0.8f), Mat(TEXT("MI_Paper")), Yaw - 9.0f, false);
	// Acoustic divider along the far edge.
	AddBox(At(42.0f, 0, DeskHeight + 22.0f), FVector(3.0f, 160.0f, 44.0f), Mat(TEXT("MI_AcousticPanel")), Yaw, false);
	if (bLamp) DeskLamp(At(10.0f, Side * 60.0f, DeskHeight), Yaw + 160.0f);
	TaskChair(At(-75.0f, 0, 0), Yaw);
}

void ARbValtrenHQ::DeskLamp(const FVector& Base, float Yaw)
{
	const FRotator R(0.0f, Yaw, 0.0f);
	auto At = [&](float X, float Z) { return Base + R.RotateVector(FVector(X, 0.0f, 0.0f)) + FVector(0.0f, 0.0f, Z); };
	AddInstance(Cylinder, At(0, 1.0f), 0.0f, FVector(0.14f, 0.14f, 0.02f), Mat(TEXT("MI_Graphite")), false);
	AddBox(At(0, 22.0f), FVector(1.6f, 1.6f, 42.0f), Mat(TEXT("MI_Brass")), Yaw, false);
	AddBox(At(10.0f, 42.0f), FVector(22.0f, 1.6f, 1.6f), Mat(TEXT("MI_Brass")), Yaw, false);
	AddBox(At(20.0f, 38.0f), FVector(12.0f, 9.0f, 7.0f), Mat(TEXT("MI_Graphite")), Yaw, false);
	AddBox(At(20.0f, 34.4f), FVector(9.0f, 6.0f, 0.4f), Mat(TEXT("MI_LampWarm")), Yaw, false);
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(At(20.0f, 30.0f));
	Light->SetIntensityUnits(ELightUnits::Lumens);
	Light->SetIntensity(450.0f);
	Light->SetAttenuationRadius(220.0f);
	Light->SetLightColor(FLinearColor(1.0f, 0.74f, 0.45f));
	Light->SetCastShadows(false);
	Light->RegisterComponent();
	++LightCount;
}

void ARbValtrenHQ::Shelf(const FVector& Center, float Yaw, float Width, int32 Seed)
{
	// Open graphite shelving (back to the wall, front along local +X) with coloured binders.
	const FRotator R(0.0f, Yaw, 0.0f);
	auto At = [&](float X, float Y, float Z) { return Center + R.RotateVector(FVector(X, Y, 0.0f)) + FVector(0.0f, 0.0f, Z); };
	const float Height = 190.0f;
	for (float Side : {-1.0f, 1.0f})
	{
		AddBox(At(0, Side * Width * 0.5f, Height * 0.5f), FVector(36.0f, 3.0f, Height), Mat(TEXT("MI_Graphite")), Yaw);
	}
	const TCHAR* Colors[] = {TEXT("MI_TealAccent"), TEXT("MI_Graphite"), TEXT("MI_PlasticGrey"), TEXT("MI_Paper"), TEXT("MI_SuitNavy")};
	FRandomStream Rand(Seed);
	for (int32 Level = 0; Level < 5; ++Level)
	{
		const float Z = 4.0f + Level * 44.0f;
		AddBox(At(0, 0, Z), FVector(36.0f, Width, 2.5f), Mat(TEXT("MI_Graphite")), Yaw, false);
		if (Level == 4) break;
		float Y = -Width * 0.5f + 6.0f;
		while (Y < Width * 0.5f - 12.0f)
		{
			if (Rand.FRand() < 0.18f)
			{
				Y += 14.0f; // gap
				continue;
			}
			const float T = Rand.FRandRange(4.5f, 7.0f);
			const float H = Rand.FRandRange(28.0f, 33.0f);
			AddBox(At(2.0f, Y + T * 0.5f, Z + 1.25f + H * 0.5f), FVector(28.0f, T, H), Mat(Colors[Rand.RandRange(0, 4)]), Yaw, false);
			Y += T + 0.6f;
		}
	}
}

void ARbValtrenHQ::ArtPrint(const FVector& WallPoint, float Yaw, const TCHAR* Print, const FVector2D& Size)
{
	// Framed print hung with its face along local +X.
	const FVector Out = FRotator(0.0f, Yaw, 0.0f).Vector();
	AddBox(WallPoint + Out * 1.5f, FVector(3.0f, Size.X + 8.0f, Size.Y + 8.0f), Mat(TEXT("MI_DeviceBlack")), Yaw, false);
	AddBox(WallPoint + Out * 3.1f, FVector(0.4f, Size.X, Size.Y), Mat(Print), Yaw, false);
}

void ARbValtrenHQ::SmallProps(const FVector& Location, float Yaw, int32 Kind)
{
	const FRotator R(0.0f, Yaw, 0.0f);
	auto At = [&](float X, float Y, float Z) { return Location + R.RotateVector(FVector(X, Y, 0.0f)) + FVector(0.0f, 0.0f, Z); };
	switch (Kind)
	{
	case 0: // waste bin
		AddInstance(Cylinder, At(0, 0, 18.0f), 0.0f, FVector(0.28f, 0.28f, 0.36f), Mat(TEXT("MI_Graphite")), true);
		break;
	case 1: // coat stand with a coat
		AddInstance(Cylinder, At(0, 0, 1.5f), 0.0f, FVector(0.4f, 0.4f, 0.03f), Mat(TEXT("MI_Graphite")), true);
		AddInstance(Cylinder, At(0, 0, 90.0f), 0.0f, FVector(0.04f, 0.04f, 1.8f), Mat(TEXT("MI_WoodWalnut")), false);
		for (int32 i = 0; i < 4; ++i)
		{
			AddBox(Location + FRotator(0.0f, Yaw + i * 90.0f, 0.0f).Vector() * 9.0f + FVector(0, 0, 170.0f), FVector(18.0f, 2.0f, 2.0f), Mat(TEXT("MI_Brass")), Yaw + i * 90.0f, false);
		}
		AddBox(At(6.0f, 0, 135.0f), FVector(12.0f, 46.0f, 70.0f), Mat(TEXT("MI_SuitNavy")), Yaw, false);
		break;
	case 2: // printer on a cabinet
		AddBox(At(0, 0, 37.0f), FVector(60.0f, 70.0f, 74.0f), Mat(TEXT("MI_PlasticGrey")), Yaw);
		AddBox(At(0, 0, 92.0f), FVector(52.0f, 58.0f, 36.0f), Mat(TEXT("MI_Paper")), Yaw);
		AddBox(At(14.0f, 0, 111.0f), FVector(20.0f, 40.0f, 2.0f), Mat(TEXT("MI_DeviceBlack")), Yaw, false);
		AddBox(At(27.0f, 0, 86.0f), FVector(4.0f, 30.0f, 3.0f), Mat(TEXT("MI_Paper")), Yaw, false);
		break;
	case 3: // water cooler
		AddBox(At(0, 0, 52.0f), FVector(32.0f, 32.0f, 104.0f), Mat(TEXT("MI_Paper")), Yaw);
		AddInstance(Cylinder, At(0, 0, 128.0f), 0.0f, FVector(0.27f, 0.27f, 0.46f), Mat(TEXT("MI_Glass")), false);
		AddBox(At(17.0f, 0, 78.0f), FVector(3.0f, 10.0f, 6.0f), Mat(TEXT("MI_TealAccent")), Yaw, false);
		break;
	default:
		break;
	}
}

void ARbValtrenHQ::Plant(const FVector& Location, bool bLarge)
{
	AddInstance(Model(bLarge ? TEXT("potted_plant_02") : TEXT("potted_plant_04")), Location, FMath::Fmod(Location.X * 0.37f + Location.Y, 360.0f),
		FVector(bLarge ? 1.4f : 1.0f));
}

void ARbValtrenHQ::FurnishRooms()
{
	UMaterialInterface* Oak = Mat(TEXT("MI_WoodOak"));
	UMaterialInterface* Walnut = Mat(TEXT("MI_WoodWalnut"));
	UMaterialInterface* Dash = Mat(TEXT("MI_ScreenDashboard"));
	UMaterialInterface* Mail = Mat(TEXT("MI_ScreenEmail"));
	UMaterialInterface* Sheet = Mat(TEXT("MI_ScreenSheet"));

	// Reception: walnut backdrop wall with a backlit logo, walnut-fronted desk, lounge on a rug.
	AddBox(FVector(500.0f, 1186.0f, 150.0f), FVector(520.0f, 10.0f, 300.0f), Walnut, 0.0f, false);
	AddLabel(TEXT("VALTREN"), FVector(500.0f, 1180.5f, 205.0f), -90.0f, 46.0f, FColor(244, 244, 242));
	AddLabel(TEXT("COLDLINE LOGISTICS"), FVector(500.0f, 1180.5f, 172.0f), -90.0f, 15.0f, FColor(200, 230, 232));
	AddBox(FVector(500.0f, 1180.0f, 150.0f), FVector(240.0f, 1.0f, 2.0f), Mat(TEXT("MI_ReceptionGlow")), 0.0f, false);
	AddBox(FVector(500.0f, 1178.0f, 296.0f), FVector(500.0f, 4.0f, 3.0f), Mat(TEXT("MI_LedStrip")), 0.0f, false);
	Desk(FVector(500.0f, 1010.0f, 0.0f), 0.0f, FVector(340.0f, 84.0f, 108.0f), Oak);
	AddBox(FVector(500.0f, 966.0f, 54.0f), FVector(320.0f, 4.0f, 104.0f), Walnut, 0.0f);
	AddBox(FVector(500.0f, 963.5f, 4.0f), FVector(320.0f, 1.0f, 3.0f), Mat(TEXT("MI_ReceptionGlow")), 0.0f, false);
	Monitor(FVector(440.0f, 1040.0f, 108.0f), -90.0f, Mail);
	TaskChair(FVector(500.0f, 1100.0f, 0.0f), -90.0f);
	AddBox(FVector(220.0f, 560.0f, 0.7f), FVector(300.0f, 220.0f, 1.4f), Mat(TEXT("MI_Rug")), 0.0f, false);
	AddInstance(Model(TEXT("sofa_02")), FVector(110.0f, 560.0f, 0.0f), 0.0f + ModelYawOffset, FVector(1.0f));
	AddInstance(Model(TEXT("modern_coffee_table_01")), FVector(255.0f, 560.0f, 0.0f), 90.0f + ModelYawOffset, FVector(1.0f));
	AddInstance(Model(TEXT("office_notepads")), FVector(255.0f, 560.0f, 39.5f), 20.0f, FVector(0.3f), nullptr, false);
	Chair(FVector(330.0f, 700.0f, 0.0f), -150.0f);
	Chair(FVector(330.0f, 420.0f, 0.0f), 150.0f);
	Plant(FVector(80.0f, 1120.0f, 0.0f), true);
	Plant(FVector(920.0f, 1120.0f, 0.0f), true);
	Plant(FVector(90.0f, 300.0f, 0.0f), true);
	SmallProps(FVector(900.0f, 300.0f, 0.0f), 0.0f, 1);
	ArtPrint(FVector(WallThickness * 0.5f + 12.0f, 820.0f, 165.0f), 0.0f, TEXT("MI_ArtPrintA"), FVector2D(70.0f, 90.0f));
	AddInstance(Model(TEXT("wall_clock")), FVector(1000.0f - WallThickness * 0.5f - 3.0f, 700.0f, 230.0f), 180.0f + ModelYawOffset, FVector(1.0f), nullptr, false);

	// Junior analyst workspace: two rows of three workplaces; shelving, printer, cooler.
	int32 Seed = 0;
	for (float X : {1200.0f, 1500.0f, 1800.0f})
	{
		if (X != 1500.0f) Workplace(FVector(X, 1085.0f, 0.0f), 90.0f, (Seed % 2) ? Dash : Mail, false, Seed); // 1500 is the player's
		++Seed;
		Workplace(FVector(X, 640.0f, 0.0f), 90.0f, (Seed % 3 == 0) ? Sheet : Dash, false, Seed);
		Plant(FVector(X + 64.0f, 610.0f, DeskHeight), false);
		++Seed;
	}
	// The player's own desk: the screen itself is the interactable (PlaceInteractables).
	Desk(FVector(1500.0f, 1085.0f, 0.0f), 0.0f, FVector(160.0f, 80.0f, DeskHeight), Oak);
	AddBox(FVector(1500.0f, 1127.0f, DeskHeight + 22.0f), FVector(160.0f, 3.0f, 44.0f), Mat(TEXT("MI_AcousticPanel")), 0.0f, false);
	AddBox(FVector(1500.0f, 1073.0f, DeskHeight + 1.0f), FVector(44.0f, 15.0f, 2.0f), Mat(TEXT("MI_DeviceBlack")), 0.0f, false);
	AddInstance(Model(TEXT("office_notepads")), FVector(1440.0f, 1070.0f, DeskHeight), 10.0f, FVector(0.25f), nullptr, false);
	AddInstance(Cylinder, FVector(1560.0f, 1060.0f, DeskHeight + 5.0f), 0.0f, FVector(0.08f, 0.08f, 0.1f), Mat(TEXT("MI_Ceramic")), false);
	TaskChair(FVector(1500.0f, 1010.0f, 0.0f), 90.0f);
	Shelf(FVector(1000.0f + WallThickness * 0.5f + 20.0f, 420.0f, 0.0f), 0.0f, 180.0f, 3);
	SmallProps(FVector(1940.0f, 470.0f, 0.0f), 180.0f, 2);
	SmallProps(FVector(1950.0f, 330.0f, 0.0f), 180.0f, 3);
	SmallProps(FVector(1360.0f, 820.0f, 0.0f), 0.0f, 0);
	Plant(FVector(1940.0f, 1130.0f, 0.0f), true);

	// Manager's office: walnut desk with a warm lamp, visitor chairs, shelving, filing cabinet (evidence), art.
	AddBox(FVector(2520.0f, 860.0f, 0.7f), FVector(420.0f, 300.0f, 1.4f), Mat(TEXT("MI_Rug")), 0.0f, false);
	Desk(FVector(2500.0f, 1010.0f, 0.0f), 0.0f, FVector(200.0f, 90.0f, DeskHeight), Walnut);
	Monitor(FVector(2440.0f, 1040.0f, DeskHeight), -90.0f, Dash);
	DeskLamp(FVector(2580.0f, 1035.0f, DeskHeight), -110.0f);
	AddBox(FVector(2530.0f, 995.0f, DeskHeight + 0.6f), FVector(30.0f, 21.0f, 1.2f), Mat(TEXT("MI_Paper")), 12.0f, false);
	TaskChair(FVector(2500.0f, 1110.0f, 0.0f), -90.0f);
	Chair(FVector(2420.0f, 860.0f, 0.0f), 90.0f, TEXT("dining_chair_02"));
	Chair(FVector(2580.0f, 860.0f, 0.0f), 90.0f, TEXT("dining_chair_02"));
	Shelf(FVector(2000.0f + WallThickness * 0.5f + 20.0f, 560.0f, 0.0f), 0.0f, 200.0f, 7);
	ArtPrint(FVector(3000.0f - WallThickness * 0.5f - 0.5f, 520.0f, 165.0f), 180.0f, TEXT("MI_ArtPrintB"), FVector2D(80.0f, 100.0f));
	SmallProps(FVector(2900.0f, 300.0f, 0.0f), 0.0f, 1);
	Plant(FVector(2080.0f, 1130.0f, 0.0f), true);
	AddInstance(Model(TEXT("wall_clock")), FVector(3000.0f - WallThickness * 0.5f - 3.0f, 760.0f, 230.0f), 180.0f + ModelYawOffset, FVector(1.0f), nullptr, false);

	// Meeting room: table for six, projector screen, art; binder evidence placed by PlaceInteractables.
	Desk(FVector(3500.0f, 700.0f, 0.0f), 0.0f, FVector(320.0f, 130.0f, DeskHeight), Oak);
	for (float X : {3390.0f, 3500.0f, 3610.0f})
	{
		Chair(FVector(X, 590.0f, 0.0f), 90.0f, TEXT("dining_chair_02"));
		Chair(FVector(X, 810.0f, 0.0f), -90.0f, TEXT("dining_chair_02"));
	}
	AddInstance(Cylinder, FVector(3560.0f, 680.0f, DeskHeight + 5.0f), 0.0f, FVector(0.08f, 0.08f, 0.1f), Mat(TEXT("MI_Ceramic")), false);
	AddInstance(Model(TEXT("projector_screen")), FVector(3500.0f, 1120.0f, 0.0f), -90.0f + ModelYawOffset, FVector(1.2f));
	ArtPrint(FVector(4000.0f - 14.0f, 420.0f, 165.0f), 180.0f, TEXT("MI_ArtPrintC"), FVector2D(70.0f, 90.0f));
	SmallProps(FVector(3080.0f, 300.0f, 0.0f), 0.0f, 0);
	Plant(FVector(3920.0f, 280.0f, 0.0f), true);

	// Training area: whiteboard with a cause-event-consequence sketch; tables; clipboard evidence on a table.
	AddBox(FVector(1000.0f - WallThickness * 0.5f - 1.5f, -700.0f, 150.0f), FVector(2.0f, 300.0f, 112.0f), Mat(TEXT("MI_Graphite")), 0.0f, false);
	AddBox(FVector(1000.0f - WallThickness * 0.5f - 2.8f, -700.0f, 150.0f), FVector(0.5f, 294.0f, 106.0f), Mat(TEXT("MI_Whiteboard")), 180.0f, false);
	AddBox(FVector(1000.0f - WallThickness * 0.5f - 5.0f, -700.0f, 92.0f), FVector(8.0f, 300.0f, 3.0f), Mat(TEXT("MI_Graphite")), 0.0f, false);
	for (float Y : {-520.0f, -880.0f})
	{
		Desk(FVector(500.0f, Y, 0.0f), 90.0f, FVector(160.0f, 70.0f, DeskHeight), Oak);
		Chair(FVector(390.0f, Y - 40.0f, 0.0f), 0.0f, TEXT("dining_chair_02"));
		Chair(FVector(390.0f, Y + 40.0f, 0.0f), 0.0f, TEXT("dining_chair_02"));
	}
	Shelf(FVector(WallThickness * 0.5f + 20.0f, -460.0f, 0.0f), 0.0f, 160.0f, 11);
	SmallProps(FVector(80.0f, -1120.0f, 0.0f), 0.0f, 0);
	Plant(FVector(80.0f, -280.0f, 0.0f), true);

	// Risk monitoring room: video wall of live dashboards and the alarm console, operator desk.
	for (int32 Col = 0; Col < 3; ++Col)
	{
		for (int32 Row = 0; Row < 2; ++Row)
		{
			const FVector P(2000.0f - WallThickness * 0.5f - 3.0f, -900.0f + Col * 190.0f, 150.0f + Row * 100.0f);
			AddBox(P, FVector(4.0f, 180.0f, 96.0f), Mat(TEXT("MI_DeviceBlack")), 0.0f, false);
			UMaterialInterface* Feed = (Col == 1 && Row == 0) ? Mat(TEXT("MI_ScreenAlarmUI")) : ((Col + Row) % 2 ? Sheet : Dash);
			AddBox(P - FVector(2.2f, 0, 0), FVector(0.4f, 172.0f, 92.0f), Feed, 180.0f, false);
		}
	}
	Desk(FVector(1500.0f, -1060.0f, 0.0f), 0.0f, FVector(300.0f, 90.0f, DeskHeight), Mat(TEXT("MI_Graphite")));
	Monitor(FVector(1400.0f, -1080.0f, DeskHeight), 90.0f, Dash);
	Monitor(FVector(1600.0f, -1080.0f, DeskHeight), 90.0f, Mail);
	TaskChair(FVector(1500.0f, -950.0f, 0.0f), -90.0f);
	AddInstance(Cylinder, FVector(1630.0f, -1040.0f, DeskHeight + 5.0f), 0.0f, FVector(0.08f, 0.08f, 0.1f), Mat(TEXT("MI_Ceramic")), false);
	SmallProps(FVector(1080.0f, -300.0f, 0.0f), 0.0f, 0);
	// Second operator row facing the video wall.
	Workplace(FVector(1700.0f, -570.0f, 0.0f), 0.0f, Dash, false, 1);
	Workplace(FVector(1700.0f, -750.0f, 0.0f), 0.0f, Sheet, false, 2);
	Plant(FVector(1340.0f, -1045.0f, DeskHeight), false);
	// KRI status rail under the video wall, status clock beside it.
	{
		const FVector P(2000.0f - WallThickness * 0.5f - 3.0f, -710.0f, 80.0f);
		AddBox(P, FVector(4.0f, 568.0f, 40.0f), Mat(TEXT("MI_DeviceBlack")), 0.0f, false);
		AddBox(P - FVector(2.2f, 0, 0), FVector(0.4f, 560.0f, 35.0f), Mat(TEXT("MI_ScreenKriTicker")), 180.0f, false);
	}
	AddInstance(Model(TEXT("wall_clock")), FVector(2000.0f - WallThickness * 0.5f - 3.0f, -330.0f, 230.0f), 180.0f + ModelYawOffset, FVector(1.0f), nullptr, false);
	// Acoustic panels: west wall bays and either side of the door.
	for (float Y : {-420.0f, -700.0f, -980.0f})
	{
		AddBox(FVector(1000.0f + WallThickness * 0.5f + 2.0f, Y, 165.0f), FVector(4.0f, 200.0f, 150.0f), Mat(TEXT("MI_AcousticPanel")), 0.0f, false);
	}
	for (float X : {1200.0f, 1800.0f})
	{
		AddBox(FVector(X, -CorridorHalfY - WallThickness * 0.5f - 2.0f, 165.0f), FVector(240.0f, 4.0f, 150.0f), Mat(TEXT("MI_AcousticPanel")), 0.0f, false);
	}
	Plant(FVector(1930.0f, -1130.0f, 0.0f), true);

	// Executive boardroom: long walnut table on a rug, ten chairs, warm pendants, display, art, credenza.
	AddBox(FVector(3000.0f, -700.0f, 0.7f), FVector(860.0f, 360.0f, 1.4f), Mat(TEXT("MI_Rug")), 0.0f, false);
	Desk(FVector(3000.0f, -700.0f, 0.0f), 0.0f, FVector(600.0f, 170.0f, DeskHeight), Walnut);
	for (int32 i = 0; i < 5; ++i)
	{
		const float X = 2780.0f + i * 110.0f;
		Chair(FVector(X, -585.0f, 0.0f), -90.0f, TEXT("dining_chair_02"));
		Chair(FVector(X, -815.0f, 0.0f), 90.0f, TEXT("dining_chair_02"));
		AddBox(FVector(X, -650.0f, DeskHeight + 0.5f), FVector(30.0f, 21.0f, 1.0f), Mat(TEXT("MI_Paper")), (i - 2) * 3.0f, false);
	}
	for (float X : {2820.0f, 3000.0f, 3180.0f})
	{
		AddInstance(Model(TEXT("modern_ceiling_lamp_01")), FVector(X, -700.0f, CeilingHeight - 118.0f), 0.0f, FVector(1.0f), nullptr, false);
		UPointLightComponent* Warm = NewObject<UPointLightComponent>(this, NAME_None);
		Warm->SetMobility(EComponentMobility::Movable);
		Warm->SetupAttachment(RootComponent);
		Warm->SetRelativeLocation(FVector(X, -700.0f, CeilingHeight - 105.0f));
		Warm->SetIntensityUnits(ELightUnits::Lumens);
		Warm->SetIntensity(1200.0f);
		Warm->SetAttenuationRadius(450.0f);
		Warm->SetLightColor(FLinearColor(1.0f, 0.76f, 0.5f));
		Warm->SetCastShadows(false);
		Warm->RegisterComponent();
		++LightCount;
	}
	AddBox(FVector(2000.0f + WallThickness * 0.5f + 3.0f, -700.0f, 165.0f), FVector(4.0f, 260.0f, 150.0f), Mat(TEXT("MI_DeviceBlack")), 0.0f, false);
	AddBox(FVector(2000.0f + WallThickness * 0.5f + 5.2f, -700.0f, 165.0f), FVector(0.4f, 250.0f, 140.0f), Dash, 0.0f, false);
	AddBox(FVector(3700.0f, -260.0f, 40.0f), FVector(300.0f, 50.0f, 80.0f), Walnut, 0.0f); // credenza
	ArtPrint(FVector(3700.0f, -200.0f - WallThickness * 0.5f - 0.5f, 175.0f), -90.0f, TEXT("MI_ArtPrintA"), FVector2D(120.0f, 80.0f));
	Plant(FVector(2080.0f, -1130.0f, 0.0f), true);
	Plant(FVector(3920.0f, -1130.0f, 0.0f), true);

	// Corridor: plants, bins and prints.
	Plant(FVector(60.0f, -140.0f, 0.0f), true);
	Plant(FVector(3940.0f, 140.0f, 0.0f), true);
	ArtPrint(FVector(750.0f, -200.0f + WallThickness * 0.5f + 0.5f, 165.0f), 90.0f, TEXT("MI_ArtPrintC"), FVector2D(60.0f, 80.0f));
	ArtPrint(FVector(250.0f, 200.0f - WallThickness * 0.5f - 0.5f, 165.0f), -90.0f, TEXT("MI_ArtPrintB"), FVector2D(60.0f, 80.0f));
	SmallProps(FVector(1980.0f, -150.0f, 0.0f), 0.0f, 0);
}

void ARbValtrenHQ::AddEasterEggs()
{
	// Small credits to the creator for curious players. Text only, no interaction prompt, no mission effect.
	const FString Name = RbCredits::CreatorName;
	TArray<FString> Parts;
	Name.ParseIntoArray(Parts, TEXT(" "));
	const FString Surname = Parts.Num() ? Parts.Last() : Name;
	FString Initials;
	for (const FString& P : Parts) Initials += P.Left(1) + TEXT(".");

	// 1) Training area, west wall: a framed certificate low beside the shelving.
	const FVector Cert(WallThickness * 0.5f + 0.5f, -1010.0f, 150.0f);
	AddBox(Cert + FVector(1.5f, 0, 0), FVector(3.0f, 42.0f, 32.0f), Mat(TEXT("MI_WoodWalnut")), 0.0f, false);
	AddBox(Cert + FVector(3.2f, 0, 0), FVector(0.4f, 36.0f, 26.0f), Mat(TEXT("MI_Paper")), 0.0f, false);
	AddLabel(TEXT("CERTIFICATE OF RECOGNITION"), Cert + FVector(3.6f, 0, 7.0f), 0.0f, 1.6f, FColor(120, 100, 60));
	AddLabel(TEXT("Risk Analyst of the Year"), Cert + FVector(3.6f, 0, 2.5f), 0.0f, 2.4f, FColor(40, 44, 52));
	AddLabel(Name, Cert + FVector(3.6f, 0, -2.5f), 0.0f, 2.8f, FColor(20, 70, 80));
	AddLabel(TEXT("Valtren Coldline Logistics - Risk function"), Cert + FVector(3.6f, 0, -7.5f), 0.0f, 1.3f, FColor(110, 110, 110));

	// 2) Manager's office shelf: a teal hardback whose spine reads author and title.
	// Eye-level shelf, at the end clear of the binders (Shelf fills up to Width / 2 - 12).
	const FVector Book(2000.0f + WallThickness * 0.5f + 22.0f, 654.0f, 4.0f + 3 * 44.0f + 1.25f + 16.0f);
	AddBox(Book, FVector(30.0f, 5.5f, 32.0f), Mat(TEXT("MI_TealAccent")), 0.0f, false);
	FString Given; // "M. A." from "M A Almahlaki"
	for (int32 i = 0; i + 1 < Parts.Num(); ++i) Given += (Given.IsEmpty() ? TEXT("") : TEXT(" ")) + Parts[i].Left(1) + TEXT(".");
	AddLabel(FString::Printf(TEXT("%s, %s  -  Thinking in Residual Risk"), *Surname, *Given),
		Book + FVector(15.2f, 0, 0), 0.0f, 1.25f, FColor(236, 238, 240), -90.0f);

	// 3) Boardroom table: a small brass maker's inlay near the far corner of the walnut top.
	const FVector Plate(3282.0f, -770.0f, DeskHeight + 0.1f);
	AddBox(Plate, FVector(10.0f, 3.0f, 0.3f), Mat(TEXT("MI_Brass")), 0.0f, false);
	AddLabel(Initials, Plate + FVector(0, 0, 0.2f), -90.0f, 1.5f, FColor(60, 44, 20), 0.0f, 90.0f);
}

template <class T>
T* ARbValtrenHQ::SpawnInteractable(const FString& Id, const FVector& Location, float Yaw, UStaticMesh* Mesh, const FVector& Scale, UMaterialInterface* Material)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	T* Actor = GetWorld()->SpawnActor<T>(Location, FRotator(0.0f, Yaw, 0.0f), Params);
	if (Actor)
	{
		Actor->InteractableId = Id;
		Actor->SetAppearance(Mesh, Scale, Material);
	}
	return Actor;
}

void ARbValtrenHQ::PlaceInteractables()
{
	auto ModelOr = [this](const TCHAR* Id) { UStaticMesh* M = Model(Id); return M ? M : Cube.Get(); };

	// Priya Okafor beside her desk, facing the door. Invisible capsule for the trace, mannequin body.
	if (ARbNpc* Priya = SpawnInteractable<ARbNpc>(TEXT("priya_okafor"), FVector(2330.0f, 880.0f, 90.0f), -90.0f, Cylinder, FVector(0.5f, 0.5f, 1.8f), nullptr))
	{
		Priya->SetBody(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")),
			LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle")), Mat(TEXT("MI_SuitNavy")), -90.0f);
	}
	// The player's workstation: the monitor screen is the interactable.
	Monitor(FVector(1500.0f, 1100.0f, DeskHeight), -90.0f, nullptr);
	SpawnInteractable<ARbWorkstation>(TEXT("workstation"), FVector(1500.0f, 1098.2f, DeskHeight + 38.5f), -90.0f, Cube, FVector(0.004f, 0.58f, 0.33f), Mat(TEXT("MI_ScreenEmail")));
	// Evidence.
	SpawnInteractable<ARbEvidenceProp>(TEXT("insurance_policy"), FVector(2930.0f, 1050.0f, 0.0f), 180.0f + ModelYawOffset, ModelOr(TEXT("drawer_cabinet")), FVector(0.55f), nullptr);
	SpawnInteractable<ARbEvidenceProp>(TEXT("maintenance_log"), FVector(3440.0f, 690.0f, DeskHeight + 0.3f), 15.0f, ModelOr(TEXT("binder_notebook")), FVector(1.0f), nullptr);
	SpawnInteractable<ARbEvidenceProp>(TEXT("manual_temp_log"), FVector(520.0f, -880.0f, DeskHeight + 0.4f), -20.0f, ModelOr(TEXT("clipboard")), FVector(1.0f), nullptr);
	SpawnInteractable<ARbEvidenceProp>(TEXT("client_contract"), FVector(3000.0f, -650.0f, DeskHeight + 0.2f), 5.0f, ModelOr(TEXT("office_notepads")), FVector(0.3f), nullptr);
	// Alarm terminal: centre monitor of the operator desk, showing an alert.
	Monitor(FVector(1500.0f, -1080.0f, DeskHeight), 90.0f, nullptr);
	SpawnInteractable<ARbEvidenceProp>(TEXT("alarm_log"), FVector(1500.0f, -1078.2f, DeskHeight + 38.5f), 90.0f, Cube, FVector(0.004f, 0.58f, 0.33f), Mat(TEXT("MI_ScreenAlarmUI")));
}
