// View models for campaign missions (Gate 3). The rubric (option scores, weights) is never
// exposed before submission; the debrief view carries it afterwards.
#pragma once

#include "CoreMinimal.h"
#include "RbTypes.h" // FRbCompetencyView
#include "RbCampaignTypes.generated.h"

UENUM(BlueprintType)
enum class ERbMissionResult : uint8
{
	Ok,
	Unavailable,
	AnotherMissionActive,
	NotActive,
	UnknownEvidence,
	RequiredEvidenceNotViewed,
	UnansweredQuestion,
	UnknownOption,
	SingleNeedsOneOption,
	UnknownMission
};

USTRUCT(BlueprintType)
struct FRbMissionSummary
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Kind;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Department;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 MinLevel = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bReview = false; // passed case reopened to build evidence
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bActive = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bPassed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Attempts = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 BestScore = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FString> Reasons; // why it is closed
};

USTRUCT(BlueprintType)
struct FRbMissionOptionView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Label;
};

USTRUCT(BlueprintType)
struct FRbMissionQuestionView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Prompt;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bMulti = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FRbMissionOptionView> Options;
};

USTRUCT(BlueprintType)
struct FRbMissionEvidenceView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Body;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bRequired = true;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bViewed = false;
};

USTRUCT(BlueprintType)
struct FRbMissionView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bFound = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Kind;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Department;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FString> Briefing; // includes notes from earlier decisions
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FRbMissionEvidenceView> Evidence;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FRbMissionQuestionView> Questions;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bActive = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double DecisionAuthority = 0.0;
};

USTRUCT(BlueprintType)
struct FRbMissionAnswer
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") FString QuestionId;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") TArray<FString> OptionIds;
};

USTRUCT(BlueprintType)
struct FRbMissionCriterionView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Prompt;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Competency;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Score = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Weight = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bAuthorityExceeded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FString> Feedback;
};

USTRUCT(BlueprintType)
struct FRbMissionDebriefView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString MissionId;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Score = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 PassMark = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bPassed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FRbMissionCriterionView> Criteria;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FRbCompetencyView> CompetencyEvidence;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 XpAwarded = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Lesson;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FString> Consequences; // resolved while the mission ran
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bPromoted = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bReview = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Level = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString LevelTitle;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FString> PromotionGaps;
};
