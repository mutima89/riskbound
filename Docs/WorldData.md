# World data format (schema 1)

The living world around the campaign is content in three files, read and validated at startup by `URbCampaignSubsystem::LoadWorld` and staged with the game (`Riskbound.Build.cs`). Code: `RbPeople.h`, `RbWorld.h/.cpp` (rules), `RbWorldData.cpp` (strict readers, `ValidateWorld`). Rules: Docs/GameDesign.md "Living world".

Reading is strict like mission files: unknown keys, wrong types, unknown metrics and duplicate keys are errors with file and line. If any world file has a problem the game loads no world content (missions still load) and logs `World data: ...`. The core test `World_ShippedContentParsesAndValidates` and the automation test `Riskbound.World.ShippedContentLoadsAndInitialises` fail on the same problems.

## Data/People.json
`{"schema": 1, "people": [person]}`. Person: `id` (lower-case, digits, `_`), `name`, `role`, `department`, `faction` (`growth`, `prudence` or absent), `bio`, `location` (HQ room), `executive` (bool), `personal` (bool: family, never in office politics), `startRelation` (-100..100).

## Data/Story.json
`{"schema": 1, "title", "clues": [clue]}`. Clue: `id`, `title`, `summary` (case-board text), `chapter` (1..4). Every clue must be obtainable from some evidence, event or reply (`ValidateWorld`).

## Data/Events.json
`{"schema": 1, "events": [event], "fallout": [template]}`.

Event: `id`, `category` (`story`, `incident`, `regulator`, `personal`, `politics`, `team`, `news`), `from` (person id), `subject`, `body` (string or paragraphs), `when` (condition), `probability` (per elapsed week once the condition holds, default 1), `deadlineWeeks` (0..12; non-zero means a reply is needed), `defaultChoice` (applied when the deadline passes; required with choices), `choices`, `clue` (gained on delivery), `headline` + `headlineTone` (-1, 0, 1).

Condition (`when`), all parts must hold: `flags`, `notFlags`, `passed` (mission ids), `minWeek`, `minLevel`, `maxLevel`, `minClues`, `minStress`, `minIncidents`, `metrics` (`[{"metric": "Reputation", "below": 50}]` or `atLeast`), `relations` (`[{"id": "callum", "atLeast": 20}]` or `below`).

Choice: `id`, `label` (a label starting with `(` such as `(No reply)` is the default and is not shown as a button), `outcome`, `effects` (`{"Metric": change}`), `relations` (`{"person": change}`, within 40), `stress` (-30..30), `setsFlags`, `clue`, `schedules` (consequences, as in mission files).

Fallout template: `department` (matches a mission's department; `default` is required), `description` (`{mission}` and `{choice}` are replaced), `headline`, `effects` (money effects are per mission level).

## Mission file additions (Docs/MissionData.md)
- Option: `relations`, `stress`.
- Evidence: `clue`.
- Context note: `ifRelation` + `relationBelow`/`relationAtLeast`, `ifClues`.
- Mission: `requiresClues`, `deadlineWeeks` (incident-triggered crises only) with `escalation: {"text", "effects"}`. A mission that another mission requires can never have a deadline (`ValidateCatalogue`).
