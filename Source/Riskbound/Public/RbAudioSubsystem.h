// Sound for the whole game: adaptive music that follows the state of the career (calm, focus,
// tension, crisis) with crossfades, office ambience, UI and event cues, and footsteps.
// Sounds are imported assets under /Game/Riskbound/Audio (Scripts/ImportAssets.py, sources and
// licences in Docs/AssetCredits.md). A missing asset is skipped silently: audio never blocks play.
// Master volume is the engine's transient primary volume (URbGameSubsystem::ApplySettings).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "RbAudioSubsystem.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class USoundWave;
class USceneComponent;

UENUM(BlueprintType)
enum class ERbCue : uint8
{
	Click,
	Confirm,
	Back,
	Notice,
	Message,
	Incident,
	Promotion,
	MissionPassed,
	MissionFailed,
	Burnout,
	Footstep,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ERbMusicMood : uint8
{
	Silent,
	Menu,
	Focus,
	Tension,
	Crisis,
	Count UMETA(Hidden)
};

UCLASS()
class RISKBOUND_API URbAudioSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Plays a one-shot cue (2D). Safe to call from anywhere with any UObject in the game instance.
	static void Cue(const UObject* WorldContext, ERbCue Cue);

	UFUNCTION(BlueprintCallable, Category = "Riskbound|Audio")
	void PlayCue(ERbCue Cue);

	// The mood the music should be in for the current game state (also used by tests).
	UFUNCTION(BlueprintPure, Category = "Riskbound|Audio")
	ERbMusicMood DesiredMood() const;

	UFUNCTION(BlueprintPure, Category = "Riskbound|Audio")
	ERbMusicMood CurrentMood() const { return Mood; }

	// A looping positional ambience (servers, HVAC) owned by the level; attenuated over Radius cm.
	UAudioComponent* SpawnAmbience(UWorld* World, const FName& Name, const FVector& Location, float Radius, float Volume);

	// A colleague's recorded line (Scripts/BuildVoices.py): /Game/Riskbound/Audio/Voice/VO_<Key>, Key from
	// rb::VoiceLineKey. Null when the line has no clip (the person then speaks with text-timed lip-sync only).
	static USoundWave* FindVoice(const FString& Key);
	// Plays it from the speaker's head, full volume at conversation distance and fading across a room.
	UAudioComponent* PlayVoice(USoundWave* Voice, USceneComponent* AttachTo, FName Socket);

	// Number of sound assets that loaded (for the self-test report).
	int32 LoadedSoundCount() const;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }

private:
	USoundBase* Load(const TCHAR* Path);
	void SetMood(ERbMusicMood NewMood);
	void EnsureRoomTone();

	UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> CueSounds;     // indexed by ERbCue (Footstep: first of the set)
	UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> FootstepSounds;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> MoodSounds;    // indexed by ERbMusicMood
	UPROPERTY(Transient) TMap<FName, TObjectPtr<USoundBase>> Ambiences;
	UPROPERTY(Transient) TObjectPtr<USoundBase> RoomTone;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> MusicA;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> MusicB;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> RoomToneComponent;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> Attenuation;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> VoiceAttenuation;

	ERbMusicMood Mood = ERbMusicMood::Silent;
	bool bMusicOnA = true;
	float MoodCheckTimer = 0.0f;
	float MoodHoldTimer = 0.0f; // keeps a mood for a while so music does not flap between states
	double LastCueTime[static_cast<int32>(ERbCue::Count)] = {};
	int32 LastFootstep = -1;
};
