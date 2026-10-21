# Riskbound 0.5.1-alpha - release notes

Fixes from a complete walkthrough of the Shipping build through the real UI, from a new game to the Chief Risk Officer capstone (Docs/WalkthroughReport.md). Offline, single-player, Windows 10/11 64-bit. Saves from 0.5.0-alpha load unchanged.

## Install
1. Download `Riskbound-Setup-0.5.1-alpha.exe` (installer, no administrator rights needed) or `Riskbound-0.5.1-alpha-Win64-Portable.zip` (unzip anywhere and run `Riskbound.exe`).
2. Optional: check the download against its `.sha256` file, e.g. in PowerShell `Get-FileHash .\Riskbound-Setup-0.5.1-alpha.exe`.
3. **Windows SmartScreen will warn "Windows protected your PC / Unknown publisher"**, because the build is not code-signed. Choose **More info**, then **Run anyway**. `UNSIGNED-BUILD.txt` (in the game folder) explains why.
4. Uninstalling keeps your saves in `%LOCALAPPDATA%\Riskbound`.

Controls: WASD to move, mouse to look, **E** interact, **J** journal, **Esc** pause.

## Fixed
- **Campaign dead end (blocker).** A player who passed every case at their level with weak evidence could be stuck with no case left to play. When that happens, passed cases at your level now reopen as **reviews**: they add competence evidence but change nothing in the organisation and take no time.
- **The best answer was usually the longest.** In 97% of questions the best option was the longest one. 634 answer labels were rewritten as plausible, reasoned choices; the best answer is now the longest in 38% of questions, and a test keeps it that way.
- Risk tools: the open tool's tab is now highlighted (the Risk register tab used to stay highlighted); the register table no longer cuts off text (wider columns, ellipsis and tooltips; heatmap moved beside the add-risk form); appetite breaches show in red; Loss scenarios and Cost-benefit labels no longer wrap awkwardly.
- Assessment form: the title no longer collides with the "sections complete" counter; the submit hint stays muted until the form is complete.
- Settings: Fullscreen shows the actual window mode.
- At the top of the ladder the debrief and journal say "You are at the top of the career ladder" instead of listing it as a missing requirement.

## Known issues
- Not yet played through by a person; the walkthrough was scripted.
- Unsigned build (SmartScreen warning above).
- No music or sound effects.
- Incidents from weak decisions are rare, and an incident-triggered crisis does not name the decision that caused it.
- The meeting-room maintenance binder can need a small turn of the view before the E prompt appears.
- The Case files list is long (33 entries).
- Keyboard and mouse only; English only; missions after 001 are played in the Case files screen.

Full list: Docs/KnownIssues.md.

## Quality
75 simulation tests and 11 Unreal automation tests pass; the Shipping walkthrough completed both phases (new game to CRO, save, quit, Continue); packaged self-tests (play, resume, tour at 79.7 fps, 1080p High on a GTX 1650) pass; the installer was installed, run and uninstalled (saves kept). See Docs/QAReport.md.

## SHA-256
```
75982a8268fde08430f901d6b7848e719bf17cfbba80850cfd7c7513bf265193  Riskbound-Setup-0.5.1-alpha.exe
f6093b2480c5f9fb67b49ac7e85c79403134f5283f1b5566d7b4edbf51888b71  Riskbound-0.5.1-alpha-Win64-Portable.zip
```
