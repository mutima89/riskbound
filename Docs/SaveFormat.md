# Save format (version 9)

Code: `RbSave.*`. Tests: `Save_*` in `Tests/Core/TestMissionSave.cpp`.

## File layout
UTF-8 text, one field per line:
```
RISKBOUND-SAVE
version<TAB>2
key<TAB>escaped value
...
checksum<TAB><16 hex digits>
```
- Values escape `\\`, `\t`, `\n`, `\r`. Doubles use `%.17g` (exact round trip).
- Lists: `key.n` count then `key.0..n-1`. Counts above 100,000 are rejected.
- Checksum: FNV-1a 64 over every byte before the `checksum` line. Detects corruption, not tampering.
- The Mission 001 debrief is not stored; it is recomputed from the stored submission (scoring is pure).

## Write procedure (`SaveGameToFile`)
1. Write `<slot>.tmp`, flush.
2. Re-read and fully parse the temp file. Abort if invalid.
3. If `<slot>` exists and is valid, copy it to `<slot>.bak`. If it exists but is invalid, rename it to `<slot>.corrupt` and leave `.bak` untouched.
4. Rename `.tmp` over `<slot>`.

A corrupt save can never overwrite a valid backup.

## Load procedure (`LoadGameFromFile`)
Primary valid -> `Ok`. Else backup valid -> `RecoveredFromBackup` (UI must tell the player). Neither -> `Corrupt` (or `NotFound`). A newer version returns `UnsupportedVersion` and never falls back, so a downgrade cannot load stale data silently. Loading never writes.

## Versions and migration
| Version | Change |
|---|---|
| 1 | Prototype: `player`, text `difficulty` (easy/normal/hard), no `org.draws`. |
| 2 | `profile.name`, numeric `profile.difficulty`, `org.draws` (random stream position). |
| 3 | `act.*` corrective actions (`act.next`, per action id/risk/title/owner/created/due/started/done/doneWeek/evidence), `kri.*` KRI history (`kri.seed`, per series id and `h.<n>.week`/`.value`; status, amber streak and escalation are re-derived on load), `quant.*` player loss scenarios (risk, `s.<n>.name/p/loss`). |
| 4 | No new keys. Organisation ids `meridian`/`aldmoor`/`kestrel` become `valtren`/`orrinbrook`/`quellard`, and the old organisation and contractor display names are replaced in every saved string (Docs/GameDesign.md "Fictional names"). Test: `Save_MigratesV3OrganisationNamesToFictionalNames`. |
| 5 | `camp.active`, `camp.viewed` (evidence opened in the active mission), `camp.rec.<n>` per submitted attempt (`.id`, `.week`, `.level`, `.score`, `.passed`, answers `.a.<k>.q` + option list `.a.<k>.o`), and `org.pend.<n>.setflag` (flag set if the consequence occurs). Older saves load with no campaign progress and no consequence flags. Test: `Save_MigratesV4WithoutCampaign`, round trip `Save_V5RoundTripsCampaignAndConsequenceFlags`. |
| 6 | The living world: `world.stress`, `world.burnouts`, `world.nextMsg`, `world.clues`, `world.fired` (events delivered), `world.rel.<n>` (`.id`, `.v` -100..100), `world.msg.<n>` (`.id`, `.event`, `.cat`, `.from`, `.subject`, `.body`, `.week`, `.deadline` (-1: no reply), `.read`, `.choice`, `.outcome`, `.auto`), `world.news.<n>` (`.week`, `.text`, `.tone`), `world.share.<n>` (`.week`, `.v`), `world.open.<n>` (crisis `.id`, `.week` it opened), and `org.hist.<n>.setflag`. Older saves load with stress 20, an empty inbox and no clues; relationship start values come from Data/People.json on load (`InitWorld`). Out-of-range stress or relations are rejected. Test: `Save_V6RoundTripsTheWorldAndMigratesV5`. |
| 7 | `world.background`, `world.skills` (learned skill ids), `world.talks` (conversations had). Older saves load with no background and no skills. Test: `Growth_SkillsAreEarnedLearnedInOrderAndSaved`. |
| 8 | Challenge modes: `chal.mode` (0 career, 1 survival, 2 daily), `chal.seq` (mission ids in order), `chal.index`, `chal.lives`, `chal.points`, `chal.over`, `chal.date` (daily seed date). Older saves load as career games. An index past the sequence is rejected. |
| 9 | Display and input settings: `settings.upscale` (0 native, 1 quality, 2 balanced, 3 performance; TSR), `settings.fps` (0 uncapped), `settings.vsync`, `settings.invertY`, `settings.pad` (controller look sensitivity), `settings.colourSafe`. Older saves load with defaults (native resolution, uncapped, vsync on); values are clamped by `ClampSettings`. |

v2 -> v3 (`MigrateV2ToV3`): KRI history is rebuilt deterministically from the organisation and seed (12 weeks ending at the current week); if Mission 001 was submitted, its treatment actions are recreated with the decision week as the creation week; no quantified risks. Test: `Save_MigratesHandcraftedV2`.

Invalid v3 data rejected: a done action without evidence, duplicate action ids, an unknown KRI id, invalid loss scenarios.

Any key change: bump `CurrentSaveVersion`, add `MigrateVnToVn+1`, add a test that loads a handcrafted old file.

## Player recovery instructions
Saves live in `<game folder>/Riskbound/Saved/SaveGames/slot1.rbsave` (packaged) or the project's `Saved/SaveGames/` (editor). `selftest.rbsave` belongs to the self-test. To restore manually: close the game, rename `slotN.rbsave.bak` to `slotN.rbsave`.
