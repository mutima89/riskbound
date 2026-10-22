#include "RbCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "RbGameSubsystem.h"
#include "RbHUD.h"
#include "RbInteractionComponent.h"
#include "RbInteractable.h"
#include "RbUIStyle.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "RbAudioSubsystem.h"
#include "UnrealClient.h"

ARbCharacter::ARbCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 90.0f);
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->MaxWalkSpeed = 320.0f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(80.0f);

	Interaction = CreateDefaultSubobject<URbInteractionComponent>(TEXT("Interaction"));
	PrimaryActorTick.bCanEverTick = true;
}

void ARbCharacter::BeginUse(ARbInteractable* Target, TFunction<void()> OnDone)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!Target || !PC || RbUI::IsReducedMotion())
	{
		if (OnDone) OnDone();
		return;
	}
	const ERbUseStyle Style = Target->GetUseStyle();
	const FVector Eye = Camera->GetComponentLocation();
	const FVector Focus = Target->GetFocusPoint();
	UseFrom = PC->GetControlRotation();
	UseTo = (Focus - Eye).Rotation();
	UseTo.Roll = 0.0f;
	UseT = 0.0f;
	UseLength = Style == ERbUseStyle::Person ? 0.3f : 0.38f;
	UseDone = MoveTemp(OnDone);
	// Screens: lean in to reading distance (never through the desk).
	bLeaning = false;
	if (Style == ERbUseStyle::Screen)
	{
		const FVector To = Focus - Eye;
		const float Move = FMath::Clamp(To.Size() - 70.0f, 0.0f, 80.0f);
		LeanTarget = GetActorTransform().InverseTransformVectorNoScale(To.GetSafeNormal() * Move);
		bLeaning = Move > 1.0f;
	}
	// Documents: the prop (or a folder from a cabinet) comes up to the camera.
	if (Style == ERbUseStyle::PickUp)
	{
		FTransform From;
		if (UStaticMesh* M = Target->GetPickUpMesh(From))
		{
			if (!HeldProp)
			{
				HeldProp = NewObject<UStaticMeshComponent>(this, TEXT("HeldProp"));
				HeldProp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				HeldProp->SetCastShadow(false);
				HeldProp->SetupAttachment(GetRootComponent());
				HeldProp->RegisterComponent();
			}
			HeldProp->SetStaticMesh(M);
			HeldProp->SetWorldTransform(From);
			HeldProp->SetVisibility(true);
			PropFrom = From;
			URbAudioSubsystem::Cue(this, ERbCue::Click);
		}
	}
}

void ARbCharacter::TickUse(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (UseT >= 0.0f && PC)
	{
		UseT = FMath::Min(1.0f, UseT + DeltaSeconds / UseLength);
		const float E = FMath::InterpEaseInOut(0.0f, 1.0f, UseT, 2.0f);
		PC->SetControlRotation(FMath::Lerp(UseFrom, UseTo, E));
		if (bLeaning) Lean = E;
		if (HeldProp && HeldProp->IsVisible())
		{
			// Held a little below the centre of view, tilted towards the reader.
			const FVector Eye = Camera->GetComponentLocation();
			const FRotator View = Camera->GetComponentRotation();
			const FTransform Held(FRotator(View.Pitch + 55.0f, View.Yaw + 90.0f, 0.0f), Eye + View.Vector() * 32.0f - FVector(0.0f, 0.0f, 6.0f), PropFrom.GetScale3D());
			FTransform T;
			T.Blend(PropFrom, Held, E);
			HeldProp->SetWorldTransform(T);
		}
		if (UseT >= 1.0f)
		{
			UseT = -1.0f;
			if (HeldProp) HeldProp->SetVisibility(false);
			if (UseDone)
			{
				TFunction<void()> Done = MoveTemp(UseDone);
				Done();
			}
		}
	}
	else if (Lean > 0.0f)
	{
		// Ease back once the screen is closed.
		const ARbHUD* Hud = PC ? Cast<ARbHUD>(PC->GetHUD()) : nullptr;
		if (!Hud || !Hud->IsScreenOpen()) Lean = FMath::Max(0.0f, Lean - DeltaSeconds / 0.3f);
	}
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f) + LeanTarget * FMath::InterpEaseInOut(0.0f, 1.0f, Lean, 2.0f));
}

void ARbCharacter::BuildInput()
{
	if (Mapping) return;

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};
	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	InteractAction = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
	PauseAction = MakeAction(TEXT("IA_Pause"), EInputActionValueType::Boolean);
	JournalAction = MakeAction(TEXT("IA_Journal"), EInputActionValueType::Boolean);
	InboxAction = MakeAction(TEXT("IA_Inbox"), EInputActionValueType::Boolean);
	PhotoAction = MakeAction(TEXT("IA_Photo"), EInputActionValueType::Boolean);
	PadLookAction = MakeAction(TEXT("IA_PadLook"), EInputActionValueType::Axis2D);
	ShotAction = MakeAction(TEXT("IA_Shot"), EInputActionValueType::Boolean);

	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Riskbound"));

	// Move is (right, forward). Keys on the forward axis are swizzled from X to Y.
	auto MapMove = [this](const FKey& Key, bool bForwardAxis, bool bNegate)
	{
		FEnhancedActionKeyMapping& M = Mapping->MapKey(MoveAction, Key);
		if (bForwardAxis)
		{
			M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		}
		if (bNegate)
		{
			M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		}
	};
	MapMove(EKeys::W, true, false);
	MapMove(EKeys::Up, true, false);
	MapMove(EKeys::S, true, true);
	MapMove(EKeys::Down, true, true);
	MapMove(EKeys::D, false, false);
	MapMove(EKeys::Right, false, false);
	MapMove(EKeys::A, false, true);
	MapMove(EKeys::Left, false, true);

	FEnhancedActionKeyMapping& Look = Mapping->MapKey(LookAction, EKeys::Mouse2D);
	UInputModifierNegate* InvertY = NewObject<UInputModifierNegate>(this);
	InvertY->bX = false;
	InvertY->bY = true;
	InvertY->bZ = false;
	Look.Modifiers.Add(InvertY);

	Mapping->MapKey(InteractAction, EKeys::E);
	Mapping->MapKey(InteractAction, EKeys::Enter);
	Mapping->MapKey(PauseAction, EKeys::Escape);
	Mapping->MapKey(PauseAction, EKeys::P);
	Mapping->MapKey(JournalAction, EKeys::J);
	Mapping->MapKey(JournalAction, EKeys::Tab);
	Mapping->MapKey(InboxAction, EKeys::I);
	Mapping->MapKey(PhotoAction, EKeys::F10);
	// Controller (Xbox layout; Steam Deck maps the same): left stick moves, right stick looks,
	// A uses, X inbox, Y journal, Start pauses, B backs out of screens (SRbScreen).
	{
		FEnhancedActionKeyMapping& Stick = Mapping->MapKey(MoveAction, EKeys::Gamepad_Left2D);
		Stick.Modifiers.Add(NewObject<UInputModifierDeadZone>(this));
		FEnhancedActionKeyMapping& Pad = Mapping->MapKey(PadLookAction, EKeys::Gamepad_Right2D);
		Pad.Modifiers.Add(NewObject<UInputModifierDeadZone>(this));
	}
	Mapping->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Bottom);
	Mapping->MapKey(InboxAction, EKeys::Gamepad_FaceButton_Left);
	Mapping->MapKey(JournalAction, EKeys::Gamepad_FaceButton_Top);
	Mapping->MapKey(PauseAction, EKeys::Gamepad_Special_Right);
	Mapping->MapKey(ShotAction, EKeys::F12);
}

void ARbCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();
	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARbCharacter::OnMove);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARbCharacter::OnLook);
	Input->BindAction(InteractAction, ETriggerEvent::Started, this, &ARbCharacter::OnInteract);
	Input->BindAction(PauseAction, ETriggerEvent::Started, this, &ARbCharacter::OnPause);
	Input->BindAction(JournalAction, ETriggerEvent::Started, this, &ARbCharacter::OnJournal);
	Input->BindAction(InboxAction, ETriggerEvent::Started, this, &ARbCharacter::OnInbox);
	Input->BindAction(PhotoAction, ETriggerEvent::Started, this, &ARbCharacter::OnPhoto);
	Input->BindAction(PadLookAction, ETriggerEvent::Triggered, this, &ARbCharacter::OnPadLook);
	Input->BindAction(ShotAction, ETriggerEvent::Started, this, &ARbCharacter::OnShot);
}

void ARbCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	BuildInput();
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr)
	{
		Subsystem->ClearAllMappings();
		Subsystem->AddMappingContext(Mapping, 0);
	}
}

void ARbCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator Yaw(0.0f, GetControlRotation().Yaw, 0.0f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}

void ARbCharacter::OnPadLook(const FInputActionValue& Value)
{
	// Stick deflection is a rate, not a delta: degrees per second scaled by the controller sensitivity.
	const URbGameSubsystem* Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<URbGameSubsystem>() : nullptr;
	const float Rate = 140.0f * (Game ? Game->GetSettings().GamepadSensitivity : 1.0f) * GetWorld()->GetDeltaSeconds();
	const bool bInvert = Game && Game->GetSettings().bInvertY;
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X * Rate);
	AddControllerPitchInput((bInvert ? Axis.Y : -Axis.Y) * Rate);
}

void ARbCharacter::OnLook(const FInputActionValue& Value)
{
	FVector2D Axis = Value.Get<FVector2D>() * LookRate * Sensitivity();
	const URbGameSubsystem* G = GetGameInstance() ? GetGameInstance()->GetSubsystem<URbGameSubsystem>() : nullptr;
	if (G && G->GetSettings().bInvertY) Axis.Y = -Axis.Y;
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

float ARbCharacter::Sensitivity() const
{
	const URbGameSubsystem* Game = GetGameInstance() ? GetGameInstance()->GetSubsystem<URbGameSubsystem>() : nullptr;
	return Game ? Game->GetSettings().MouseSensitivity : 1.0f;
}

static ARbHUD* HudFor(const APawn* Pawn)
{
	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	return PC ? Cast<ARbHUD>(PC->GetHUD()) : nullptr;
}

void ARbCharacter::OnInteract()
{
	Interaction->Interact();
}

void ARbCharacter::OnPause()
{
	if (ARbHUD* Hud = HudFor(this)) Hud->OpenPauseMenu();
}

void ARbCharacter::OnJournal()
{
	if (ARbHUD* Hud = HudFor(this)) Hud->OpenJournal();
}

void ARbCharacter::OnInbox()
{
	ARbHUD* Hud = HudFor(this);
	if (Hud && !Hud->IsScreenOpen()) Hud->OpenInbox();
}

void ARbCharacter::OnPhoto()
{
	if (ARbHUD* Hud = HudFor(this)) Hud->TogglePhotoMode();
}

void ARbCharacter::OnShot()
{
	// Works in every build; files go to Saved/Screenshots.
	FScreenshotRequest::RequestScreenshot(false);
}

void ARbCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickUse(DeltaSeconds);
	// Footsteps: one every stride while walking on the floor.
	const float Speed = GetVelocity().Size2D();
	if (Speed < 40.0f || !GetCharacterMovement()->IsMovingOnGround())
	{
		StrideDistance = StrideLength * 0.6f; // the first step comes soon after starting to walk
		return;
	}
	StrideDistance += Speed * DeltaSeconds;
	if (StrideDistance >= StrideLength)
	{
		StrideDistance = 0.0f;
		URbAudioSubsystem::Cue(this, ERbCue::Footstep);
	}
}
