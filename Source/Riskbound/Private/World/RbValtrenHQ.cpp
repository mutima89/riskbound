#include "RbValtrenHQ.h"

#include "Animation/AnimSequence.h"
#include "Components/DirectionalLightComponent.h"
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

UStaticMeshComponent* ARbValtrenHQ::AddBox(const FVector& Center, const FVector& Size, UMaterialInterface* Material, float Yaw, bool bCollision)
{
	return AddMesh(Cube, Center, Yaw, Size / 100.0f, Material, bCollision); // the basic cube is 100 cm
}

void ARbValtrenHQ::AddLabel(const FString& Text, const FVector& Location, float Yaw, float Size, const FColor& Color)
{
	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, NAME_None);
	Label->SetupAttachment(RootComponent);
	Label->SetRelativeLocation(Location);
	Label->SetRelativeRotation(FRotator(0.0f, Yaw, 0.0f));
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
	AddBox(FVector(Location.X, Location.Y, CeilingHeight - 1.0f), FVector(120.0f, 60.0f, 2.0f), Mat(TEXT("MI_LightPanel")), 0.0f, false);
	URectLightComponent* Light = NewObject<URectLightComponent>(this, NAME_None);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(Location.X, Location.Y, CeilingHeight - 4.0f));
	Light->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	Light->SetIntensityUnits(ELightUnits::Lumens);
	Light->SetIntensity(Lumens);
	Light->SetSourceWidth(120.0f);
	Light->SetSourceHeight(60.0f);
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
	PlaceInteractables();
	UE_LOG(LogRiskbound, Display, TEXT("Valtren HQ built: %d components, %d lights, %d rooms, art %s"), ComponentCount, LightCount, Rooms().Num(),
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
	AddBox(FVector(Width * 0.5f, 0.0f, -10.0f), FVector(Width + 60.0f, 2.0f * BuildingHalfY + 60.0f, 20.0f), Mat(TEXT("MI_FloorConcrete")));
	AddBox(FVector(Width * 0.5f, 0.0f, CeilingHeight + 5.0f), FVector(Width + 60.0f, 2.0f * BuildingHalfY + 60.0f, 10.0f), Mat(TEXT("MI_CeilingSoft")));
	const TMap<FString, const TCHAR*> Floors = {
		{TEXT("Reception"), TEXT("MI_FloorTerrazzo")}, {TEXT("Junior analyst workspace"), TEXT("MI_FloorLaminate")},
		{TEXT("Manager's office"), TEXT("MI_FloorParquet")}, {TEXT("Meeting room"), TEXT("MI_FloorLaminate")},
		{TEXT("Training area"), TEXT("MI_FloorLaminate")}, {TEXT("Risk monitoring room"), TEXT("MI_FloorConcrete")},
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
		bool bFirst = true;
		for (int32 ix = 0; ix < Cols; ++ix)
		{
			for (int32 iy = 0; iy < 2; ++iy)
			{
				const FVector P(C.X + (ix - (Cols - 1) * 0.5f) * S.X / Cols, C.Y + (iy - 0.5f) * S.Y / 2.0f, 0.0f);
				AddPanelLight(P, bDim ? PanelLumens * 0.35f : PanelLumens, bFirst, bDim ? FLinearColor(0.85f, 0.92f, 1.0f) : FLinearColor(1.0f, 0.96f, 0.9f));
				bFirst = false;
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
	// Screen faces local +X; Base is the point on the desk under the monitor's foot.
	const FRotator R(0.0f, Yaw, 0.0f);
	auto At = [&](float X, float Z) { return Base + R.RotateVector(FVector(X, 0.0f, 0.0f)) + FVector(0.0f, 0.0f, Z); };
	AddBox(At(0, 0.6f), FVector(18.0f, 22.0f, 1.2f), Mat(TEXT("MI_Graphite")), Yaw, false);
	AddBox(At(-2.0f, 12.0f), FVector(3.0f, 5.0f, 22.0f), Mat(TEXT("MI_Graphite")), Yaw, false);
	AddBox(At(0, 38.0f), FVector(3.0f, 62.0f, 37.0f), Mat(TEXT("MI_DeviceBlack")), Yaw);
	if (Screen)
	{
		AddBox(At(1.6f, 38.5f), FVector(0.4f, 58.0f, 33.0f), Screen, Yaw, false);
	}
	// Keyboard in front of the monitor.
	AddBox(At(32.0f, 1.0f), FVector(15.0f, 44.0f, 2.0f), Mat(TEXT("MI_DeviceBlack")), Yaw, false);
}

void ARbValtrenHQ::Chair(const FVector& Location, float Yaw, const TCHAR* ModelId)
{
	// The arm chair model's forward axis is opposite to the dining chair's.
	const float Extra = FCString::Strcmp(ModelId, TEXT("modern_arm_chair_01")) == 0 ? 180.0f : 0.0f;
	if (!AddMesh(Model(ModelId), Location, Yaw + ModelYawOffset + Extra, FVector(1.0f)))
	{
		AddBox(Location + FVector(0, 0, 23.0f), FVector(50.0f, 50.0f, 46.0f), Mat(TEXT("MI_Graphite")), Yaw);
	}
}

void ARbValtrenHQ::Plant(const FVector& Location, bool bLarge)
{
	AddMesh(Model(bLarge ? TEXT("potted_plant_02") : TEXT("potted_plant_04")), Location, FMath::Fmod(Location.X * 0.37f + Location.Y, 360.0f),
		FVector(bLarge ? 1.4f : 1.0f));
}

void ARbValtrenHQ::FurnishRooms()
{
	UMaterialInterface* Oak = Mat(TEXT("MI_WoodOak"));
	UMaterialInterface* Walnut = Mat(TEXT("MI_WoodWalnut"));
	UMaterialInterface* Screen = Mat(TEXT("MI_ScreenTeal"));

	// Reception: desk with teal front, company sign, sofa, coffee table, plants, clock.
	Desk(FVector(500.0f, 1010.0f, 0.0f), 0.0f, FVector(320.0f, 80.0f, 105.0f), Oak);
	AddBox(FVector(500.0f, 968.0f, 52.0f), FVector(320.0f, 2.0f, 100.0f), Mat(TEXT("MI_TealAccent")), 0.0f, false);
	Monitor(FVector(420.0f, 1030.0f, 105.0f), -90.0f, Screen);
	AddBox(FVector(500.0f, 1150.0f, 200.0f), FVector(380.0f, 4.0f, 70.0f), Mat(TEXT("MI_DeviceBlack")), 0.0f, false);
	AddLabel(TEXT("VALTREN COLDLINE LOGISTICS"), FVector(500.0f, 1147.5f, 205.0f), -90.0f, 22.0f, FColor(240, 242, 244));
	AddBox(FVector(500.0f, 1147.0f, 178.0f), FVector(300.0f, 1.0f, 3.0f), Mat(TEXT("MI_TealAccent")), 0.0f, false);
	AddMesh(Model(TEXT("sofa_02")), FVector(120.0f, 560.0f, 0.0f), 0.0f + ModelYawOffset, FVector(1.0f));
	AddMesh(Model(TEXT("modern_coffee_table_01")), FVector(260.0f, 560.0f, 0.0f), 90.0f + ModelYawOffset, FVector(1.0f));
	AddMesh(Model(TEXT("office_notepads")), FVector(260.0f, 560.0f, 39.5f), 20.0f, FVector(0.3f), nullptr, false);
	Plant(FVector(80.0f, 1120.0f, 0.0f), true);
	Plant(FVector(920.0f, 1120.0f, 0.0f), true);
	Plant(FVector(90.0f, 300.0f, 0.0f), true);
	AddMesh(Model(TEXT("wall_clock")), FVector(1000.0f - WallThickness * 0.5f - 3.0f, 700.0f, 220.0f), 180.0f + ModelYawOffset, FVector(1.0f), nullptr, false);

	// Junior analyst workspace: two rows of desks with monitors and task chairs.
	for (float X : {1200.0f, 1500.0f, 1800.0f})
	{
		Desk(FVector(X, 1085.0f, 0.0f), 0.0f, FVector(160.0f, 80.0f, DeskHeight), Oak);
		Chair(FVector(X, 980.0f, 0.0f), 90.0f);
		if (X != 1500.0f) Monitor(FVector(X, 1100.0f, DeskHeight), -90.0f, Screen); // 1500 is the player's workstation
		Desk(FVector(X, 640.0f, 0.0f), 0.0f, FVector(160.0f, 80.0f, DeskHeight), Oak);
		Monitor(FVector(X, 655.0f, DeskHeight), -90.0f, Screen);
		Chair(FVector(X, 535.0f, 0.0f), 90.0f);
		Plant(FVector(X + 60.0f, 625.0f, DeskHeight), false);
	}
	AddMesh(Model(TEXT("office_notepads")), FVector(1440.0f, 1070.0f, DeskHeight), 10.0f, FVector(0.25f), nullptr, false);
	Plant(FVector(1940.0f, 300.0f, 0.0f), true);

	// Manager's office: walnut desk, visitor chairs, filing cabinet (evidence), plant, clock.
	Desk(FVector(2500.0f, 1010.0f, 0.0f), 0.0f, FVector(200.0f, 90.0f, DeskHeight), Walnut);
	Monitor(FVector(2440.0f, 1030.0f, DeskHeight), -90.0f, Screen);
	Chair(FVector(2500.0f, 1110.0f, 0.0f), -90.0f);
	Chair(FVector(2420.0f, 860.0f, 0.0f), 90.0f, TEXT("dining_chair_02"));
	Chair(FVector(2580.0f, 860.0f, 0.0f), 90.0f, TEXT("dining_chair_02"));
	Plant(FVector(2080.0f, 1130.0f, 0.0f), true);
	AddMesh(Model(TEXT("wall_clock")), FVector(3000.0f - WallThickness * 0.5f - 3.0f, 700.0f, 220.0f), 180.0f + ModelYawOffset, FVector(1.0f), nullptr, false);

	// Meeting room: oak table for six, projector screen, binder (evidence) placed by PlaceInteractables.
	Desk(FVector(3500.0f, 700.0f, 0.0f), 0.0f, FVector(320.0f, 130.0f, DeskHeight), Oak);
	for (float X : {3390.0f, 3500.0f, 3610.0f})
	{
		Chair(FVector(X, 590.0f, 0.0f), 90.0f, TEXT("dining_chair_02"));
		Chair(FVector(X, 810.0f, 0.0f), -90.0f, TEXT("dining_chair_02"));
	}
	AddMesh(Model(TEXT("projector_screen")), FVector(3500.0f, 1120.0f, 0.0f), -90.0f + ModelYawOffset, FVector(1.2f));
	Plant(FVector(3920.0f, 280.0f, 0.0f), true);

	// Training area: tables facing a whiteboard on the divider wall; clipboard (evidence) on a table.
	AddBox(FVector(1000.0f - WallThickness * 0.5f - 1.5f, -700.0f, 150.0f), FVector(2.0f, 300.0f, 110.0f), Mat(TEXT("MI_CeilingWhite")), 0.0f, false);
	AddBox(FVector(1000.0f - WallThickness * 0.5f - 3.0f, -700.0f, 92.0f), FVector(6.0f, 300.0f, 4.0f), Mat(TEXT("MI_Graphite")), 0.0f, false);
	for (float Y : {-520.0f, -880.0f})
	{
		Desk(FVector(500.0f, Y, 0.0f), 90.0f, FVector(160.0f, 70.0f, DeskHeight), Oak);
		Chair(FVector(390.0f, Y - 40.0f, 0.0f), 0.0f, TEXT("dining_chair_02"));
		Chair(FVector(390.0f, Y + 40.0f, 0.0f), 0.0f, TEXT("dining_chair_02"));
	}
	Plant(FVector(80.0f, -280.0f, 0.0f), true);

	// Risk monitoring room: video wall, operator desk with three monitors (the centre one is the alarm terminal).
	for (int32 Col = 0; Col < 3; ++Col)
	{
		for (int32 Row = 0; Row < 2; ++Row)
		{
			const FVector P(2000.0f - WallThickness * 0.5f - 3.0f, -900.0f + Col * 190.0f, 150.0f + Row * 100.0f);
			AddBox(P, FVector(4.0f, 180.0f, 92.0f), Mat(TEXT("MI_DeviceBlack")), 0.0f, false);
			AddBox(P - FVector(2.2f, 0, 0), FVector(0.4f, 172.0f, 86.0f), Mat((Col == 1 && Row == 0) ? TEXT("MI_ScreenAlert") : TEXT("MI_ScreenTeal")), 0.0f, false);
		}
	}
	Desk(FVector(1500.0f, -1060.0f, 0.0f), 0.0f, FVector(300.0f, 90.0f, DeskHeight), Mat(TEXT("MI_Graphite")));
	Monitor(FVector(1400.0f, -1080.0f, DeskHeight), 90.0f, Screen);
	Monitor(FVector(1600.0f, -1080.0f, DeskHeight), 90.0f, Screen);
	Chair(FVector(1500.0f, -950.0f, 0.0f), -90.0f);

	// Executive boardroom: long walnut table, ten chairs, pendant lights, wall display.
	Desk(FVector(3000.0f, -700.0f, 0.0f), 0.0f, FVector(600.0f, 170.0f, DeskHeight), Walnut);
	for (int32 i = 0; i < 5; ++i)
	{
		const float X = 2780.0f + i * 110.0f;
		Chair(FVector(X, -585.0f, 0.0f), -90.0f, TEXT("dining_chair_02"));
		Chair(FVector(X, -815.0f, 0.0f), 90.0f, TEXT("dining_chair_02"));
	}
	for (float X : {2820.0f, 3000.0f, 3180.0f})
	{
		AddMesh(Model(TEXT("modern_ceiling_lamp_01")), FVector(X, -700.0f, CeilingHeight - 118.0f), 0.0f, FVector(1.0f), nullptr, false);
	}
	AddBox(FVector(2000.0f + WallThickness * 0.5f + 3.0f, -700.0f, 165.0f), FVector(4.0f, 260.0f, 150.0f), Mat(TEXT("MI_DeviceBlack")), 0.0f, false);
	AddBox(FVector(2000.0f + WallThickness * 0.5f + 5.2f, -700.0f, 165.0f), FVector(0.4f, 250.0f, 140.0f), Screen, 0.0f, false);
	Plant(FVector(2080.0f, -1130.0f, 0.0f), true);
	Plant(FVector(3920.0f, -1130.0f, 0.0f), true);

	// Corridor: plants at both ends.
	Plant(FVector(60.0f, -140.0f, 0.0f), true);
	Plant(FVector(3940.0f, 140.0f, 0.0f), true);
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
	SpawnInteractable<ARbWorkstation>(TEXT("workstation"), FVector(1500.0f, 1098.2f, DeskHeight + 38.5f), -90.0f, Cube, FVector(0.004f, 0.58f, 0.33f), Mat(TEXT("MI_ScreenTeal")));
	// Evidence.
	SpawnInteractable<ARbEvidenceProp>(TEXT("insurance_policy"), FVector(2930.0f, 1050.0f, 0.0f), 180.0f + ModelYawOffset, ModelOr(TEXT("drawer_cabinet")), FVector(0.55f), nullptr);
	SpawnInteractable<ARbEvidenceProp>(TEXT("maintenance_log"), FVector(3440.0f, 690.0f, DeskHeight + 0.3f), 15.0f, ModelOr(TEXT("binder_notebook")), FVector(1.0f), nullptr);
	SpawnInteractable<ARbEvidenceProp>(TEXT("manual_temp_log"), FVector(520.0f, -880.0f, DeskHeight + 0.4f), -20.0f, ModelOr(TEXT("clipboard")), FVector(1.0f), nullptr);
	SpawnInteractable<ARbEvidenceProp>(TEXT("client_contract"), FVector(3000.0f, -650.0f, DeskHeight + 0.2f), 5.0f, ModelOr(TEXT("office_notepads")), FVector(0.3f), nullptr);
	// Alarm terminal: centre monitor of the operator desk, showing an alert.
	Monitor(FVector(1500.0f, -1080.0f, DeskHeight), 90.0f, nullptr);
	SpawnInteractable<ARbEvidenceProp>(TEXT("alarm_log"), FVector(1500.0f, -1078.2f, DeskHeight + 38.5f), 90.0f, Cube, FVector(0.004f, 0.58f, 0.33f), Mat(TEXT("MI_ScreenAlert")));
}
