// Optional AI colleagues (idea 29, owner-approved, off by default): free-typed conversations with
// Valtren's people through the Anthropic Messages API, using the player's own API key.
// The key is stored only in Saved/Config/RiskboundAi.cfg on this PC (never in the save, never logged).
// Nothing is sent unless the player turns the feature on and enters a key. Docs/AiColleagues.md.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "RbAiPrompt.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RbAiColleaguesSubsystem.generated.h"

UCLASS()
class RISKBOUND_API URbAiColleaguesSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool IsEnabled() const { return bEnabled; }
	bool HasKey() const { return !ApiKey.IsEmpty(); }
	bool IsReady() const { return bEnabled && HasKey(); }
	void SetEnabled(bool bOn);
	void SetApiKey(const FString& Key); // empty clears it
	FString MaskedKey() const;

	// Conversation with one person; the transcript lives for the session only.
	const TArray<rb::AiTurn>& Transcript(const FString& PersonId) const;
	bool IsWaiting() const { return bWaiting; }
	// Sends the player's line. OnDone runs on the game thread with an error message ("" on success).
	void Ask(const FString& PersonId, const FString& Line, TFunction<void(const FString& Error)> OnDone);

private:
	void Load();
	void Store() const;
	FString ConfigPath() const;

	bool bEnabled = false;
	FString ApiKey;
	bool bWaiting = false;
	TMap<FString, TArray<rb::AiTurn>> Transcripts;
};
