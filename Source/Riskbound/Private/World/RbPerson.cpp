#include "RbPerson.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/MorphTarget.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RbUIStyle.h"

namespace
{
// Rocketbox (3ds Max biped) bone names.
const FName PelvisBone(TEXT("Bip01 Pelvis"));
const FName NeckBone(TEXT("Bip01 Neck"));
const FName HeadBone(TEXT("Bip01 Head"));
const FName LeftEyeBone(TEXT("Bip01 LEye"));
const FName RightEyeBone(TEXT("Bip01 REye"));
const FName JawBone(TEXT("Bip01 MJaw"));

constexpr float WalkSpeed = 118.0f;     // cm/s, matches the walk loop's stride
constexpr float LookRange = 480.0f;     // people notice the player within this distance
constexpr float SpeakRate = 14.0f;      // characters a second
constexpr float MaxSpeech = 9.0f;       // seconds of lip-sync for the longest lines

TArray<TWeakObjectPtr<URbPersonComponent>>& Registry()
{
	static TArray<TWeakObjectPtr<URbPersonComponent>> R;
	return R;
}

FCompactPoseBoneIndex BoneIndex(const FCompactPose& Pose, const FName& Name)
{
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Name);
	if (MeshIndex == INDEX_NONE) return FCompactPoseBoneIndex(INDEX_NONE);
	return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
}

FTransform ComponentSpace(const FCompactPose& Pose, FCompactPoseBoneIndex Index)
{
	FTransform T = Pose[Index];
	for (FCompactPoseBoneIndex P = Pose.GetParentBoneIndex(Index); P.IsValid(); P = Pose.GetParentBoneIndex(P))
	{
		T = T * Pose[P];
	}
	return T;
}

FTransform RefComponentSpace(const FBoneContainer& Bones, FCompactPoseBoneIndex Index)
{
	FTransform T = Bones.GetRefPoseTransform(Index);
	for (FCompactPoseBoneIndex P = Bones.GetParentBoneIndex(Index); P.IsValid(); P = Bones.GetParentBoneIndex(P))
	{
		T = T * Bones.GetRefPoseTransform(P);
	}
	return T;
}

// Turns a bone by Delta about the component's axes, around the bone's own position.
void RotateInComponentSpace(FCompactPose& Pose, FCompactPoseBoneIndex Index, const FQuat& Delta)
{
	if (!Index.IsValid()) return;
	const FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Index);
	const FQuat ParentRot = Parent.IsValid() ? ComponentSpace(Pose, Parent).GetRotation() : FQuat::Identity;
	const FQuat Current = ParentRot * Pose[Index].GetRotation();
	Pose[Index].SetRotation((ParentRot.Inverse() * (Delta * Current)).GetNormalized());
}

// Meshes face +Y in component space: yaw turns about Z (+ towards -X), pitch about X (+ looks up).
FQuat LookDelta(float YawDeg, float PitchDeg)
{
	return FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDeg)) * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(PitchDeg));
}

// Letter to viseme: 0 Sil, 1 PP, 2 FF, 3 TH, 4 DD, 5 KK, 6 CH, 7 SS, 8 nn, 9 RR, 10 aa, 11 E, 12 I, 13 O, 14 U.
uint8 VisemeFor(TCHAR C, TCHAR Next)
{
	switch (FChar::ToLower(C))
	{
	case 'a': return 10;
	case 'e': return 11;
	case 'i': case 'y': return 12;
	case 'o': return 13;
	case 'u': case 'w': case 'q': return 14;
	case 'p': case 'b': case 'm': return 1;
	case 'f': case 'v': return 2;
	case 't': return FChar::ToLower(Next) == 'h' ? 3 : 4;
	case 'd': case 'l': return 4;
	case 'k': case 'g': case 'c': case 'x': return 5;
	case 'j': return 6;
	case 's': return FChar::ToLower(Next) == 'h' ? 6 : 7;
	case 'z': return 7;
	case 'n': return 8;
	case 'r': return 9;
	case 'h': return 10;
	default: return 0;
	}
}

// How far each viseme opens the jaw, for meshes without face shapes.
const float VisemeJaw[] = {0.0f, 0.0f, 0.15f, 0.2f, 0.3f, 0.35f, 0.3f, 0.15f, 0.25f, 0.3f, 0.9f, 0.6f, 0.45f, 0.75f, 0.5f};
} // namespace

// ---------------------------------------------------------------------------- evaluation

void FRbPersonAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	Pose = CastChecked<URbPersonAnimInstance>(InAnimInstance)->Pose;
}

bool FRbPersonAnimProxy::Evaluate(FPoseContext& Output)
{
	auto Sample = [this](UAnimSequence* Seq, float Time, bool bInPlace, FPoseContext& Into)
	{
		if (!Seq)
		{
			Into.ResetToRefPose();
			return;
		}
		FAnimationPoseData Data(Into);
		Seq->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(Time), false));
		if (bInPlace) PinPelvis(Into);
	};
	if (Pose.Blend && Pose.BlendAlpha > 0.001f)
	{
		FPoseContext From(this);
		FPoseContext To(this);
		Sample(Pose.Base, Pose.BaseTime, Pose.bBaseInPlace, From);
		Sample(Pose.Blend, Pose.BlendTime, Pose.bBlendInPlace, To);
		FAnimationPoseData FromData(From);
		FAnimationPoseData ToData(To);
		FAnimationPoseData OutData(Output);
		FAnimationRuntime::BlendTwoPosesTogether(ToData, FromData, FMath::Clamp(Pose.BlendAlpha, 0.0f, 1.0f), OutData);
	}
	else
	{
		Sample(Pose.Base, Pose.BaseTime, Pose.bBaseInPlace, Output);
	}
	ApplyLook(Output);
	return true;
}

void FRbPersonAnimProxy::PinPelvis(FPoseContext& Context) const
{
	// Walking loops travel; the component moves the person instead, so hold the hips over the origin.
	FCompactPose& P = Context.Pose;
	const FCompactPoseBoneIndex Pelvis = BoneIndex(P, PelvisBone);
	if (!Pelvis.IsValid()) return;
	const FCompactPoseBoneIndex Parent = P.GetParentBoneIndex(Pelvis);
	const FTransform ParentCS = Parent.IsValid() ? ComponentSpace(P, Parent) : FTransform::Identity;
	FVector CS = (P[Pelvis] * ParentCS).GetLocation();
	const FVector Ref = RefComponentSpace(P.GetBoneContainer(), Pelvis).GetLocation();
	CS.X = Ref.X;
	CS.Y = Ref.Y;
	P[Pelvis].SetLocation(ParentCS.InverseTransformPosition(CS));
}

void FRbPersonAnimProxy::ApplyLook(FPoseContext& Context) const
{
	FCompactPose& P = Context.Pose;
	if (FMath::Abs(Pose.HeadYaw) + FMath::Abs(Pose.HeadPitch) > 0.01f)
	{
		// Neck takes a third, the head the rest, so the turn reads as natural rather than a swivel.
		RotateInComponentSpace(P, BoneIndex(P, NeckBone), LookDelta(Pose.HeadYaw * 0.35f, Pose.HeadPitch * 0.35f));
		RotateInComponentSpace(P, BoneIndex(P, HeadBone), LookDelta(Pose.HeadYaw * 0.65f, Pose.HeadPitch * 0.65f));
	}
	if (FMath::Abs(Pose.EyeYaw) + FMath::Abs(Pose.EyePitch) > 0.01f)
	{
		const FQuat Eyes = LookDelta(Pose.EyeYaw, Pose.EyePitch);
		RotateInComponentSpace(P, BoneIndex(P, LeftEyeBone), Eyes);
		RotateInComponentSpace(P, BoneIndex(P, RightEyeBone), Eyes);
	}
	if (Pose.Jaw > 0.01f)
	{
		RotateInComponentSpace(P, BoneIndex(P, JawBone), FQuat(FVector::ForwardVector, FMath::DegreesToRadians(-9.0f * Pose.Jaw)));
	}
}

// ---------------------------------------------------------------------------- component

URbPersonComponent::URbPersonComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AnimClass = URbPersonAnimInstance::StaticClass();
	VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	bCastDynamicShadow = true;
}

void URbPersonComponent::OnRegister()
{
	Super::OnRegister();
	Registry().AddUnique(this);
}

void URbPersonComponent::OnUnregister()
{
	Registry().RemoveAll([this](const TWeakObjectPtr<URbPersonComponent>& P) { return !P.IsValid() || P.Get() == this; });
	Super::OnUnregister();
}

URbPersonComponent* URbPersonComponent::Find(const UWorld* World, const FString& InPersonId)
{
	URbPersonComponent* Hidden = nullptr;
	for (const TWeakObjectPtr<URbPersonComponent>& P : Registry())
	{
		URbPersonComponent* C = P.Get();
		if (!C || C->GetWorld() != World || C->PersonId != InPersonId || InPersonId.IsEmpty()) continue;
		const AActor* Owner = C->GetOwner();
		if (C->IsVisible() && (!Owner || !Owner->IsHidden())) return C;
		Hidden = C;
	}
	return Hidden;
}

FVector URbPersonComponent::GetEyeLocation() const
{
	if (GetBoneIndex(HeadBone) != INDEX_NONE) return GetSocketLocation(HeadBone) + FVector(0.0f, 0.0f, 6.0f);
	return GetComponentLocation() + FVector(0.0f, 0.0f, 160.0f);
}

void URbPersonComponent::Setup(const RbCast::FLook& Look, const FString& InPersonId, float InFacingYaw, int32 Seed)
{
	PersonId = InPersonId;
	bFemale = Look.bFemale;
	bCanAnimate = !Look.bFallback;
	MeshYaw = Look.MeshYaw;
	FacingYaw = TargetFacing = InFacingYaw;
	Rand.Initialize(Seed * 7919 + 17);
	SetSkeletalMesh(Look.Mesh);
	SetWorldRotation(FRotator(0.0f, FacingYaw + MeshYaw, 0.0f));
	IdleLoop = Look.Idle;
	if (Look.Idle) Loaded.AddUnique(Look.Idle);
	Pose = FRbPersonPose();
	Pose.Base = IdleLoop;
	Pose.BaseTime = IdleLoop ? Rand.FRandRange(0.0f, IdleLoop->GetPlayLength()) : 0.0f; // nobody moves in step
	NextBlink = Rand.FRandRange(0.5f, 3.0f);
	NextSaccade = Rand.FRandRange(0.3f, 1.5f);
	MoodTarget = Mood = 0.1f; // a resting face with a hint of warmth reads as alive, not blank
	FindShapes();
}

void URbPersonComponent::SitDown(bool bInAtTable)
{
	Activity = ERbPersonActivity::Sitting;
	bAtTable = bInAtTable;
	if (UAnimSequence* S = Loop(bAtTable ? (Rand.FRand() < 0.5f ? TEXT("sit_table_idle_neutral_01") : TEXT("sit_table_breathe_01")) : TEXT("sit_chair_idle_neutral_01")))
	{
		IdleLoop = S;
		SetLoop(S, 0.0f);
		Pose.BaseTime = Rand.FRandRange(0.0f, S->GetPlayLength());
	}
}

UAnimSequence* URbPersonComponent::Loop(const TCHAR* Name)
{
	if (!bCanAnimate) return nullptr;
	if (TObjectPtr<UAnimSequence>* Found = LoopCache.Find(Name)) return *Found;
	UAnimSequence* A = RbCast::Anim(bFemale, Name);
	LoopCache.Add(Name, A);
	if (A) Loaded.AddUnique(A);
	return A;
}

UAnimSequence* URbPersonComponent::RestLoop()
{
	if (Activity == ERbPersonActivity::Sitting) return IdleLoop;
	if (Activity == ERbPersonActivity::Walking) return Loop(TEXT("walk_neutral_01"));
	if (bEngaged)
	{
		if (UAnimSequence* L = Loop(TEXT("gestic_listen_neutral_01"))) return L;
	}
	return IdleLoop;
}

void URbPersonComponent::SetLoop(UAnimSequence* NewLoop, float BlendSeconds, bool bInPlace)
{
	if (!NewLoop) return;
	if (Pose.Blend && !bGestureOneShot && Pose.Blend == NewLoop) return;   // already heading there
	if (!Pose.Blend && Pose.Base == NewLoop) return;
	if (Pose.Blend && Pose.BlendAlpha >= 0.5f)
	{
		// Commit the half-finished crossfade so the new one starts from what is on screen.
		Pose.Base = Pose.Blend;
		Pose.BaseTime = bGestureOneShot ? 0.0f : Pose.BlendTime;
		Pose.bBaseInPlace = Pose.bBlendInPlace;
	}
	bGestureOneShot = false;
	if (BlendSeconds <= 0.0f)
	{
		Pose.Base = NewLoop;
		Pose.BaseTime = 0.0f;
		Pose.bBaseInPlace = bInPlace;
		Pose.Blend = nullptr;
		Pose.BlendAlpha = 0.0f;
		return;
	}
	Pose.Blend = NewLoop;
	Pose.BlendTime = 0.0f;
	Pose.BlendAlpha = 0.0f;
	Pose.bBlendInPlace = bInPlace;
	BlendSpeed = 1.0f / BlendSeconds;
}

bool URbPersonComponent::PlayGesture(const TCHAR* Name, float BlendIn)
{
	UAnimSequence* G = Loop(Name);
	if (!G || Activity == ERbPersonActivity::Walking) return false;
	if (Pose.Blend && Pose.BlendAlpha >= 0.5f && !bGestureOneShot)
	{
		Pose.Base = Pose.Blend;
		Pose.BaseTime = Pose.BlendTime;
		Pose.bBaseInPlace = Pose.bBlendInPlace;
	}
	Pose.Blend = G;
	Pose.BlendTime = 0.0f;
	Pose.BlendAlpha = 0.0f;
	Pose.bBlendInPlace = false;
	BlendSpeed = 1.0f / FMath::Max(BlendIn, 0.05f);
	bGestureOneShot = true;
	GestureLeft = FMath::Min(G->GetPlayLength(), 3.2f);
	return true;
}

void URbPersonComponent::Say(const FString& Text)
{
	// Build the viseme track: one shape per letter group, pauses at punctuation.
	Visemes.Reset();
	VisemeTimes.Reset();
	float T = 0.0f;
	const float Step = 1.0f / SpeakRate;
	uint8 Last = 255;
	for (int32 i = 0; i < Text.Len() && T < MaxSpeech; ++i)
	{
		const TCHAR C = Text[i];
		if (C == ',' || C == ';' || C == ':') { Visemes.Add(0); VisemeTimes.Add(T); T += 0.18f; Last = 0; continue; }
		if (C == '.' || C == '?' || C == '!') { Visemes.Add(0); VisemeTimes.Add(T); T += 0.32f; Last = 0; continue; }
		const uint8 V = VisemeFor(C, i + 1 < Text.Len() ? Text[i + 1] : TCHAR(' '));
		if (V != Last)
		{
			Visemes.Add(V);
			VisemeTimes.Add(T);
			Last = V;
		}
		T += Step;
	}
	Visemes.Add(0);
	VisemeTimes.Add(T);
	SpeakT = 0.0f;
	SpeakLength = T;
	MoodTarget = FMath::Max(MoodTarget, 0.15f);
	if (Activity == ERbPersonActivity::Standing)
	{
		static const TCHAR* const Talk[] = {TEXT("gestic_talk_neutral_01"), TEXT("gestic_talk_neutral_02"), TEXT("gestic_talk_relaxed_01"), TEXT("gestic_talk_neutral_03")};
		SetLoop(Loop(Talk[Rand.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Talk)) - 1)]), 0.4f);
	}
	else if (Activity == ERbPersonActivity::Sitting && bAtTable)
	{
		PlayGesture(TEXT("sit_table_gestic_thoughtful"), 0.4f);
	}
}

void URbPersonComponent::StopTalking()
{
	SpeakT = -1.0f;
	if (Activity == ERbPersonActivity::Standing) SetLoop(RestLoop(), 0.5f);
}

void URbPersonComponent::SetEngaged(bool bInEngaged)
{
	if (bEngaged == bInEngaged) return;
	bEngaged = bInEngaged;
	if (SpeakT < 0.0f && Activity == ERbPersonActivity::Standing) SetLoop(RestLoop(), 0.6f);
	if (!bEngaged) MoodTarget = 0.1f;
}

void URbPersonComponent::React(int32 Sign)
{
	MoodTarget = Sign > 0 ? 0.85f : (Sign < 0 ? -0.8f : -0.15f);
	MoodHold = 4.0f;
	NodSign = Sign > 0 ? 1 : (Sign < 0 ? -1 : 0);
	NodT = NodSign != 0 ? 0.0f : -1.0f;
	if (Activity == ERbPersonActivity::Sitting)
	{
		if (bAtTable) PlayGesture(Sign < 0 ? TEXT("sit_table_gestic_shrug_01") : TEXT("sit_table_gestic_thoughtful"));
		return;
	}
	if (Sign > 0)
	{
		if (!PlayGesture(Rand.FRand() < 0.5f ? TEXT("gestic_listen_accept_01") : TEXT("gestic_listen_accept_02"))) PlayGesture(TEXT("gestic_listen_accept_01"));
	}
	else if (Sign < 0)
	{
		if (!PlayGesture(Rand.FRand() < 0.5f ? TEXT("gestic_listen_deny_01") : TEXT("gestic_listen_deny_02"))) PlayGesture(TEXT("gestic_shrug_01"));
	}
	else
	{
		if (!PlayGesture(TEXT("gestic_thoughtful_01"))) PlayGesture(TEXT("gestic_shrug_01"));
	}
}

void URbPersonComponent::TurnTo(const FVector& WorldPoint)
{
	const FVector D = WorldPoint - GetComponentLocation();
	if (D.SizeSquared2D() > 1.0f) TargetFacing = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)));
}

void URbPersonComponent::WalkRoute(const TArray<FVector>& Points, TFunction<void()> OnArrive)
{
	Route = Points;
	RouteIndex = 0;
	OnArrived = MoveTemp(OnArrive);
	WaitedForPlayer = 0.0f;
	if (Route.IsEmpty())
	{
		if (OnArrived) { TFunction<void()> Done = MoveTemp(OnArrived); Done(); }
		return;
	}
	Activity = ERbPersonActivity::Walking;
	SpeakT = -1.0f;
	UAnimSequence* Walk = Loop(TEXT("walk_neutral_01"));
	if (!Walk) Walk = Loop(TEXT("walk_neutral_02"));
	SetLoop(Walk ? Walk : IdleLoop.Get(), 0.3f, Walk != nullptr);
}

bool URbPersonComponent::PlayerPosition(FVector& OutLocation, FVector& OutForward) const
{
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) return false;
	OutLocation = Cam->GetCameraLocation();
	OutForward = Cam->GetCameraRotation().Vector();
	return true;
}

// ---------------------------------------------------------------------------- tick

void URbPersonComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (GetSkinnedAsset())
	{
		TickWalk(DeltaTime);
		TickBody(DeltaTime);
		TickFace(DeltaTime);
		// Body turns towards its target facing at a comfortable pace.
		const float Diff = FMath::FindDeltaAngleDegrees(FacingYaw, TargetFacing);
		if (FMath::Abs(Diff) > 0.5f)
		{
			const float Rate = (Activity == ERbPersonActivity::Walking ? 260.0f : 110.0f) * DeltaTime;
			FacingYaw += FMath::Clamp(Diff, -Rate, Rate);
			SetWorldRotation(FRotator(0.0f, FacingYaw + MeshYaw, 0.0f));
		}
		if (URbPersonAnimInstance* Anim = Cast<URbPersonAnimInstance>(GetAnimInstance())) Anim->Pose = Pose;
	}
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void URbPersonComponent::TickBody(float DeltaTime)
{
	auto Advance = [](UAnimSequence* Seq, float& Time, float Dt, bool bWrap)
	{
		if (!Seq) return;
		const float Len = FMath::Max(Seq->GetPlayLength(), 0.05f);
		Time += Dt;
		Time = bWrap ? FMath::Fmod(Time, Len) : FMath::Min(Time, Len);
	};
	Advance(Pose.Base, Pose.BaseTime, DeltaTime, true);
	if (!Pose.Blend) return;
	Advance(Pose.Blend, Pose.BlendTime, DeltaTime, !bGestureOneShot);
	if (bGestureOneShot)
	{
		GestureLeft -= DeltaTime;
		const float Out = 0.45f;
		if (GestureLeft > Out) Pose.BlendAlpha = FMath::Min(1.0f, Pose.BlendAlpha + BlendSpeed * DeltaTime);
		else Pose.BlendAlpha = FMath::Max(0.0f, GestureLeft / Out) * FMath::Min(Pose.BlendAlpha + 0.001f, 1.0f);
		if (GestureLeft <= 0.0f)
		{
			Pose.Blend = nullptr;
			Pose.BlendAlpha = 0.0f;
			bGestureOneShot = false;
		}
		return;
	}
	Pose.BlendAlpha = FMath::Min(1.0f, Pose.BlendAlpha + BlendSpeed * DeltaTime);
	if (Pose.BlendAlpha >= 1.0f)
	{
		Pose.Base = Pose.Blend;
		Pose.BaseTime = Pose.BlendTime;
		Pose.bBaseInPlace = Pose.bBlendInPlace;
		Pose.Blend = nullptr;
		Pose.BlendAlpha = 0.0f;
	}
}

void URbPersonComponent::TickWalk(float DeltaTime)
{
	if (Activity != ERbPersonActivity::Walking || Route.IsEmpty()) return;
	FVector Here = GetComponentLocation();
	FVector Target = Route[RouteIndex];
	Target.Z = Here.Z;
	FVector PlayerAt, PlayerFwd;
	// Stop for the player rather than walk through them; look at them while waiting.
	if (PlayerPosition(PlayerAt, PlayerFwd))
	{
		const FVector ToPlayer = FVector(PlayerAt.X, PlayerAt.Y, Here.Z) - Here;
		const FVector Heading = (Target - Here).GetSafeNormal2D();
		if (ToPlayer.Size2D() < 125.0f && FVector::DotProduct(ToPlayer.GetSafeNormal2D(), Heading) > 0.2f)
		{
			// Someone walking up to the player has arrived once the player is in the way of the last step.
			if (RouteIndex == Route.Num() - 1 && (Target - Here).Size2D() < 220.0f)
			{
				Route.Reset();
				Activity = ERbPersonActivity::Standing;
				SetLoop(IdleLoop, 0.3f);
				if (OnArrived)
				{
					TFunction<void()> Done = MoveTemp(OnArrived);
					Done();
				}
				return;
			}
			if (WaitedForPlayer <= 0.0f) SetLoop(IdleLoop, 0.25f);
			WaitedForPlayer += DeltaTime;
			return;
		}
		if (WaitedForPlayer > 0.0f)
		{
			WaitedForPlayer = 0.0f;
			UAnimSequence* Walk = Loop(TEXT("walk_neutral_01"));
			SetLoop(Walk ? Walk : IdleLoop.Get(), 0.25f, Walk != nullptr);
		}
	}
	const FVector ToTarget = Target - Here;
	const float Dist = static_cast<float>(ToTarget.Size2D());
	const float Stepped = WalkSpeed * DeltaTime;
	TargetFacing = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X)));
	// Slow down while turning so corners are not taken sideways.
	const float Turn = FMath::Abs(FMath::FindDeltaAngleDegrees(FacingYaw, TargetFacing));
	const float Pace = Turn > 60.0f ? 0.35f : 1.0f;
	if (Dist <= Stepped * Pace + 2.0f)
	{
		SetWorldLocation(Target);
		if (++RouteIndex >= Route.Num())
		{
			Route.Reset();
			Activity = ERbPersonActivity::Standing;
			SetLoop(IdleLoop, 0.35f);
			if (OnArrived)
			{
				TFunction<void()> Done = MoveTemp(OnArrived);
				Done();
			}
		}
		return;
	}
	SetWorldLocation(Here + ToTarget / Dist * Stepped * Pace);
}

void URbPersonComponent::FindShapes()
{
	static const TCHAR* const Expression[NumExpressions] = {
		TEXT("EyeBlinkLeft"), TEXT("EyeBlinkRight"), TEXT("MouthSmileLeft"), TEXT("MouthSmileRight"), TEXT("MouthFrownLeft"), TEXT("MouthFrownRight"),
		TEXT("BrowDownLeft"), TEXT("BrowDownRight"), TEXT("BrowInnerUp"), TEXT("BrowOuterUpLeft"), TEXT("BrowOuterUpRight"),
		TEXT("CheekSquintLeft"), TEXT("CheekSquintRight"), TEXT("MouthPressLeft"), TEXT("MouthPressRight"), TEXT("JawOpen"), TEXT("EyeSquintLeft"), TEXT("EyeSquintRight")};
	static const TCHAR* const Viseme[NumVisemes] = {TEXT("VI_00_Sil"), TEXT("VI_01_PP"), TEXT("VI_02_FF"), TEXT("VI_03_TH"), TEXT("VI_04_DD"), TEXT("VI_05_KK"),
		TEXT("VI_06_CH"), TEXT("VI_07_SS"), TEXT("VI_08_nn"), TEXT("VI_09_RR"), TEXT("VI_10_aa"), TEXT("VI_11_E"), TEXT("VI_12_I"), TEXT("VI_13_O"), TEXT("VI_14_U")};
	ShapeNames.Init(NAME_None, NumExpressions + NumVisemes);
	ShapeWeights.Init(0.0f, NumExpressions + NumVisemes);
	VisemeWeights.Init(0.0f, NumVisemes);
	bHasVisemes = false;
	const USkeletalMesh* Asset = GetSkeletalMeshAsset();
	if (!Asset) return;
	// Imported shape names carry a library prefix (AK_09_EyeBlinkLeft, AA_VI_10_aa); match on the end.
	for (const TObjectPtr<UMorphTarget>& M : Asset->GetMorphTargets())
	{
		if (!M) continue;
		const FString Name = M->GetName();
		for (int32 i = 0; i < NumExpressions; ++i)
		{
			if (ShapeNames[i].IsNone() && Name.EndsWith(Expression[i]) && (Name.StartsWith(TEXT("AK_")) || Name.Len() == FCString::Strlen(Expression[i]))) ShapeNames[i] = M->GetFName();
		}
		for (int32 v = 0; v < NumVisemes; ++v)
		{
			if (ShapeNames[NumExpressions + v].IsNone() && Name.EndsWith(Viseme[v])) ShapeNames[NumExpressions + v] = M->GetFName();
		}
	}
	bHasVisemes = !ShapeNames[NumExpressions + 10].IsNone();
}

void URbPersonComponent::SetShape(int32 Shape, float Weight)
{
	if (!ShapeNames.IsValidIndex(Shape) || ShapeNames[Shape].IsNone()) return;
	if (FMath::IsNearlyEqual(ShapeWeights[Shape], Weight, 0.002f)) return;
	ShapeWeights[Shape] = Weight;
	SetMorphTarget(ShapeNames[Shape], Weight, false);
}

void URbPersonComponent::TickFace(float DeltaTime)
{
	const bool bCalm = RbUI::IsReducedMotion();
	// ---- where to look ----
	FVector PlayerAt, PlayerFwd;
	float WantYaw = 0.0f, WantPitch = 0.0f;
	bool bLookAtPlayer = false;
	if (PlayerPosition(PlayerAt, PlayerFwd) && Activity != ERbPersonActivity::Walking)
	{
		const FVector Eye = GetEyeLocation();
		const FVector ToPlayer = PlayerAt - Eye;
		const float Dist = static_cast<float>(ToPlayer.Size());
		// In component space the face points along +Y.
		const FVector L = GetComponentTransform().InverseTransformVectorNoScale(ToPlayer);
		const float Yaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(-L.X, L.Y)));
		const float Pitch = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(L.Z, FVector2D(L.X, L.Y).Size())));
		// People notice someone close by who is in front of them, and always the person they are talking to.
		bLookAtPlayer = bEngaged || (Dist < LookRange && FMath::Abs(Yaw) < 100.0f && (Dist < 260.0f || FVector::DotProduct(PlayerFwd, -ToPlayer.GetSafeNormal()) > 0.7f));
		if (bLookAtPlayer)
		{
			WantYaw = Yaw;
			WantPitch = Pitch;
			if (bEngaged && Activity == ERbPersonActivity::Standing && FMath::Abs(Yaw) > 35.0f) TurnTo(PlayerAt);
		}
	}
	else if (Activity == ERbPersonActivity::Walking && PlayerPosition(PlayerAt, PlayerFwd) && WaitedForPlayer > 0.0f)
	{
		const FVector L = GetComponentTransform().InverseTransformVectorNoScale(PlayerAt - GetEyeLocation());
		WantYaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(-L.X, L.Y)));
		WantPitch = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(L.Z, FVector2D(L.X, L.Y).Size())));
		bLookAtPlayer = true;
	}
	LookWeight = FMath::FInterpTo(LookWeight, bLookAtPlayer ? 1.0f : 0.0f, DeltaTime, 2.5f);

	// ---- saccades: small jumps when engaged, idle glances otherwise ----
	NextSaccade -= DeltaTime;
	if (NextSaccade <= 0.0f && !bCalm)
	{
		NextSaccade = bLookAtPlayer ? Rand.FRandRange(0.5f, 1.8f) : Rand.FRandRange(1.2f, 3.5f);
		Saccade = bLookAtPlayer ? FVector2f(Rand.FRandRange(-2.5f, 2.5f), Rand.FRandRange(-1.5f, 1.5f))
			: FVector2f(Rand.FRandRange(-16.0f, 16.0f), Rand.FRandRange(-8.0f, 4.0f));
		if (!bLookAtPlayer && FMath::Abs(Saccade.X) > 10.0f) NextBlink = FMath::Min(NextBlink, 0.05f); // big glances come with a blink
	}
	const float HeadShare = bEngaged ? 0.85f : 0.6f;
	const FVector2f Want(FMath::Clamp(WantYaw, -60.0f, 60.0f) * LookWeight, FMath::Clamp(WantPitch, -25.0f, 20.0f) * LookWeight);
	const FVector2f HeadWant = Want * HeadShare + (bLookAtPlayer ? FVector2f::ZeroVector : Saccade * 0.35f);
	HeadLook.X = FMath::FInterpTo(HeadLook.X, HeadWant.X, DeltaTime, 4.0f);
	HeadLook.Y = FMath::FInterpTo(HeadLook.Y, HeadWant.Y, DeltaTime, 4.0f);
	float HeadYaw = HeadLook.X, HeadPitch = HeadLook.Y;
	// Nod or head shake on a reaction.
	if (NodT >= 0.0f)
	{
		NodT += DeltaTime;
		const float Fade = FMath::Max(0.0f, 1.0f - NodT / 1.2f);
		if (NodSign > 0) HeadPitch += -7.0f * FMath::Sin(NodT * 2.0f * PI / 0.6f) * Fade;
		else HeadYaw += 8.0f * FMath::Sin(NodT * 2.0f * PI / 0.5f) * Fade;
		if (NodT > 1.2f) NodT = -1.0f;
	}
	if (Activity == ERbPersonActivity::Walking)
	{
		HeadYaw *= 0.5f;
		HeadPitch *= 0.5f;
	}
	Pose.HeadYaw = HeadYaw;
	Pose.HeadPitch = HeadPitch;
	// Eyes cover what the head does not, plus the saccade.
	Pose.EyeYaw = FMath::Clamp(Want.X - HeadLook.X + Saccade.X * (bLookAtPlayer ? 1.0f : 0.65f), -22.0f, 22.0f);
	Pose.EyePitch = FMath::Clamp(Want.Y - HeadLook.Y + Saccade.Y * (bLookAtPlayer ? 1.0f : 0.65f), -14.0f, 12.0f);

	// ---- blinks ----
	NextBlink -= DeltaTime;
	float Blink = 0.0f;
	if (NextBlink <= 0.0f && BlinkT < 0.0f) BlinkT = 0.0f;
	if (BlinkT >= 0.0f)
	{
		BlinkT += DeltaTime;
		constexpr float BlinkLength = 0.16f;
		Blink = FMath::Sin(FMath::Min(BlinkT / BlinkLength, 1.0f) * PI);
		if (BlinkT >= BlinkLength)
		{
			BlinkT = -1.0f;
			NextBlink = Rand.FRand() < 0.15f ? 0.22f : Rand.FRandRange(2.2f, 5.5f); // sometimes a double blink
		}
	}
	// Eyelids follow a downward look a little.
	const float LidDrop = FMath::Clamp(-Pose.EyePitch / 30.0f, 0.0f, 0.3f);
	SetShape(BlinkL, FMath::Max(Blink, LidDrop));
	SetShape(BlinkR, FMath::Max(Blink, LidDrop));

	// ---- mood and expressions ----
	if (MoodHold > 0.0f)
	{
		MoodHold -= DeltaTime;
		if (MoodHold <= 0.0f) MoodTarget = bEngaged ? 0.15f : 0.1f;
	}
	Mood = FMath::FInterpTo(Mood, MoodTarget, DeltaTime, 3.0f);
	const float Pleased = FMath::Max(Mood, 0.0f), Unhappy = FMath::Max(-Mood, 0.0f);
	SetShape(SmileL, 0.5f * Pleased);
	SetShape(SmileR, 0.5f * Pleased);
	SetShape(CheekSquintL, 0.3f * Pleased);
	SetShape(CheekSquintR, 0.3f * Pleased);
	SetShape(SquintL, 0.15f * Pleased);
	SetShape(SquintR, 0.15f * Pleased);
	SetShape(FrownL, 0.45f * Unhappy);
	SetShape(FrownR, 0.45f * Unhappy);
	SetShape(BrowDownL, 0.55f * Unhappy);
	SetShape(BrowDownR, 0.55f * Unhappy);
	SetShape(PressL, 0.3f * Unhappy);
	SetShape(PressR, 0.3f * Unhappy);

	// ---- lip-sync ----
	int32 Current = 0;
	float Brows = 0.0f;
	if (SpeakT >= 0.0f)
	{
		SpeakT += DeltaTime;
		for (int32 i = 0; i < VisemeTimes.Num() && VisemeTimes[i] <= SpeakT; ++i) Current = Visemes[i];
		// Brows lift a little on stressed syllables.
		Brows = 0.18f * FMath::Max(0.0f, FMath::Sin(SpeakT * 3.1f));
		if (SpeakT > SpeakLength)
		{
			SpeakT = -1.0f;
			Current = 0;
			if (Activity == ERbPersonActivity::Standing) SetLoop(RestLoop(), 0.5f);
		}
	}
	SetShape(BrowInnerUp, Brows + 0.25f * Unhappy);
	SetShape(BrowOuterUpL, Brows * 0.7f);
	SetShape(BrowOuterUpR, Brows * 0.7f);
	float Jaw = 0.0f;
	for (int32 v = 0; v < NumVisemes; ++v)
	{
		const float Target = (v == Current && Current != 0) ? 0.75f : 0.0f;
		VisemeWeights[v] = FMath::FInterpTo(VisemeWeights[v], Target, DeltaTime, 22.0f);
		SetShape(NumExpressions + v, VisemeWeights[v]);
		Jaw += VisemeWeights[v] * VisemeJaw[v];
	}
	Pose.Jaw = bHasVisemes ? 0.0f : FMath::Clamp(Jaw, 0.0f, 1.0f);
}
