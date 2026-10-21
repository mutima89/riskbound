# Known issues (1.0.0)

Honest list for players. Severity: **High** affects most players, **Medium** noticeable, **Low** cosmetic.

| Area | Issue | Severity | Notes |
|---|---|---|---|
| Release | The executable and installer are unsigned, so Windows SmartScreen warns "Unknown publisher". | Medium | Needs a code-signing certificate (Docs/Release.md "Signing"). |
| Characters | Faces don't animate when people speak; conversations and interviews are on screen. | Low | MetaHuman faces need a GPU with more than 4 GB of video memory to author (Scripts/CreateMetaHumans.py is ready). |
| Characters | A few background staff share an avatar with a named person in another room. | Low | 20 avatars for 14 named people plus staff. |
| Career | There is no level-1 tutorial (M000); new games start at level 2, Junior Risk Analyst. | Low | |
| Content | Some wrong answers may still be easy to rule out. | Low | Tuning from playtests. |
| UI | Notices show one at a time while a screen is open (the newest replaces older ones). | Low | By design, to avoid covering screen content. |
| Monitoring room | Part of the KRI status strip is hidden behind the operator desks from some angles. | Low | |
| Saves | The checksum detects accidental corruption, not deliberate editing. Edited saves load with values brought into range. | Low | By design (single-player, offline). |
| Language | English only. | Low | |
| Hardware | Tested on one PC (GeForce GTX 1650, 1920x1080). Use Low or Medium graphics, or upscaling, if the frame rate is poor. | Low | |

Fixed since 0.5.1-alpha: music and sound, controller support, realistic office cast (no mannequins), missions with in-person evidence and boardroom presentations, crises that name their cause, fallout from weak decisions, case list grouping, the binder interaction target, and promotion ceremonies at every level (including with reduced motion).
