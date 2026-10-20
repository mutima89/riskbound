#include "RbGameSubsystem.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "RbConvert.h"
#include "RbMission001.h"
#include "RbSaveSubsystem.h"
#include "RbLog.h"
#include "RbUIStyle.h"

#include <algorithm>

using namespace RbConvert;
namespace m001 = rb::m001;

const TCHAR* URbGameSubsystem::CheckpointStart = TEXT("M001:start");
const TCHAR* URbGameSubsystem::CheckpointMetManager = TEXT("M001:met_manager");
const TCHAR* URbGameSubsystem::CheckpointSubmitted = TEXT("M001:submitted");
const TCHAR* URbGameSubsystem::CheckpointDebriefed = TEXT("M001:debriefed");

namespace
{
rb::EDifficulty ToSim(ERbDifficulty D)
{
	switch (D)
	{
	case ERbDifficulty::Analyst: return rb::EDifficulty::Analyst;
	case ERbDifficulty::Executive: return rb::EDifficulty::Executive;
	default: return rb::EDifficulty::Professional;
	}
}

ERbSubmitResult FromSim(m001::ESubmitError E)
{
	switch (E)
	{
	case m001::ESubmitError::None: return ERbSubmitResult::Ok;
	case m001::ESubmitError::AlreadySubmitted: return ERbSubmitResult::AlreadySubmitted;
	case m001::ESubmitError::ManagerNotMet: return ERbSubmitResult::ManagerNotMet;
	case m001::ESubmitError::IncidentReportNotReviewed: return ERbSubmitResult::IncidentReportNotReviewed;
	case m001::ESubmitError::RegisterNotInspected: return ERbSubmitResult::RegisterNotInspected;
	case m001::ESubmitError::IncompleteSubmission: return ERbSubmitResult::IncompleteSubmission;
	}
	return ERbSubmitResult::IncompleteSubmission;
}

m001::Submission ToSim(const FRbSubmissionInput& In)
{
	m001::Submission S;
	S.MissingInfo = ToStdVector(In.MissingInfo);
	S.InherentLikelihood = In.InherentLikelihood;
	S.InherentImpact = In.InherentImpact;
	for (ERbControlRating R : In.ControlRatings)
	{
		S.ControlRatings.push_back(static_cast<rb::EControlEffectiveness>(static_cast<uint8>(R)));
	}
	S.ResidualLikelihood = In.ResidualLikelihood;
	S.ResidualImpact = In.ResidualImpact;
	S.TreatmentId = ToStd(In.TreatmentId);
	S.ActionOwnerId = ToStd(In.ActionOwnerId);
	S.RationaleTagIds = ToStdVector(In.RationaleTagIds);
	S.RationaleText = ToStd(In.RationaleText);
	return S;
}

TArray<FRbOptionView> ToViews(const std::vector<m001::Option>& Options)
{
	TArray<FRbOptionView> Out;
	for (const m001::Option& O : Options)
	{
		Out.Add({ToFString(O.Id), ToFString(O.Label), ToFString(O.Description)});
	}
	return Out;
}

FRbAppraisalView ToView(const rb::TreatmentAppraisal& A, const FString& Label)
{
	FRbAppraisalView V;
	V.TreatmentLabel = Label;
	V.AnnualRiskReduction = A.AnnualRiskReduction;
	V.PvBenefit = A.PvBenefit;
	V.PvCost = A.PvCost;
	V.Npv = A.Npv;
	V.Roi = A.Roi;
	V.BenefitCostRatio = A.BenefitCostRatio;
	V.bCostEffective = A.bCostEffective;
	return V;
}

FString OptionLabel(const std::vector<m001::Option>& Options, const std::string& Id)
{
	for (const m001::Option& O : Options)
	{
		if (O.Id == Id) return ToFString(O.Label);
	}
	return ToFString(Id);
}
} // namespace

void URbGameSubsystem::NewGame(ERbDifficulty Difficulty, const FString& ProfileName)
{
	const rb::PlayerSettings KeptSettings = State.Settings;
	State = rb::NewGame(ToStd(ProfileName), ToSim(Difficulty), rb::GameState().Seed);
	State.Settings = KeptSettings;
	if (Difficulty == ERbDifficulty::Executive)
	{
		State.Settings.bHints = false; // Executive: no hints by default (Docs/GameDesign.md)
	}
	State.Checkpoint = ToStd(CheckpointStart);
	bHasGame = true;
	AfterAction(false);
}

ERbLoadOutcome URbGameSubsystem::Continue(FString& OutMessage)
{
	URbSaveSubsystem* Save = GetGameInstance()->GetSubsystem<URbSaveSubsystem>();
	return Save ? Save->Load(OutMessage) : ERbLoadOutcome::NotFound;
}

void URbGameSubsystem::SetLoadedState(rb::GameState&& Loaded)
{
	State = MoveTemp(Loaded);
	bHasGame = true;
	OnStateChanged.Broadcast();
}

ERbManagerTalk URbGameSubsystem::TalkToManager()
{
	m001::MissionState& M = State.Mission001;
	if (!M.IsComplete(m001::EObjective::MeetManager))
	{
		M.MeetManager();
		State.Checkpoint = ToStd(CheckpointMetManager);
		AfterAction(true);
		return ERbManagerTalk::Briefing;
	}
	return M.bSubmitted ? ERbManagerTalk::Debrief : ERbManagerTalk::Reminder;
}

bool URbGameSubsystem::ViewEvidence(const FString& EvidenceId)
{
	const bool bKnown = State.Mission001.ViewEvidence(ToStd(EvidenceId));
	if (bKnown)
	{
		AfterAction(false);
	}
	return bKnown;
}

ERbSubmitResult URbGameSubsystem::ValidateAssessment(const FRbSubmissionInput& Input) const
{
	if (!bHasGame) return ERbSubmitResult::NoGame;
	return FromSim(State.Mission001.Validate(ToSim(Input)));
}

ERbSubmitResult URbGameSubsystem::SubmitAssessment(const FRbSubmissionInput& Input)
{
	if (!bHasGame) return ERbSubmitResult::NoGame;
	const ERbSubmitResult Result =
		FromSim(State.Mission001.Submit(ToSim(Input), State.Difficulty, State.Career, State.Org, State.Register));
	if (Result == ERbSubmitResult::Ok)
	{
		State.Checkpoint = ToStd(CheckpointSubmitted);
		AfterAction(true);
	}
	return Result;
}

void URbGameSubsystem::MarkDebriefReceived()
{
	if (!State.Mission001.bSubmitted || IsDebriefed()) return;
	State.Checkpoint = ToStd(CheckpointDebriefed);
	AfterAction(true);
}

void URbGameSubsystem::AfterAction(bool bAutosave)
{
	for (const std::string& Id : rb::EvaluateAchievements(State))
	{
		for (const rb::AchievementDef& Def : rb::AchievementDefs())
		{
			if (Def.Id == Id) Notify(FString::Printf(TEXT("Achievement unlocked: %s"), *ToFString(Def.Title)));
		}
	}
	if (bAutosave)
	{
		if (URbSaveSubsystem* Save = GetGameInstance()->GetSubsystem<URbSaveSubsystem>())
		{
			Save->Autosave();
		}
	}
	OnStateChanged.Broadcast();
}

void URbGameSubsystem::Notify(const FString& Message)
{
	UE_LOG(LogRiskbound, Display, TEXT("Riskbound: %s"), *Message);
	OnNotice.Broadcast(Message);
}

FString URbGameSubsystem::GetCurrentObjectiveText() const
{
	if (!bHasGame) return FString();
	if (IsDebriefed()) return TEXT("Mission 001 complete. Further missions arrive in a later build.");
	if (State.Mission001.bSubmitted) return TEXT("Receive your debrief from Priya Okafor in her office");
	const int32 Current = State.Mission001.CurrentObjective();
	if (Current >= m001::ObjectiveCount) return FString();
	if (Current >= static_cast<int32>(m001::EObjective::IdentifyMissingInformation))
	{
		return TEXT("Complete the R-014 assessment form at your workstation");
	}
	return ToFString(m001::ObjectiveText(static_cast<m001::EObjective>(Current)));
}

TArray<FRbObjectiveView> URbGameSubsystem::GetObjectives() const
{
	TArray<FRbObjectiveView> Out;
	for (int32 i = 0; i < m001::ObjectiveCount; ++i)
	{
		const m001::EObjective O = static_cast<m001::EObjective>(i);
		bool bDone = State.Mission001.IsComplete(O);
		if (O == m001::EObjective::ReceiveDebrief) bDone = IsDebriefed();
		Out.Add({ToFString(m001::ObjectiveText(O)), bDone});
	}
	return Out;
}

TArray<FRbEvidenceView> URbGameSubsystem::GetEvidence() const
{
	TArray<FRbEvidenceView> Out;
	for (const m001::EvidenceItem& E : m001::Evidence())
	{
		Out.Add(GetEvidenceItem(ToFString(E.Id)));
	}
	return Out;
}

FRbEvidenceView URbGameSubsystem::GetEvidenceItem(const FString& EvidenceId) const
{
	FRbEvidenceView V;
	const std::string Id = ToStd(EvidenceId);
	for (const m001::EvidenceItem& E : m001::Evidence())
	{
		if (E.Id != Id) continue;
		V.Id = EvidenceId;
		V.Title = ToFString(E.Title);
		V.Location = ToFString(E.Location);
		V.Body = ToFString(E.Body);
		V.bControlEvidence = E.bControlEvidence;
		const std::vector<std::string>& Viewed = State.Mission001.ViewedEvidence;
		V.bViewed = std::find(Viewed.begin(), Viewed.end(), Id) != Viewed.end();
	}
	return V;
}

TArray<FRbOptionView> URbGameSubsystem::GetMissingInfoOptions() const { return ToViews(m001::MissingInfoCandidates()); }
TArray<FRbOptionView> URbGameSubsystem::GetTreatmentOptions() const { return ToViews(m001::TreatmentOptions()); }
TArray<FRbOptionView> URbGameSubsystem::GetActionOwnerOptions() const { return ToViews(m001::ActionOwnerOptions()); }
TArray<FRbOptionView> URbGameSubsystem::GetRationaleTags() const { return ToViews(m001::RationaleTags()); }

TArray<FString> URbGameSubsystem::GetControlNames() const
{
	TArray<FString> Out;
	for (const rb::Control& C : m001::ExistingControls())
	{
		Out.Add(ToFString(C.Name));
	}
	return Out;
}

int32 URbGameSubsystem::GetMatrixSize() const
{
	return State.Register.Matrix().Size;
}

FRbDebriefView URbGameSubsystem::GetDebrief() const
{
	FRbDebriefView V;
	const m001::MissionState& M = State.Mission001;
	if (!M.bSubmitted) return V; // the rubric stays hidden until Submit()
	const m001::Debrief& D = M.Result;
	V.bAvailable = true;
	V.bPassed = D.bPassed;
	V.OverallScore = D.OverallScore;
	for (const m001::CriterionResult& C : D.Criteria)
	{
		V.Criteria.Add({ToFString(C.Name), C.Score, C.Weight, ToFString(C.Feedback)});
	}
	V.Assumptions = ToFStrings(D.Assumptions);
	V.TradeOffs = ToFStrings(D.TradeOffs);
	V.Improvements = ToFStrings(D.Improvements);
	V.Chosen = ToView(D.ChosenAppraisal, OptionLabel(m001::TreatmentOptions(), M.Submitted.TreatmentId));
	V.Recommended = ToView(D.RecommendedAppraisal, TEXT("Recommended option"));
	V.BaselineExpectedLoss = D.BaselineExpectedLoss;
	V.BaselineWorstCase = D.BaselineWorstCase;
	V.XpAwarded = D.XpAwarded;
	return V;
}

FRbCareerView URbGameSubsystem::GetCareer() const
{
	FRbCareerView V;
	const rb::CareerState& C = State.Career;
	V.Level = C.Level;
	V.Title = ToFString(rb::LevelDef(C.Level).Title);
	V.Xp = C.Xp;
	V.PassedMissions = ToFStrings(C.PassedMissions);
	for (int32 i = 0; i < rb::CompetencyCount; ++i)
	{
		const rb::ECompetency Comp = static_cast<rb::ECompetency>(i);
		const rb::CompetencyRecord& R = C.Get(Comp);
		V.Competencies.Add({ToFString(rb::ToString(Comp)), R.Score, R.Assessments});
	}
	V.UnmetPromotionGates = ToFStrings(C.CheckPromotion().UnmetGates);
	return V;
}

FRbRegisterEntryView URbGameSubsystem::GetRegisterEntry(const FString& RiskId) const
{
	FRbRegisterEntryView V;
	const rb::RiskEntry* E = State.Register.Find(ToStd(RiskId));
	if (!E) return V;
	const rb::RiskMatrix& Matrix = State.Register.Matrix();
	V.bFound = true;
	V.Id = ToFString(E->Id);
	V.Title = ToFString(E->Title);
	V.Cause = ToFString(E->Cause);
	V.Event = ToFString(E->Event);
	V.Consequence = ToFString(E->Consequence);
	V.Category = ToFString(E->Category);
	V.Owner = ToFString(E->Owner);
	V.InherentLikelihood = E->InherentLikelihood;
	V.InherentImpact = E->InherentImpact;
	V.InherentRating = ToFString(rb::ToString(Matrix.Rate(E->InherentLikelihood, E->InherentImpact)));
	V.ResidualLikelihood = E->ResidualLikelihood;
	V.ResidualImpact = E->ResidualImpact;
	V.ResidualRating = ToFString(rb::ToString(Matrix.Rate(E->ResidualLikelihood, E->ResidualImpact)));
	V.Appetite = ToFString(rb::ToString(E->Appetite));
	V.Treatment = ToFString(rb::ToString(E->Treatment));
	V.Status = ToFString(rb::ToString(E->Status));
	V.ActionOwner = ToFString(E->ActionOwner);
	for (const rb::Control& C : E->Controls)
	{
		V.Controls.Add(ToFString(C.Name));
	}
	return V;
}

FString URbGameSubsystem::GetCheckpoint() const { return ToFString(State.Checkpoint); }
bool URbGameSubsystem::HasMetManager() const { return State.Mission001.IsComplete(m001::EObjective::MeetManager); }
bool URbGameSubsystem::IsSubmitted() const { return State.Mission001.bSubmitted; }
bool URbGameSubsystem::IsDebriefed() const { return State.Checkpoint == ToStd(CheckpointDebriefed); }

ERbDifficulty URbGameSubsystem::GetDifficulty() const
{
	switch (State.Difficulty)
	{
	case rb::EDifficulty::Analyst: return ERbDifficulty::Analyst;
	case rb::EDifficulty::Executive: return ERbDifficulty::Executive;
	default: return ERbDifficulty::Professional;
	}
}

TArray<FString> URbGameSubsystem::GetAchievementTitles() const
{
	TArray<FString> Out;
	for (const std::string& Id : State.Achievements)
	{
		for (const rb::AchievementDef& Def : rb::AchievementDefs())
		{
			if (Def.Id == Id) Out.Add(ToFString(Def.Title));
		}
	}
	return Out;
}

FRbSettingsView URbGameSubsystem::GetSettings() const
{
	const rb::PlayerSettings& S = State.Settings;
	FRbSettingsView V;
	V.MouseSensitivity = static_cast<float>(S.MouseSensitivity);
	V.TextScale = static_cast<float>(S.TextScale);
	V.MasterVolume = static_cast<float>(S.MasterVolume);
	V.bReducedMotion = S.bReducedMotion;
	V.bHints = S.bHints;
	V.bFullscreen = S.bFullscreen;
	return V;
}

void URbGameSubsystem::SetSettings(const FRbSettingsView& In, bool bApplyWindowMode)
{
	rb::PlayerSettings& S = State.Settings;
	S.MouseSensitivity = FMath::Clamp(In.MouseSensitivity, 0.1f, 5.0f);
	S.TextScale = FMath::Clamp(In.TextScale, 0.8f, 1.6f);
	S.MasterVolume = FMath::Clamp(In.MasterVolume, 0.0f, 1.0f);
	S.bReducedMotion = In.bReducedMotion;
	S.bHints = In.bHints;
	S.bFullscreen = In.bFullscreen;
	ApplySettings(bApplyWindowMode);
	OnStateChanged.Broadcast();
}

void URbGameSubsystem::ApplySettings(bool bApplyWindowMode) const
{
	const rb::PlayerSettings& S = State.Settings;
	RbUI::SetTextScale(static_cast<float>(S.TextScale));
	if (IConsoleVariable* MotionBlur = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))
	{
		MotionBlur->Set(S.bReducedMotion ? 0 : 4, ECVF_SetByGameSetting);
	}
	if (GEngine)
	{
		if (FAudioDeviceHandle Audio = GEngine->GetMainAudioDevice())
		{
			Audio->SetTransientPrimaryVolume(static_cast<float>(S.MasterVolume));
		}
		// Window mode is only touched on an explicit settings change, never in the editor.
		if (bApplyWindowMode && !GIsEditor)
		{
			if (UGameUserSettings* User = GEngine->GetGameUserSettings())
			{
				User->SetFullscreenMode(S.bFullscreen ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
				User->ApplyResolutionSettings(false);
				User->SaveSettings();
			}
		}
	}
}

FString URbGameSubsystem::SubmitResultText(ERbSubmitResult Result)
{
	switch (Result)
	{
	case ERbSubmitResult::Ok: return TEXT("Assessment submitted.");
	case ERbSubmitResult::NoGame: return TEXT("No game in progress.");
	case ERbSubmitResult::AlreadySubmitted: return TEXT("This assessment has already been submitted.");
	case ERbSubmitResult::ManagerNotMet: return TEXT("Meet Priya Okafor before submitting.");
	case ERbSubmitResult::IncidentReportNotReviewed: return TEXT("Review the incident report in your inbox first.");
	case ERbSubmitResult::RegisterNotInspected: return TEXT("Inspect register entry R-014 first.");
	case ERbSubmitResult::IncompleteSubmission: return TEXT("Some sections are incomplete: every section needs an answer.");
	}
	return FString();
}
