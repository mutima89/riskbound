# Career model

Code: `RbCareer.*`. Ladder data in `BuildLadder()`; this file explains the rules.

## Rules
- Ten levels; Mission 001 starts at level 2 (Junior Risk Analyst). New games may later start at level 1 for the tutorial (M000).
- Ten competencies, each scored 0..100 with an evidence count.
- Each assessed piece of work updates a competency: first result sets the score, later ones blend `new = old x 0.6 + result x 0.4`.
- **Promotion gate** (to level+1) = every listed competency >= its minimum AND at least `MinAssessmentsPerRequirement` assessments for each AND every required mission passed. XP is never checked. Test: `Career_XpAloneNeverPromotes`.
- Decision authority (currency) caps what the player can approve; options above it must be escalated.
- Specialisms (enterprise, financial, operational, strategic, cyber, compliance, business continuity, project, environmental) are tracked now; specialism-specific missions arrive in Gate 3/4.
- Post-CRO pathways: Group CRO, CEO, COO, Specialist Executive, Board Risk Adviser.

## Campaign progression (Gate 3)
Code: `RbCampaign.*` (sim), `URbCampaignSubsystem` (UE). Tests: `Campaign_*` in `Tests/Core/TestCampaign.cpp`, `Riskbound.Campaign.*`.
- Missions after M001 are `MissionDef` content: evidence, single- or multi-choice questions (each tied to one competency, weights sum to 100), pass mark, organisation time they take, a lesson. Content is checked by `ValidateMission` / `ValidateCatalogue`.
- **Availability is decided in the sim** (`CheckAvailability`): not already passed, career level >= `MinLevel`, every `RequiresPassed` mission passed (M001 counts), every `RequiresFlags` flag set, no `ExcludedByFlags` flag set. Only one mission is active at a time. A failed mission can be retried; every attempt is recorded.
- **Scoring** (`ScoreMission`, pure): single choice = the option's score; multi-select = `round(100 x hits / correct) - 25 x wrong picks`, clamped 0..100. A single choice that commits more than the player's decision authority without escalating is capped at 40 and says why. Overall = weighted mean; pass = overall >= pass mark. Each mission gives one assessment per competency it tested (the weighted mean of that competency's questions), so evidence counts measure distinct pieces of work.
- **Submission** (`SubmitMission`): applies the assessments, records the mission (XP is feedback only), records one organisational decision (quality = score) with every chosen option's effects, sets the options' flags and `<id>_passed` / `<id>_failed`, schedules the options' consequences (probability x difficulty multiplier), then advances organisation time by the mission's weeks. Anything due resolves then, from this or any earlier mission.
- **Consequences carry between missions** three ways: (1) organisation metrics persist; (2) a consequence that occurs can set a flag (`FlagIfOccurs`) that opens a crisis or another mission; (3) `ContextNote`s add briefing paragraphs when a flag is (or is not) set or a metric is below/above a threshold, so later missions refer to earlier choices. A mitigating flag from a good earlier decision lowers probability and severity.
- **Promotion review** runs after every submission: `TryPromote` with the rules above (competency minimums, evidence counts, required missions). The debrief lists the remaining gaps. Tests: `Campaign_PromotionNeedsCompetenceAndMissionsAcrossMissions` (promotes only after M003) and `Campaign_PassingMissionsWithWeakCompetenceDoesNotPromote` (missions passed, communication 42 < 45: no promotion).

## Levels
| # | Title | Authority | Promotion needs (competency >= min) | Missions |
|---|---|---|---|---|
| 1 | Intern / Graduate Trainee | 0 | RiskIdent 40, Comms 35 | M000 |
| 2 | Junior Risk Analyst | 5k | RiskIdent 55, Assessment 55, Controls 50, Comms 45 | M001-M003 |
| 3 | Risk Analyst | 25k | Assessment 62, Quant 55, Controls 60, Judgement 50 | M004-M006 |
| 4 | Senior Risk Analyst | 75k | Quant 65, Judgement 60, Stakeholder 55, Comms 60 | M007-M009 |
| 5 | Supervisor / Team Lead | 150k | Leadership 55, Stakeholder 62, Judgement 65, Governance 50 | M010-M012 |
| 6 | Risk Manager | 300k | Governance 62, Strategy 55, Leadership 62, Quant 68 | M013-M015 |
| 7 | Senior Risk Manager | 750k | Strategy 65, Stakeholder 70, Judgement 72, Leadership 68 | M016-M018, C001 |
| 8 | Head of Risk | 1.5m | Strategy 72, Governance 72, Comms 72, Leadership 72 | M019-M022, C002 |
| 9 | Director of Risk | 5m | Strategy 78, Governance 78, Stakeholder 78, Judgement 78 | M023-M026, C003 |
| 10 | Chief Risk Officer | 25m | (top) CRO capstone | - |

Responsibilities, stakeholder expectations and new tools per level are in `BuildLadder()`.
Mission ids M002+ and crisis ids C001+ are placeholders for Gate 4 content; promotion beyond level 2 is impossible until they exist, which is intended.
