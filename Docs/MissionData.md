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

Question: `{"id", "prompt", "kind": "single"|"multi", "competency", "weight", "options": [...]}`.
Competencies: `RiskIdentification, RiskAssessment, ControlEvaluation, QuantitativeAnalysis, Communication, StakeholderManagement, Judgement, Leadership, StrategicThinking, Governance`.

Option: `{"id", "label", "feedback", "score" (single), "correct" (multi), "cost", "escalates", "effects", "setsFlags", "schedules"}`.
- `cost` above the player's approval authority, without `"escalates": true`, caps that answer at 40.
- `effects`: `{"Metric": change}`. Metrics: `Revenue, Costs, Cash` (currency) and the 0..100 indices `RiskExposure, ControlEffectiveness, Reputation, StakeholderTrust, EmployeeMorale, Resilience, StrategicProgress`.
- `schedules`: consequences `{"id", "description", "delayWeeks", "probability", "effects", "flagIfOccurs", "mitigatedBy", "mitigatedProbabilityFactor", "mitigatedSeverityFactor"}`. Probability is multiplied by the difficulty factor (0.8 / 1.0 / 1.2).

## Authoring rules
- All names are fictional (Docs/GameDesign.md "Fictional names").
- Every figure stated in an option must be what `RbQuant` computes; add a test like `Data_M004FiguresMatchTheQuantEngine`.
- Never state expected loss as the worst case; offer the confusion as a wrong answer with feedback instead.
- Feedback explains why, for wrong and right answers. The rubric is only shown after submission.
- A crisis required for promotion (ladder) must not depend on a random flag; flag-triggered crises are optional extras.

## Shipped content
| Id | Title | Level | Requires | Assesses |
|---|---|---|---|---|
| M002 | The Contractor Nobody Checks | 2 | M001 | identification, assessment, control evaluation, judgement, communication |
| M003 | Reading the Signals | 2 | M002 | identification, assessment, control evaluation, judgement, communication |
| M004 | What Is It Worth? | 3 | M003 | quantitative analysis (x2), assessment, judgement, communication |
| M005 | Testing the Control | 3 | M003 | control evaluation, assessment, quantitative analysis, judgement, communication |
