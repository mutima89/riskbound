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
