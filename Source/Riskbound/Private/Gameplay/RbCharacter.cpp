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
#include "RbAudioSubsystem.h"

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

void ARbCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>() * LookRate * Sensitivity();
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

void ARbCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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
