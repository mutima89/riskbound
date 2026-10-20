# Save format (version 2)

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

Any key change: bump `CurrentSaveVersion`, add `MigrateVnToVn+1`, add a test that loads a handcrafted old file.

## Player recovery instructions
Saves live in `<game folder>/Riskbound/Saved/SaveGames/slot1.rbsave` (packaged) or the project's `Saved/SaveGames/` (editor). `selftest.rbsave` belongs to the self-test. To restore manually: close the game, rename `slotN.rbsave.bak` to `slotN.rbsave`.
