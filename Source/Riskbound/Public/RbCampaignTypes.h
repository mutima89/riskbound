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
	EvidenceElsewhere,
	PresentInPerson,
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
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 WeeksToEscalate = -1; // open crisis with a deadline
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
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Kind; // single, multi, numeric (type a figure), rating (likelihood and impact 1-5)
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Unit; // numeric: shown next to the input
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
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString At;         // HQ location id; empty: the workstation
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString AtName;     // "Risk monitoring room"
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString PersonName; // interview with this person, may be empty
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
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString PresentAt;     // presented in person at this location id
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString PresentAtName;
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
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FString> Fallout;   // weak answers that may cause an incident
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FString> Escalated; // crisis titles that escalated
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 NewMessages = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bBurnout = false;
};

// ---- Living world (Docs/GameDesign.md "Living world") ----

USTRUCT(BlueprintType)
struct FRbInboxMessageView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Category;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString FromName;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString FromRole;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Subject;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Body;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Week = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bRead = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bNeedsReply = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 WeeksLeft = -1; // to reply; 0 = due this week
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FRbMissionOptionView> Choices; // replies (never the default "no reply")
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString ChosenLabel;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Outcome;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bAutoResolved = false;
};

USTRUCT(BlueprintType)
struct FRbPersonView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Role;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Bio;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Relation = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString RelationLabel;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bExecutive = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bPersonal = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Faction;
};

USTRUCT(BlueprintType)
struct FRbClueView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Summary;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Chapter = 1;
};

USTRUCT(BlueprintType)
struct FRbNewsView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Week = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Text;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Tone = 0;
};

USTRUCT(BlueprintType)
struct FRbWorldView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Week = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Stress = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Burnouts = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double ShareIndex = 100.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double ShareChange = 0.0; // since the previous point
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<float> ShareHistory;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FRbNewsView> News; // newest first
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Unread = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 AwaitingReply = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString StoryTitle;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 CluesFound = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 CluesTotal = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double Morale = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double Reputation = 0.0;
};

// ---- Phase 3: skills, backgrounds, conversations ----

USTRUCT(BlueprintType)
struct FRbSkillView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Branch;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Description;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString RequiresName; // skill needed first, may be empty
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bLearned = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bCanLearn = false;
};

USTRUCT(BlueprintType)
struct FRbBackgroundView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Description;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString StartingSkill; // display name, may be empty
};

USTRUCT(BlueprintType)
struct FRbDialogueResponseView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Label;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Tone;       // Diplomatic, Direct, Analytical
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString SkillName;  // unlocked by this skill, may be empty
};

USTRUCT(BlueprintType)
struct FRbConversationView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bFound = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bEnded = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString PersonName;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString PersonRole;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString NodeId;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Line;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FRbDialogueResponseView> Responses;
};
