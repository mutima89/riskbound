// Blueprint-facing views of RiskboundSim state. These are copies for display only:
// nothing here computes risk, scores or consequences.
#pragma once

#include "CoreMinimal.h"
#include "RbTypes.generated.h"

UENUM(BlueprintType)
enum class ERbDifficulty : uint8
{
	Analyst,
	Professional,
	Executive
};

UENUM(BlueprintType)
enum class ERbControlRating : uint8
{
	Ineffective,
	PartiallyEffective,
	Effective
};

UENUM(BlueprintType)
enum class ERbSubmitResult : uint8
{
	Ok,
	NoGame,
	AlreadySubmitted,
	ManagerNotMet,
	IncidentReportNotReviewed,
	RegisterNotInspected,
	IncompleteSubmission
};

UENUM(BlueprintType)
enum class ERbLoadOutcome : uint8
{
	Ok,
	RecoveredFromBackup,
	NotFound,
	Corrupt,
	UnsupportedVersion
};

// What talking to the manager produced, so the UI knows which dialogue to show.
UENUM(BlueprintType)
enum class ERbManagerTalk : uint8
{
	Briefing,  // first meeting: MeetManager() was just called
	Reminder,  // met, assessment not yet submitted
	Debrief    // submitted: show the debrief
};

USTRUCT(BlueprintType)
struct FRbOptionView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Label;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Description;
};

USTRUCT(BlueprintType)
struct FRbEvidenceView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Location;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Body;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bControlEvidence = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bViewed = false;
};

USTRUCT(BlueprintType)
struct FRbObjectiveView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Text;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bComplete = false;
};

// The player's assessment form, mirrored 1:1 onto rb::m001::Submission.
USTRUCT(BlueprintType)
struct FRbSubmissionInput
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") TArray<FString> MissingInfo;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") int32 InherentLikelihood = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") int32 InherentImpact = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") TArray<ERbControlRating> ControlRatings;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") int32 ResidualLikelihood = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") int32 ResidualImpact = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") FString TreatmentId;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") FString ActionOwnerId;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") TArray<FString> RationaleTagIds;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") FString RationaleText;
};

USTRUCT(BlueprintType)
struct FRbAppraisalView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString TreatmentLabel;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double AnnualRiskReduction = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double PvBenefit = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double PvCost = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double Npv = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double Roi = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double BenefitCostRatio = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bCostEffective = false;
};

USTRUCT(BlueprintType)
struct FRbCriterionView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Score = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Weight = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Feedback;
};

// Only populated after Submit(); bAvailable is false before that so the rubric never leaks.
USTRUCT(BlueprintType)
struct FRbDebriefView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bPassed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 OverallScore = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FRbCriterionView> Criteria;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FString> Assumptions;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FString> TradeOffs;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FString> Improvements;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FRbAppraisalView Chosen;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FRbAppraisalView Recommended;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double BaselineExpectedLoss = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") double BaselineWorstCase = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 XpAwarded = 0;
};

USTRUCT(BlueprintType)
struct FRbCompetencyView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Score = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Assessments = 0;
};

USTRUCT(BlueprintType)
struct FRbCareerView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Level = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 Xp = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FString> PassedMissions;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FRbCompetencyView> Competencies;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FString> UnmetPromotionGates;
};

USTRUCT(BlueprintType)
struct FRbRegisterEntryView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") bool bFound = false;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Cause;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Event;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Consequence;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Category;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Owner;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 InherentLikelihood = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 InherentImpact = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString InherentRating;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 ResidualLikelihood = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") int32 ResidualImpact = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString ResidualRating;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Appetite;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Treatment;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString Status;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") FString ActionOwner;
	UPROPERTY(BlueprintReadOnly, Category = "Riskbound") TArray<FString> Controls;
};

USTRUCT(BlueprintType)
struct FRbSettingsView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") float MouseSensitivity = 1.0f;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") float TextScale = 1.0f;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") float MasterVolume = 0.8f;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") bool bReducedMotion = false;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") bool bHints = true;
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") bool bFullscreen = true;
	// 0 Low .. 3 Epic (engine scalability presets; Low turns Lumen off).
	UPROPERTY(BlueprintReadWrite, Category = "Riskbound") int32 GraphicsQuality = 2;
};
