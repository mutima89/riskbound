#include "SRbCaseFiles.h"

#include "RbCampaignSubsystem.h"
#include "RbCareer.h"
#include "RbGameSubsystem.h"
#include "RbUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Riskbound"

using namespace RbUI;

namespace
{
FOnClicked CaseClick(TFunction<void()> Fn)
{
	return FOnClicked::CreateLambda([Fn]() { Fn(); return FReply::Handled(); });
}

TSharedRef<SWidget> CaseCard(TSharedRef<SWidget> Content)
{
	return SNew(SBorder).BorderImage(CardBrush()).Padding(Space4)[Content];
}

TSharedRef<SWidget> CaseText(const FString& Text, int32 Size = 14, const FLinearColor& Color = TextPrimary, EWeight Weight = EWeight::Regular)
{
	return SNew(STextBlock).Text(FText::FromString(Text)).Font(FontW(Size, Weight)).ColorAndOpacity(Color).AutoWrapText(true);
}

FString PromotionLine(const TArray<FString>& Gaps)
{
	if (Gaps.IsEmpty()) return TEXT("You meet every promotion requirement for your level.");
	if (Gaps.Num() == 1 && Gaps[0].StartsWith(TEXT("Already at the top"))) return TEXT("You are at the top of the career ladder.");
	return FString(TEXT("Next promotion still needs: ")) + FString::Join(Gaps, TEXT("; "));
}

FLinearColor CaseScoreColor(int32 Score)
{
	return Score >= 75 ? Good : (Score >= 50 ? Warning : Bad);
}
} // namespace

void SRbCaseFiles::Construct(const FArguments& InArgs)
{
	Campaign = InArgs._Campaign;
	Game = InArgs._Game;
	OnClose = InArgs._OnClose;
	OnMissionStarted = InArgs._OnMissionStarted;
	Where = InArgs._Where;
	TSharedPtr<SButton> Close;
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[MakeEyebrow(LOCTEXT("CaseEyebrow", "Workstation  /  Case files"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, Space1, 0.0f, Space3)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)[MakeHeading(LOCTEXT("CaseTitle", "Case files"))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(FontW(13, EWeight::Medium)).ColorAndOpacity(TextMuted)
				.Text_Lambda([this]()
				{
					if (!Game.IsValid()) return FText::GetEmpty();
					const FRbCareerView C = Game->GetCareer();
					return FText::Format(LOCTEXT("CaseCareer", "{0} (level {1})  |  Approval authority {2}"), FText::FromString(C.Title), C.Level,
						Money(rb::LevelDef(C.Level).DecisionAuthority));
				})
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(Body, SBox)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, Space3, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text_Lambda([this]() { return Status; }).Font(FontW(13, EWeight::Medium)).ColorAndOpacity(Warning).AutoWrapText(true)
			]
			+ SHorizontalBox::Slot().AutoWidth()[MakeButton(LOCTEXT("CaseClose", "Back to workstation"), CaseClick([this]() { OnClose(); }), &Close)]
		]
	];
	FirstFocus = Close;
	ShowList();
}

void SRbCaseFiles::ShowList()
{
	View = EView::List;
	Status = FText::GetEmpty();
	OpenEvidenceId.Reset();
	Rebuild();
}

void SRbCaseFiles::ShowMission(const FString& InMissionId)
{
	if (!Campaign.IsValid()) return;
	const ERbMissionResult R = Campaign->StartMission(InMissionId);
	if (R != ERbMissionResult::Ok)
	{
		Status = FText::FromString(URbCampaignSubsystem::ResultText(R));
		return;
	}
	if (MissionId != InMissionId)
	{
		const TMap<FString, TArray<FString>>* Draft = Campaign->GetDraft(InMissionId);
		Answers = Draft ? *Draft : TMap<FString, TArray<FString>>();
	}
	if (OnMissionStarted) OnMissionStarted();
	MissionId = InMissionId;
	View = EView::Mission;
	Status = FText::GetEmpty();
	OpenEvidenceId.Reset();
	Rebuild();
}

void SRbCaseFiles::ShowDebrief(const FString& InMissionId)
{
	MissionId = InMissionId;
	View = EView::Debrief;
	Status = FText::GetEmpty();
	Rebuild();
}

void SRbCaseFiles::Rebuild()
{
	if (!Body.IsValid()) return;
	switch (View)
	{
	case EView::List: Body->SetContent(BuildList()); break;
	case EView::Mission: Body->SetContent(BuildMission()); break;
	case EView::Debrief: Body->SetContent(BuildDebrief()); break;
	}
}

TArray<FRbMissionAnswer> SRbCaseFiles::CurrentAnswers() const
{
	TArray<FRbMissionAnswer> Out;
	for (const TPair<FString, TArray<FString>>& A : Answers)
	{
		if (A.Value.Contains(FString())) continue; // a rating with only one of its two parts chosen
		FRbMissionAnswer Answer;
		Answer.QuestionId = A.Key;
		Answer.OptionIds = A.Value;
		Out.Add(Answer);
	}
	return Out;
}

void SRbCaseFiles::SetAnswer(const FString& QuestionId, const FString& OptionId, bool bMulti, bool bSelected)
{
	TArray<FString>& Picked = Answers.FindOrAdd(QuestionId);
	if (!bMulti) Picked.Reset();
	if (bSelected) Picked.AddUnique(OptionId);
	else Picked.Remove(OptionId);
	if (Picked.IsEmpty()) Answers.Remove(QuestionId);
	SaveDraft();
}

void SRbCaseFiles::SaveDraft()
{
	if (Campaign.IsValid() && !MissionId.IsEmpty()) Campaign->SetDraft(MissionId, Answers);
}

TSharedRef<SWidget> SRbCaseFiles::BuildList()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	if (!Campaign.IsValid()) return Box;
	const TArray<FRbMissionSummary> Missions = Campaign->GetMissions();
	if (Game.IsValid())
	{
		const TArray<FString> Gaps = Game->GetCareer().UnmetPromotionGates;
		const FString GapText = PromotionLine(Gaps);
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space3)[CaseText(GapText, 13, TextMuted)];
		if (Campaign->IsStuck())
		{
			Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space3)
			[
				CaseCard(CaseText(TEXT("You have passed every case open at your level, but your evidence is not yet strong enough for promotion. ")
					TEXT("Review a case to strengthen it: reviews count as new evidence of your competence and do not change the organisation."), 14, Warning))
			];
		}
	}
	if (Missions.IsEmpty())
	{
		Box->AddSlot().AutoHeight()[CaseText(TEXT("No case files are installed."), 14, TextMuted)];
		return Box;
	}
	for (const FRbMissionSummary& M : Missions)
	{
		FString State;
		FLinearColor StateColor = TextMuted;
		if (M.bActive) { State = TEXT("In progress"); StateColor = Accent; }
		else if (M.bPassed) { State = FString::Printf(TEXT("Passed (best %d)"), M.BestScore); StateColor = Good; }
		else if (M.bAvailable && M.bReview) { State = FString::Printf(TEXT("Review available (best %d): practice to build evidence"), M.BestScore); StateColor = Warning; }
		else if (M.bAvailable) { State = M.Attempts ? FString::Printf(TEXT("Retry available (best %d)"), M.BestScore) : FString(TEXT("Available")); StateColor = Accent; }
		if (M.WeeksToEscalate >= 0 && !M.bPassed)
		{
			State += M.WeeksToEscalate == 0 ? FString(TEXT("  |  Escalates this week")) : FString::Printf(TEXT("  |  Escalates in %d week%s"), M.WeeksToEscalate, M.WeeksToEscalate == 1 ? TEXT("") : TEXT("s"));
			StateColor = Bad;
		}
		else { State = FString::Join(M.Reasons, TEXT("; ")); }

		TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
		const FString Id = M.Id;
		if (M.bAvailable || M.bActive)
		{
			Buttons->AddSlot().AutoWidth().Padding(Space2, 0.0f, 0.0f, 0.0f)
			[
				MakeButton(M.bActive ? LOCTEXT("CaseResume", "Resume") : (M.bReview ? LOCTEXT("CaseReview", "Review case") : LOCTEXT("CaseOpen", "Open case")),
					CaseClick([this, Id]() { ShowMission(Id); }), nullptr, true, true)
			];
		}
		if (M.Attempts > 0)
		{
			Buttons->AddSlot().AutoWidth().Padding(Space2, 0.0f, 0.0f, 0.0f)
			[
				MakeButton(LOCTEXT("CaseDebrief", "Debrief"), CaseClick([this, Id]() { ShowDebrief(Id); }))
			];
		}
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)
		[
			CaseCard(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[CaseText(FString::Printf(TEXT("%s  |  %s  |  %s"), *M.Id, *M.Kind, *M.Department), 12, TextMuted, EWeight::Medium)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)[CaseText(M.Title, 17, TextPrimary, EWeight::SemiBold)]
					+ SVerticalBox::Slot().AutoHeight()[CaseText(State, 13, StateColor)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Buttons])
		];
	}
	return Box;
}

TSharedRef<SWidget> SRbCaseFiles::BuildMission()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	if (!Campaign.IsValid()) return Box;
	const FRbMissionView M = Campaign->GetMission(MissionId);
	if (!M.bFound) return Box;
	Box->AddSlot().AutoHeight()[CaseText(FString::Printf(TEXT("%s  |  %s  |  %s"), *M.Id, *M.Kind, *M.Department), 12, TextMuted, EWeight::Medium)];
	Box->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, Space3)[CaseText(M.Title, 20, TextPrimary, EWeight::SemiBold)];

	// Briefing, including notes that come from earlier decisions.
	TSharedRef<SVerticalBox> Brief = SNew(SVerticalBox);
	for (int32 i = 0; i < M.Briefing.Num(); ++i)
	{
		Brief->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)[CaseText(M.Briefing[i], 14, i == 0 ? TextPrimary : Accent)];
	}
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space3)[CaseCard(Brief)];

	// Evidence.
	TSharedRef<SVerticalBox> Ev = SNew(SVerticalBox);
	Ev->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)[CaseText(TEXT("Evidence"), 15, TextPrimary, EWeight::SemiBold)];
	for (const FRbMissionEvidenceView& E : M.Evidence)
	{
		if (!E.At.IsEmpty() && E.At != Where)
		{
			// Seen in person: the button becomes a pointer to where it is.
			const FString Where2 = E.PersonName.IsEmpty() ? FString::Printf(TEXT("examine it in the %s"), *E.AtName.ToLower())
				: FString::Printf(TEXT("talk to %s in the %s"), *E.PersonName, *E.AtName.ToLower());
			Ev->AddSlot().AutoHeight().Padding(0.0f, Space1, 0.0f, Space2)
			[
				CaseText(FString::Printf(TEXT("%s%s  -  in person: %s%s"), E.bViewed ? TEXT("[seen]  ") : TEXT(""), *E.Title, *Where2, E.bRequired ? TEXT("") : TEXT(" (optional)")),
					14, E.bViewed ? TextMuted : Warning, EWeight::Medium)
			];
			if (E.bViewed && OpenEvidenceId == E.Id) Ev->AddSlot().AutoHeight().Padding(Space4, Space1, 0.0f, Space2)[CaseText(E.Body, 14, TextPrimary)];
			continue;
		}
		const FString EvId = E.Id;
		const FString Label = FString::Printf(TEXT("%s%s%s"), E.bViewed ? TEXT("[read]  ") : TEXT(""), *E.Title, E.bRequired ? TEXT("") : TEXT("  (optional)"));
		Ev->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space1)
		[
			MakeButton(FText::FromString(Label), CaseClick([this, EvId]()
			{
				if (!Campaign.IsValid()) return;
				Campaign->ViewEvidence(MissionId, EvId, Where);
				OpenEvidenceId = OpenEvidenceId == EvId ? FString() : EvId;
				Rebuild();
			}))
		];
		if (OpenEvidenceId == E.Id)
		{
			Ev->AddSlot().AutoHeight().Padding(Space4, Space1, 0.0f, Space2)[CaseText(E.Body, 14, TextPrimary)];
		}
	}
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space3)[CaseCard(Ev)];

	// Questions: no scores are shown before submission.
	for (int32 qi = 0; qi < M.Questions.Num(); ++qi)
	{
		const FRbMissionQuestionView& Q = M.Questions[qi];
		TSharedRef<SVerticalBox> QBox = SNew(SVerticalBox);
		QBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)
		[
			CaseText(FString::Printf(TEXT("%d. %s%s"), qi + 1, *Q.Prompt, Q.bMulti ? TEXT("  (select all that apply)") : TEXT("")), 15, TextPrimary, EWeight::SemiBold)
		];
		const FString QId = Q.Id;
		if (Q.Kind == TEXT("numeric"))
		{
			// A figure the player works out, with the workstation tools if they like; scored by how close it is.
			const TArray<FString>* Prior = Answers.Find(QId);
			QBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(220.0f)
					[
						SNew(SEditableTextBox)
						.Text(FText::FromString(Prior && Prior->Num() == 1 ? (*Prior)[0] : FString()))
						.HintText(LOCTEXT("NumericHint", "Enter a figure"))
						.Font(FontW(15, EWeight::Medium))
						.OnTextChanged_Lambda([this, QId](const FText& T)
						{
							const FString V = T.ToString().TrimStartAndEnd();
							if (V.IsEmpty()) Answers.Remove(QId);
							else Answers.Add(QId, {V});
							SaveDraft();
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(Space2, 0.0f, 0.0f, 0.0f)[CaseText(Q.Unit, 14, TextMuted)]
			];
			QBox->AddSlot().AutoHeight().Padding(0.0f, Space1, 0.0f, 0.0f)
			[
				CaseText(TEXT("Work it out from the evidence; the Risk tools at your workstation can help. Scored by how close you are."), 12, TextMuted)
			];
		}
		else if (Q.Kind == TEXT("rating"))
		{
			// Two rows of 1-5 on the Valtren matrix scale.
			static const TCHAR* const Words[2][5] = {{TEXT("Rare"), TEXT("Unlikely"), TEXT("Possible"), TEXT("Likely"), TEXT("Almost certain")},
				{TEXT("Minimal"), TEXT("Minor"), TEXT("Moderate"), TEXT("Major"), TEXT("Severe")}};
			for (int32 Part = 0; Part < 2; ++Part)
			{
				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
				Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, Space3, 0.0f)
				[
					SNew(SBox).WidthOverride(90.0f)[CaseText(Part == 0 ? TEXT("Likelihood") : TEXT("Impact"), 14, TextMuted, EWeight::Medium)]
				];
				for (int32 V = 1; V <= 5; ++V)
				{
					const FString Value = FString::FromInt(V);
					Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Space3, 0.0f)
					[
						SNew(SCheckBox)
						.Style(&RadioStyle())
						.IsChecked_Lambda([this, QId, Part, Value]()
						{
							const TArray<FString>* Picked = Answers.Find(QId);
							return Picked && Picked->Num() == 2 && (*Picked)[Part] == Value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([this, QId, Part, Value](ECheckBoxState)
						{
							TArray<FString>& Picked = Answers.FindOrAdd(QId);
							Picked.SetNum(2);
							Picked[Part] = Value;
							SaveDraft();
						})
						[
							SNew(SBox).Padding(Space1, 0.0f, 0.0f, 0.0f)[CaseText(FString::Printf(TEXT("%d %s"), V, Words[Part][V - 1]), 13)]
						]
					];
				}
				QBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)[Row];
			}
		}
		for (const FRbMissionOptionView& O : Q.Options)
		{
			const FString OId = O.Id;
			const bool bMulti = Q.bMulti;
			QBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SNew(SCheckBox)
				.Style(bMulti ? &CheckStyle() : &RadioStyle())
				.IsChecked_Lambda([this, QId, OId]()
				{
					const TArray<FString>* Picked = Answers.Find(QId);
					return Picked && Picked->Contains(OId) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this, QId, OId, bMulti](ECheckBoxState S) { SetAnswer(QId, OId, bMulti, S == ECheckBoxState::Checked); })
				[
					SNew(SBox).Padding(Space2, 0.0f, 0.0f, 0.0f)[CaseText(O.Label, 14)]
				]
			];
		}
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)[CaseCard(QBox)];
	}

	Box->AddSlot().AutoHeight().Padding(0.0f, Space2, 0.0f, 0.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Font(FontW(13, EWeight::Medium)).ColorAndOpacity(TextMuted).AutoWrapText(true)
			.Text_Lambda([this]()
			{
				if (!Campaign.IsValid()) return FText::GetEmpty();
				const ERbMissionResult R = Campaign->ValidateAnswers(MissionId, CurrentAnswers());
				const FRbMissionView MV = Campaign->GetMission(MissionId);
				if (R == ERbMissionResult::Ok && !MV.PresentAt.IsEmpty() && MV.PresentAt != Where)
				{
					return FText::Format(LOCTEXT("CasePresent", "Ready. This one is presented in person: take it to the lectern in the {0}."), FText::FromString(MV.PresentAtName.ToLower()));
				}
				return R == ERbMissionResult::Ok ? LOCTEXT("CaseReady", "Ready to submit. Your answers are scored only after you submit.")
					: FText::FromString(URbCampaignSubsystem::ResultText(R));
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(Space2, 0.0f, 0.0f, 0.0f)
		[
			MakeButton(LOCTEXT("CaseSetAside", "Set aside"), CaseClick([this]()
			{
				if (Campaign.IsValid()) Campaign->AbandonMission();
				ShowList();
			}))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(Space2, 0.0f, 0.0f, 0.0f)
		[
			MakeButton(LOCTEXT("CaseSubmit", "Submit"), CaseClick([this]()
			{
				if (!Campaign.IsValid()) return;
				const ERbMissionResult R = Campaign->SubmitMission(MissionId, CurrentAnswers(), Where);
				if (R == ERbMissionResult::Ok)
				{
					Answers.Reset();
					ShowDebrief(MissionId);
				}
				else
				{
					Status = FText::FromString(URbCampaignSubsystem::ResultText(R));
				}
			}), nullptr,
			TAttribute<bool>::CreateLambda([this]()
			{
				if (!Campaign.IsValid() || Campaign->ValidateAnswers(MissionId, CurrentAnswers()) != ERbMissionResult::Ok) return false;
				const FRbMissionView MV = Campaign->GetMission(MissionId);
				return MV.PresentAt.IsEmpty() || MV.PresentAt == Where;
			}),
			true)
		]
	];
	return Box;
}

TSharedRef<SWidget> SRbCaseFiles::BuildDebrief()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	if (!Campaign.IsValid()) return Box;
	const FRbMissionDebriefView D = Campaign->GetDebrief(MissionId);
	if (!D.bAvailable)
	{
		Box->AddSlot().AutoHeight()[CaseText(TEXT("No debrief yet."), 14, TextMuted)];
		return Box;
	}
	Box->AddSlot().AutoHeight()[CaseText(FString::Printf(TEXT("%s  |  Debrief"), *D.MissionId), 12, TextMuted, EWeight::Medium)];
	Box->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, Space3)[CaseText(D.Title, 20, TextPrimary, EWeight::SemiBold)];
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space3)
	[
		CaseCard(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				CaseText(FString::Printf(TEXT("Decision quality %d / 100  -  %s (pass mark %d)"), D.Score, D.bPassed ? TEXT("passed") : TEXT("not passed"), D.PassMark),
					18, CaseScoreColor(D.Score), EWeight::SemiBold)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, Space1, 0.0f, 0.0f)
			[
				CaseText(TEXT("Decision quality is judged on your reasoning, separately from whatever luck brings later."), 13, TextMuted)
			])
	];
	if (D.bReview)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space3)
		[
			CaseCard(CaseText(TEXT("Review: this attempt counts as new evidence of your competence. The organisation already lived with your original decision, so nothing else changes."), 14, TextMuted))
		];
	}
	if (D.bPromoted)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space3)
		[
			CaseCard(CaseText(FString::Printf(TEXT("Promoted: you are now %s (level %d). New tools and a higher approval authority come with the role."), *D.LevelTitle, D.Level),
				16, Good, EWeight::SemiBold))
		];
	}
	for (const FRbMissionCriterionView& C : D.Criteria)
	{
		TSharedRef<SVerticalBox> CBox = SNew(SVerticalBox);
		CBox->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)[CaseText(C.Prompt, 14, TextPrimary, EWeight::SemiBold)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(Space3, 0.0f, 0.0f, 0.0f)
			[
				CaseText(FString::Printf(TEXT("%d  (weight %d%%)"), C.Score, C.Weight), 14, CaseScoreColor(C.Score), EWeight::SemiBold)
			]
		];
		CBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[CaseText(C.Competency, 12, TextMuted, EWeight::Medium)];
		for (const FString& F : C.Feedback)
		{
			CBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[CaseText(F, 13, C.bAuthorityExceeded && F.Contains(TEXT("authority")) ? Warning : TextPrimary)];
		}
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)[CaseCard(CBox)];
	}
	if (!D.Consequences.IsEmpty())
	{
		TSharedRef<SVerticalBox> CBox = SNew(SVerticalBox);
		CBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space1)[CaseText(TEXT("While you worked"), 15, TextPrimary, EWeight::SemiBold)];
		for (const FString& C : D.Consequences) CBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[CaseText(C, 13)];
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)[CaseCard(CBox)];
	}
	if (!D.Fallout.IsEmpty() || !D.Escalated.IsEmpty() || D.bBurnout || D.NewMessages > 0)
	{
		TSharedRef<SVerticalBox> WBox = SNew(SVerticalBox);
		WBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space1)[CaseText(TEXT("What happens next"), 15, TextPrimary, EWeight::SemiBold)];
		for (const FString& F : D.Fallout)
		{
			WBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[CaseText(TEXT("Loose end: a weak call here may come back as an incident in the coming weeks."), 13, Warning)];
			break; // one line: the incident itself will name the decision when it happens
		}
		for (const FString& E : D.Escalated) WBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[CaseText(FString(TEXT("Escalated without you: ")) + E, 13, Bad)];
		if (D.bBurnout) WBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[CaseText(TEXT("You burned out and took two weeks off. The organisation carried on without you."), 13, Bad)];
		if (D.NewMessages > 0)
		{
			WBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[CaseText(FString::Printf(TEXT("%d new message%s in your inbox."), D.NewMessages, D.NewMessages == 1 ? TEXT("") : TEXT("s")), 13, Accent)];
		}
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)[CaseCard(WBox)];
	}
	TSharedRef<SVerticalBox> Tail = SNew(SVerticalBox);
	if (!D.Lesson.IsEmpty()) Tail->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space2)[CaseText(FString(TEXT("Lesson: ")) + D.Lesson, 14, Accent)];
	Tail->AddSlot().AutoHeight()
	[
		CaseText(PromotionLine(D.PromotionGaps), 13, TextMuted)
	];
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Space3)[CaseCard(Tail)];
	Box->AddSlot().AutoHeight()[MakeButton(LOCTEXT("CaseBackToList", "Back to case files"), CaseClick([this]() { ShowList(); }), nullptr, true, true)];
	return Box;
}

#undef LOCTEXT_NAMESPACE
