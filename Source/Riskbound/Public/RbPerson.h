// A person in the office: the cast body (RbCast) with a face and body language that react to the player.
// One component does it all so Priya, interviewees and background staff behave alike:
//   - body: a looping idle that crossfades to talking, listening, reaction gestures, sitting and walking loops;
//   - face: blinks, eye saccades, head and eyes that follow the player at conversation distance,
//     text-driven lip-sync (Rocketbox visemes) and expressions (ARKit-style shapes);
//   - walking: a route of waypoints at walking pace, stopping for the player and looking at them.
// Face shapes need the "_facial" avatars (Scripts/FetchCharacters.py); without them the head and eyes still
// move and the mouth moves on the jaw bone. Nothing here changes the simulation (Docs/Architecture.md).
#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "CoreMinimal.h"
#include "RbCast.h"
#include "RbPerson.generated.h"

class UAnimSequence;
class UAudioComponent;

// What the evaluation thread needs each frame, copied from the component.
struct FRbPersonPose
{
	TObjectPtr<UAnimSequence> Base = nullptr;   // current loop
	float BaseTime = 0.0f;
	TObjectPtr<UAnimSequence> Blend = nullptr;  // incoming loop or a one-shot gesture
	float BlendTime = 0.0f;
	float BlendAlpha = 0.0f;                    // 0 = all Base, 1 = all Blend
	bool bBaseInPlace = false;                  // pin the pelvis (walking loops carry travel)
	bool bBlendInPlace = false;
	float HeadYaw = 0.0f, HeadPitch = 0.0f;     // degrees, component space, + = left / up
	float EyeYaw = 0.0f, EyePitch = 0.0f;
	float Jaw = 0.0f;                           // 0..1, only used when the mesh has no viseme shapes
};

struct FRbPersonAnimProxy : public FAnimInstanceProxy
{
	FRbPersonAnimProxy() = default;
	explicit FRbPersonAnimProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

protected:
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	FRbPersonPose Pose;
	void PinPelvis(FPoseContext& Context) const;
	void ApplyLook(FPoseContext& Context) const;
};

UCLASS(Transient, NotBlueprintable)
class RISKBOUND_API URbPersonAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	FRbPersonPose Pose; // written by URbPersonComponent each tick

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FRbPersonAnimProxy(this); }
};

UENUM()
enum class ERbPersonActivity : uint8
{
	Standing,
	Sitting,
	Walking
};

UCLASS(ClassGroup = (Riskbound))
class RISKBOUND_API URbPersonComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	URbPersonComponent();

	// Body, idle and identity. PersonId is the People.json id ("" for background staff); FacingYaw is
	// the world yaw the person faces (the mesh's own yaw offset is added here). Seed varies timing.
	void Setup(const RbCast::FLook& Look, const FString& InPersonId, float FacingYaw, int32 Seed);
	// Sit at a table or on a chair (Rocketbox sit_* loops); the component should stand on the chair's spot.
	void SitDown(bool bAtTable);

	// Speaks a line: talking gestures and lip-sync. A recorded clip for the line (URbAudioSubsystem::FindVoice)
	// plays from the head and the lip-sync is stretched to its length; without one, the length comes from the text.
	void Say(const FString& Text);
	void StopTalking();
	// Attentive while the player talks to them: faces the player and listens between lines.
	void SetEngaged(bool bInEngaged);
	// Reaction to the player's choice: +1 pleased (nod, smile), -1 unhappy (shake, frown), 0 thoughtful.
	void React(int32 Sign);
	// A one-shot gesture by animation name (e.g. "gestic_shrug_01"); false if that loop is missing.
	bool PlayGesture(const TCHAR* Name, float BlendIn = 0.25f);

	// Walks through world-space points (Z ignored, kept on this component's floor height), then calls OnArrive.
	void WalkRoute(const TArray<FVector>& Points, TFunction<void()> OnArrive = nullptr);
	bool IsWalking() const { return Activity == ERbPersonActivity::Walking; }
	void TurnTo(const FVector& WorldPoint);

	const FString& GetPersonId() const { return PersonId; }
	bool HasFaceShapes() const { return bHasVisemes; }

	// The office person with this People.json id in World (the first one found), or null.
	static URbPersonComponent* Find(const UWorld* World, const FString& InPersonId);
	// Where to look at to meet this person's eyes.
	FVector GetEyeLocation() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

private:
	// ---- body ----
	void SetLoop(UAnimSequence* Loop, float BlendSeconds = 0.35f, bool bInPlace = false);
	UAnimSequence* Loop(const TCHAR* Name);
	UAnimSequence* RestLoop(); // what to return to after talking or a gesture
	void TickBody(float DeltaTime);
	FRbPersonPose Pose;
	float BlendSpeed = 0.0f;        // alpha per second while blending
	bool bGestureOneShot = false;   // Blend is a one-shot gesture that fades back to Base
	float GestureLeft = 0.0f;
	TObjectPtr<UAnimSequence> IdleLoop;
	UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> Loaded; // keeps every loop in use alive
	TMap<FString, TObjectPtr<UAnimSequence>> LoopCache;
	ERbPersonActivity Activity = ERbPersonActivity::Standing;
	bool bAtTable = false;
	bool bFemale = false;
	bool bCanAnimate = false;       // Rocketbox body (the mannequin stand-in only idles)
	float MeshYaw = -90.0f;
	float FacingYaw = 0.0f;         // current world facing
	float TargetFacing = 0.0f;
	FString PersonId;

	// ---- face ----
	void FindShapes();
	void TickFace(float DeltaTime);
	void SetShape(int32 Shape, float Weight);
	enum EShape : int32
	{
		BlinkL, BlinkR, SmileL, SmileR, FrownL, FrownR, BrowDownL, BrowDownR, BrowInnerUp, BrowOuterUpL, BrowOuterUpR,
		CheekSquintL, CheekSquintR, PressL, PressR, JawOpen, SquintL, SquintR, NumExpressions
	};
	static constexpr int32 NumVisemes = 15; // Sil PP FF TH DD KK CH SS nn RR aa E I O U
	TArray<FName> ShapeNames;               // NumExpressions + NumVisemes, None when the mesh lacks it
	TArray<float> ShapeWeights;
	bool bHasVisemes = false;
	FRandomStream Rand;
	float NextBlink = 2.0f, BlinkT = -1.0f;
	float NextSaccade = 1.0f;
	FVector2f Saccade = FVector2f::ZeroVector;
	FVector2f HeadLook = FVector2f::ZeroVector; // current head yaw/pitch
	float LookWeight = 0.0f;
	bool bEngaged = false;
	float Mood = 0.0f, MoodTarget = 0.0f, MoodHold = 0.0f;  // -1 unhappy .. +1 pleased
	float NodT = -1.0f; int32 NodSign = 0;    // procedural nod (+1) or head shake (-1)
	// Lip-sync: one viseme per letter group at a speaking rate, or spread over the recorded clip.
	void StopVoice();
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> VoiceAudio;
	TArray<uint8> Visemes;
	TArray<float> VisemeTimes;
	float SpeakT = -1.0f, SpeakLength = 0.0f;
	TArray<float> VisemeWeights;

	// ---- walking ----
	TArray<FVector> Route;
	int32 RouteIndex = 0;
	TFunction<void()> OnArrived;
	float WaitedForPlayer = 0.0f;
	void TickWalk(float DeltaTime);
	bool PlayerPosition(FVector& OutLocation, FVector& OutForward) const;
};
