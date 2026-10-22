# Mission data format (schema 1)

Missions after Mission 001 are content files: `Data/Missions/<id>.json`, one mission per file, file name = mission id.
Code: `RbJson.*` (reader), `RbMissionData.*` (file -> `MissionDef`), `RbCampaign.*` (validation and rules).
Rules for availability, scoring, consequences and promotion: Docs/CareerModel.md "Campaign progression".

## Loading and validation
- The game reads every `*.json` in `Data/Missions` at startup (`URbCampaignSubsystem::LoadFromDisk`); the files are staged with the packaged game (`Riskbound.Build.cs` RuntimeDependencies).
- Reading is strict: unknown keys, wrong types, unknown competency or metric names, duplicate keys and malformed JSON are errors with file and line. A typo never falls back silently to a default.
- Then `ValidateCatalogue` checks the content: weights sum to 100, every single-choice question has an option scoring 80+, every multi-select has at least one correct and one incorrect option, probabilities in 0..1, delays >= 1 week, index effects within +-50, unique ids, prerequisites exist.
- If any file has a problem, the game loads **no** campaign missions and logs every problem (`LogRiskbound: Error: Mission data: ...`). The core test `Data_ShippedMissionsParseAndValidate` and the automation test `Riskbound.Campaign.ShippedMissionDataLoads` fail on the same problems, so broken content cannot ship.
- `Data_BestPlayPromotesThroughTheShippedMissions` plays M001 and then the best answers of the shipped missions to prove the chain is reachable and promotion happens where intended.

## Top level
| Key | Type | Required | Meaning |
|---|---|---|---|
| `schema` | 1 | yes | format version |
| `id` | string | yes | `M002`.. missions, `C001`.. crises, `CRO` capstone; letters, digits, `_`, `-` |
| `title` | string | yes | |
| `kind` | `mission` / `crisis` / `capstone` | no (mission) | |
| `department` | string | yes | shown in the case list |
| `briefing` | string or array of paragraphs | yes | |
| `minLevel` | 1..10 | yes | career level needed |
| `requiresPassed` | [mission ids] | no | `M001` counts |
| `requiresFlags` / `excludedByFlags` | [flags] | no | organisation flags |
| `passMark` | 1..100 | no (60) | |
| `xp` | int | no (100) | feedback only, never a promotion input |
| `weeks` | 0..52 | no (2) | organisation time that passes on submission |
| `lesson` | string or paragraphs | no | added to the learning history |
| `context` | [context notes] | no | briefing paragraphs that depend on earlier events |
| `evidence` | [evidence] | yes | |
| `questions` | [questions] | yes | |

Context note: `{"text": ..., "ifFlag": "x"}` or `{"text": ..., "ifNotFlag": "x"}` and/or `{"ifMetric": "StakeholderTrust", "below": 50}` (or `"atLeast"`).

Evidence: `{"id", "title", "body" (string or paragraphs), "required": true}`. Required evidence must be opened before submitting.

Question: `{"id", "prompt", "kind": "single"|"multi"|"numeric"|"rating", "competency", "weight", ...}`.
- `single` / `multi`: `"options": [...]`.
- `numeric` (a figure the player types, usually worked out with the workstation tools): `"answer"`, `"tolerance"` (relative, default 0.02), `"unit"`, `"explanation"` (debrief), `"mistakes": [{"value", "feedback"}]` (common wrong figures, scored 10 with their feedback). Scored 100 within tolerance, 70 within 3x, 35 within 10x, else 0.
- `rating` (place the risk on the 5x5 matrix): `"likelihood"`, `"impact"` (1-5), `"explanation"`. Each step away costs 30.
Evidence may carry `"at"` (HQ location: `reception`, `workspace`, `manager`, `meeting`, `training`, `monitoring`, `boardroom`) and `"person"` (an interview, Data/People.json): it can only be examined there (`EvidenceElsewhere` otherwise). A mission with `"presentAt"` is submitted in person at that location (the boardroom lectern) (`PresentInPerson`).
Competencies: `RiskIdentification, RiskAssessment, ControlEvaluation, QuantitativeAnalysis, Communication, StakeholderManagement, Judgement, Leadership, StrategicThinking, Governance`.

Option: `{"id", "label", "feedback", "score" (single), "correct" (multi), "cost", "escalates", "effects", "setsFlags", "schedules"}`.
- `cost` above the player's approval authority, without `"escalates": true`, caps that answer at 40.
- `effects`: `{"Metric": change}`. Metrics: `Revenue, Costs, Cash` (currency) and the 0..100 indices `RiskExposure, ControlEffectiveness, Reputation, StakeholderTrust, EmployeeMorale, Resilience, StrategicProgress`.
- `schedules`: consequences `{"id", "description", "delayWeeks", "probability", "effects", "flagIfOccurs", "mitigatedBy", "mitigatedProbabilityFactor", "mitigatedSeverityFactor"}`. Probability is multiplied by the difficulty factor (0.8 / 1.0 / 1.2).

## Authoring rules
- All names are fictional (Docs/GameDesign.md "Fictional names").
- Every evidence item has an `"at"` room, a case uses at least two rooms, and at least one item is an interview (`"person"`, a work colleague). Put a document where it would live (contracts in the meeting room, live data in the monitoring room, finance and board papers in the boardroom, policies in the manager's office, letters and notices at reception, safety and people material in the training area), and turn an email or note from a colleague into an interview with them. Each room has two standing spots and two document spots; more items in one room line up beside them. Checked by `World_EveryCaseIsWorkedOnFoot`.
- Every figure stated in an option must be what `RbQuant` computes; add a test like `Data_M004FiguresMatchTheQuantEngine`.
- Never state expected loss as the worst case; offer the confusion as a wrong answer with feedback instead.
- Feedback explains why, for wrong and right answers. The rubric is only shown after submission.
- A crisis required for promotion (ladder) must not depend on a random flag; flag-triggered crises are optional extras.

## Achievements file
`Data/Achievements.json`: `{"schema": 1, "achievements": [{"id", "title", "description", "when": {<one condition>}}]}`.
Conditions: `code` (built-in Mission 001 rule), `passMission`, `reachLevel`, `missionsPassed`, `crisesPassed`, `missionScoreAtLeast` + `mission`, `anyScoreAtLeast`, `scoreAtLeast` + `count`, `flag`, `metricAtLeast` / `metricBelow` + `metric`, `incidentsAvoided`, `incidentsMitigated`, `competencyAtLeast` + `competency`, `allCompetenciesAtLeast`, `learningEntries`, `retryPassed`, `escalations`. All are evaluated from saved state (`IsAchievementMet`), so unlocks survive save/load and can be re-derived. `ValidateAchievements` checks unique ids, code ids, mission/metric/competency targets; `Data_EveryReferencedFlagCanBeSet` checks every flag an achievement or mission reads can be set by some choice or incident.

## Shipped content
30 missions (M001 hand-built plus 29 data missions, M027-M030 optional specialisms), 12 crises (C001-C003 required by the career ladder and always reachable; C004-C012 open only when an earlier decision leads to an incident), the CRO capstone, 50 achievements. `Data_FullCampaignBestPlayReachesChiefRiskOfficer` plays the best answer to every question from M001 to the capstone and checks the player is promoted through all ten levels on competence evidence.

| Id | Title | Kind | Level | Requires |
|---|---|---|---|---|
| C001 | Heatwave Week | crisis | 7 | M016 |
| C002 | The Lights Go Out | crisis | 8 | M020 |
| C003 | Contract on the Line | crisis | 9 | M024 |
| C004 | Eastfield Down | crisis | 2 | flag eastfield_breakdown |
| C005 | The A-Road Collision | crisis | 2 | flag fatigue_collision |
| C006 | Harrowgate Under Water | crisis | 3 | flag harrowgate_flooded |
| C007 | The Inspector's Letter | crisis | 3 | flag regulator_finding |
| C008 | Ransomware | crisis | 5 | flag ransomware_outbreak |
| C009 | Supplier Collapse | crisis | 7 | flag supplier_insolvency |
| C010 | Data Breach | crisis | 5 | flag data_breach |
| C011 | Fraud in the Depot | crisis | 5 | flag fraud_discovered |
| C012 | The Whistleblower | crisis | 8 | flag whistleblower |
| CRO | Chief Risk Officer | capstone | 10 | M026, C003 |
| M002 | The Contractor Nobody Checks | mission | 2 | M001 |
| M003 | Reading the Signals | mission | 2 | M002 |
| M004 | What Is It Worth? | mission | 3 | M003 |
| M005 | Testing the Control | mission | 3 | M003 |
| M006 | Near Misses | mission | 3 | M004, M005 |
| M007 | The Tail Matters | mission | 4 | M006 |
| M008 | Challenge, Not Sign-Off | mission | 4 | M007 |
| M009 | Reviewing the New Analyst | mission | 4 | M008 |
| M010 | Too Much Work, Too Few People | mission | 5 | M009 |
| M011 | The Breach Report | mission | 5 | M010 |
| M012 | Cyber Hygiene | mission | 5 | M011 |
| M013 | Setting the Appetite | mission | 6 | M012 |
| M014 | Stress Test the Plan | mission | 6 | M013 |
| M015 | The Framework Gap | mission | 6 | M014 |
| M016 | Buying Pellmoor | mission | 7 | M015 |
| M017 | One Supplier Away | mission | 7 | M016 |
| M018 | The Programme Is Slipping | mission | 7 | M017 |
| M019 | The Enterprise View | mission | 8 | M018, C001 |
| M020 | Writing for the Board | mission | 8 | M019 |
| M021 | Over the Horizon | mission | 8 | M020 |
| M022 | What People Really Think | mission | 8 | M021 |
| M023 | The Board Risk Committee | mission | 9 | M022, C002 |
| M024 | Cash Is Oxygen | mission | 9 | M023 |
| M025 | The Regulator Calls | mission | 9 | M024 |
| M026 | Strategy Under Uncertainty | mission | 9 | M025 |
| M027 | Leak Check | mission | 4 | M006 |
| M028 | The New Depot | mission | 5 | M009 |
| M029 | Rehearsal | mission | 6 | M012 |
| M030 | Following the Money | mission | 6 | M010 |
