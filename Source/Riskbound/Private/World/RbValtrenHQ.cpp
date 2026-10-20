#include "RbValtrenHQ.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RbInteractable.h"
#include "RbLog.h"

namespace
{
constexpr float BuildingMinX = 0.0f;
constexpr float BuildingMaxX = 4000.0f;
constexpr float BuildingHalfY = 1200.0f;
constexpr float CorridorHalfY = 200.0f;
constexpr float WallThickness = 20.0f;
constexpr float WallHeight = 320.0f;
constexpr float DoorWidth = 220.0f;
constexpr float DeskHeight = 75.0f;

const FLinearColor FloorColor(0.18f, 0.18f, 0.19f);
const FLinearColor WallColor(0.62f, 0.63f, 0.66f);
const FLinearColor WoodColor(0.36f, 0.24f, 0.15f);
const FLinearColor DeskColor(0.30f, 0.32f, 0.36f);
const FLinearColor PaperColor(0.90f, 0.88f, 0.80f);
const FLinearColor ScreenColor(0.10f, 0.30f, 0.45f);
const FLinearColor PersonColor(0.55f, 0.22f, 0.30f);
const FLinearColor CabinetColor(0.40f, 0.42f, 0.30f);
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
	Add(TEXT("Corridor"), FVector(150.0f, 0.0f, Eye), FVector(4000.0f, 0.0f, 120.0f));
	for (const FRoom& Room : Rooms())
	{
		const FVector2D C = Room.Bounds.GetCenter();
		const FVector2D S = Room.Bounds.GetSize();
		const float Dir = Room.bNorthSide ? 1.0f : -1.0f; // into the room from the corridor
		const float DoorY = Room.bNorthSide ? CorridorHalfY : -CorridorHalfY;
		Add(Room.Name, FVector(C.X - 0.32f * S.X, DoorY + Dir * 130.0f, Eye),
			FVector(C.X + 0.3f * S.X, C.Y + Dir * 0.3f * S.Y, 100.0f));
	}
}

void ARbValtrenHQ::Build()
{
	if (bBuilt) return;
	bBuilt = true;
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Cube || !Cylinder || !BaseMaterial)
	{
		UE_LOG(LogRiskbound, Error, TEXT("Valtren HQ: engine basic shapes missing; is /Engine/BasicShapes cooked?"));
		return;
	}
	BuildRooms();
	BuildLighting();
	PlaceInteractables();
	UE_LOG(LogRiskbound, Display, TEXT("Valtren HQ built: %d components, %d rooms"), ComponentCount, Rooms().Num());
}

void ARbValtrenHQ::AddBox(const FVector& Center, const FVector& Size, const FLinearColor& Color)
{
	UStaticMeshComponent* Box = NewObject<UStaticMeshComponent>(this, NAME_None);
	Box->SetMobility(EComponentMobility::Movable);
	Box->SetStaticMesh(Cube);
	Box->SetupAttachment(RootComponent);
	Box->SetRelativeLocation(Center);
	Box->SetRelativeScale3D(Size / 100.0f); // the basic cube is 100 cm
	Box->RegisterComponent();
	UMaterialInstanceDynamic* Material = Box->CreateDynamicMaterialInstance(0, BaseMaterial);
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	++ComponentCount;
}

void ARbValtrenHQ::AddWallX(float Y, float X0, float X1, const TArray<float>& DoorCentersX)
{
	float Start = X0;
	TArray<float> Doors = DoorCentersX;
	Doors.Sort();
	auto Segment = [this, Y](float A, float B)
	{
		if (B - A > 1.0f)
		{
			AddBox(FVector((A + B) * 0.5f, Y, WallHeight * 0.5f), FVector(B - A, WallThickness, WallHeight), WallColor);
		}
	};
	for (float Door : Doors)
	{
		Segment(Start, Door - DoorWidth * 0.5f);
		// Lintel above the door keeps the wall line readable.
		AddBox(FVector(Door, Y, WallHeight - 30.0f), FVector(DoorWidth, WallThickness, 60.0f), WallColor);
		Start = Door + DoorWidth * 0.5f;
	}
	Segment(Start, X1);
}

void ARbValtrenHQ::AddWallY(float X, float Y0, float Y1)
{
	AddBox(FVector(X, (Y0 + Y1) * 0.5f, WallHeight * 0.5f), FVector(WallThickness, Y1 - Y0, WallHeight), WallColor);
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

void ARbValtrenHQ::AddRoomLight(const FVector& Location)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(Location);
	Light->SetIntensityUnits(ELightUnits::Lumens);
	Light->SetIntensity(9000.0f);
	Light->SetAttenuationRadius(1400.0f);
	Light->SetLightColor(FLinearColor(1.0f, 0.96f, 0.9f));
	Light->RegisterComponent();
	++ComponentCount;
}

void ARbValtrenHQ::BuildRooms()
{
	const float Width = BuildingMaxX - BuildingMinX;
	// Floor slab and a tinted floor per room so rooms read apart at a glance.
	AddBox(FVector(Width * 0.5f, 0.0f, -10.0f), FVector(Width + 2 * WallThickness, 2 * BuildingHalfY + 2 * WallThickness, 20.0f), FloorColor);

	TArray<float> NorthDoors;
	TArray<float> SouthDoors;
	int32 Index = 0;
	for (const FRoom& Room : Rooms())
	{
		const FVector2D C = Room.Bounds.GetCenter();
		const FVector2D S = Room.Bounds.GetSize();
		const float Hue = 360.0f * (static_cast<float>(Index++) / Rooms().Num());
		const FLinearColor Tint = FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue / 360.0f * 255.0f), 60, 90);
		AddBox(FVector(C.X, C.Y, 0.5f), FVector(S.X - WallThickness, S.Y - WallThickness, 1.0f), Tint);
		(Room.bNorthSide ? NorthDoors : SouthDoors).Add(C.X);

		// Name over the door on the corridor side, and again inside the room.
		const float DoorY = Room.bNorthSide ? CorridorHalfY : -CorridorHalfY;
		const float Out = Room.bNorthSide ? -1.0f : 1.0f; // towards the corridor
		AddLabel(Room.Name, FVector(C.X, DoorY + Out * (WallThickness * 0.5f + 1.0f), WallHeight - 30.0f), Room.bNorthSide ? -90.0f : 90.0f, 26.0f, FColor(255, 214, 120));
		AddLabel(Room.Name, FVector(C.X, DoorY - Out * (WallThickness * 0.5f + 1.0f), 250.0f), Room.bNorthSide ? 90.0f : -90.0f, 30.0f, FColor(255, 214, 120));
		AddRoomLight(FVector(C.X, C.Y, WallHeight - 40.0f));
	}
	AddRoomLight(FVector(1000.0f, 0.0f, WallHeight - 40.0f));
	AddRoomLight(FVector(3000.0f, 0.0f, WallHeight - 40.0f));

	// Outer walls and the corridor walls with a door into every room.
	AddWallX(BuildingHalfY, BuildingMinX, BuildingMaxX, {});
	AddWallX(-BuildingHalfY, BuildingMinX, BuildingMaxX, {});
	AddWallX(CorridorHalfY, BuildingMinX, BuildingMaxX, NorthDoors);
	AddWallX(-CorridorHalfY, BuildingMinX, BuildingMaxX, SouthDoors);
	AddWallY(BuildingMinX, -BuildingHalfY, BuildingHalfY);
	AddWallY(BuildingMaxX, -BuildingHalfY, BuildingHalfY);
	for (float X : {1000.0f, 2000.0f, 3000.0f}) AddWallY(X, CorridorHalfY, BuildingHalfY);
	for (float X : {1000.0f, 2000.0f}) AddWallY(X, -BuildingHalfY, -CorridorHalfY);

	AddLabel(TEXT("VALTREN COLDLINE LOGISTICS  -  Head office"), FVector(2000.0f, 0.0f, WallHeight + 40.0f), 180.0f, 40.0f, FColor::White);

	// Furniture.
	AddBox(FVector(500.0f, 980.0f, 50.0f), FVector(260.0f, 80.0f, 100.0f), WoodColor);                 // reception desk
	AddBox(FVector(1500.0f, 1050.0f, DeskHeight * 0.5f), FVector(200.0f, 90.0f, DeskHeight), DeskColor); // analyst desk (player)
	AddBox(FVector(1200.0f, 650.0f, DeskHeight * 0.5f), FVector(160.0f, 80.0f, DeskHeight), DeskColor);
	AddBox(FVector(1800.0f, 650.0f, DeskHeight * 0.5f), FVector(160.0f, 80.0f, DeskHeight), DeskColor);
	AddBox(FVector(2500.0f, 1060.0f, DeskHeight * 0.5f), FVector(220.0f, 90.0f, DeskHeight), WoodColor); // manager's desk
	AddBox(FVector(3500.0f, 700.0f, DeskHeight * 0.5f), FVector(320.0f, 150.0f, DeskHeight), WoodColor); // meeting table
	AddBox(FVector(1500.0f, -1050.0f, DeskHeight * 0.5f), FVector(260.0f, 90.0f, DeskHeight), DeskColor); // monitoring desk
	AddBox(FVector(1500.0f, -1185.0f, 200.0f), FVector(500.0f, 8.0f, 160.0f), ScreenColor);            // video wall
	AddBox(FVector(500.0f, -700.0f, DeskHeight * 0.5f), FVector(160.0f, 80.0f, DeskHeight), DeskColor);  // training table
	AddBox(FVector(300.0f, -1180.0f, 170.0f), FVector(300.0f, 6.0f, 120.0f), FLinearColor::White);     // whiteboard
	AddBox(FVector(3000.0f, -700.0f, DeskHeight * 0.5f), FVector(700.0f, 180.0f, DeskHeight), WoodColor); // boardroom table
}

void ARbValtrenHQ::BuildLighting()
{
	UDirectionalLightComponent* Sun = NewObject<UDirectionalLightComponent>(this, TEXT("Sun"));
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetupAttachment(RootComponent);
	Sun->SetRelativeRotation(FRotator(-55.0f, 35.0f, 0.0f));
	Sun->SetIntensity(6.0f);
	Sun->SetAtmosphereSunLight(true);
	Sun->RegisterComponent();

	USkyAtmosphereComponent* Atmosphere = NewObject<USkyAtmosphereComponent>(this, TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(RootComponent);
	Atmosphere->RegisterComponent();

	USkyLightComponent* Sky = NewObject<USkyLightComponent>(this, TEXT("SkyLight"));
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SetupAttachment(RootComponent);
	Sky->bRealTimeCapture = true;
	Sky->SetIntensity(1.0f);
	Sky->RegisterComponent();
	ComponentCount += 3;
}

template <class T>
T* ARbValtrenHQ::SpawnInteractable(const FString& Id, const FVector& Location, float Yaw, UStaticMesh* Mesh, const FVector& Scale,
	const FLinearColor& Color, const FString& Label, float LabelHeight)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	T* Actor = GetWorld()->SpawnActor<T>(Location, FRotator(0.0f, Yaw, 0.0f), Params);
	if (Actor)
	{
		Actor->InteractableId = Id;
		Actor->SetAppearance(Mesh, Scale, Color, Label, LabelHeight);
	}
	return Actor;
}

void ARbValtrenHQ::PlaceInteractables()
{
	const float OnDesk = DeskHeight + 3.0f;
	// Text labels face the player's approach direction (TextRender faces +X of the actor).
	SpawnInteractable<ARbNpc>(TEXT("priya_okafor"), FVector(2420.0f, 900.0f, 90.0f), -90.0f, Cylinder, FVector(0.5f, 0.5f, 1.8f), PersonColor,
		TEXT("Priya Okafor\nRisk Manager"), 115.0f);
	SpawnInteractable<ARbWorkstation>(TEXT("workstation"), FVector(1500.0f, 1060.0f, DeskHeight + 25.0f), -90.0f, Cube, FVector(0.06f, 0.7f, 0.45f), ScreenColor,
		TEXT("Your workstation"), 45.0f);
	SpawnInteractable<ARbEvidenceProp>(TEXT("insurance_policy"), FVector(2860.0f, 1110.0f, 65.0f), -90.0f, Cube, FVector(0.6f, 0.5f, 1.3f), CabinetColor,
		TEXT("Filing cabinet"), 85.0f);
	SpawnInteractable<ARbEvidenceProp>(TEXT("maintenance_log"), FVector(3450.0f, 690.0f, OnDesk), -90.0f, Cube, FVector(0.3f, 0.4f, 0.08f), FLinearColor(0.1f, 0.2f, 0.6f),
		TEXT("Maintenance binder"), 25.0f);
	SpawnInteractable<ARbEvidenceProp>(TEXT("alarm_log"), FVector(1500.0f, -1070.0f, DeskHeight + 25.0f), 90.0f, Cube, FVector(0.06f, 0.7f, 0.45f), ScreenColor,
		TEXT("Alarm terminal"), 45.0f);
	SpawnInteractable<ARbEvidenceProp>(TEXT("manual_temp_log"), FVector(500.0f, -690.0f, OnDesk), 90.0f, Cube, FVector(0.25f, 0.35f, 0.04f), PaperColor,
		TEXT("Clipboard"), 25.0f);
	SpawnInteractable<ARbEvidenceProp>(TEXT("client_contract"), FVector(3000.0f, -650.0f, OnDesk), 90.0f, Cube, FVector(0.35f, 0.25f, 0.05f), FLinearColor(0.5f, 0.1f, 0.1f),
		TEXT("Document wallet"), 25.0f);
}
