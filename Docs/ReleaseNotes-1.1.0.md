# Riskbound 1.1.0

From graduate to Chief Risk Officer, one real decision at a time. Offline, single-player, Windows 10/11 64-bit.

## What's new since 1.0.0

**People you can read**
- Faces: every colleague has a textured face that blinks, glances, turns to look at you and lip-syncs what they say. Smiles, frowns and lowered brows show how a conversation is going.
- Voiced colleagues: 128 lines for 12 characters (about 18.5 minutes) recorded offline with the open Kokoro-82M voice model (Apache 2.0). The voice plays from the speaker's head and the mouth movement is stretched to each recording, so it closes as the line ends. Lines without a recording still lip-sync from the text.
- Body language and reactions: talking gestures while speaking, a listening pose while you choose. A reply that helps the relationship gets a nod and a smile, one that hurts it a head shake and a frown.

**A working office**
- Walking colleagues: people walk between rooms through the doors and the corridor, stop for you and look at you.
- Seated meetings and the board: a meeting sits at the meeting-room table; when you present a case in the boardroom, the chair, the chief executive, finance and operations are seated at the table.
- Physical interaction: documents are lifted towards you, a filing cabinet hands over a folder, you lean in to screens and turn to face people. Conversations use a panel along the bottom so faces stay in view.
- Colleagues bring news: a new message from someone in the office is delivered in person. They walk over and tell you; read it now or later. If you are busy they leave it in your inbox.

**Pressure when it counts**
- Crisis alarm and board-call clock: a crisis opens with a red alert light in the monitoring room and corridor. On Professional the board call is in 10 minutes, on Executive 6 (Analyst has no clock). When the call starts, evidence you have not seen is out of reach: decide with what you have. Scoring is unchanged.

**Casework on foot**
- Walk-around casework: every evidence item in the 52 data cases now sits in an office room. Each case uses at least two rooms and at least one colleague in person. Reading stands spread the papers around each room, Priya's interviews happen at her own desk, and the objective shows how many stops are left.

## Fixes
- Music and room tone played at about a tenth of their intended level: the fade-in applied the volume a second time. Each music track now has its own level, so every mood sits about 8 LU under the voices. The crisis track was close to dialogue level, and the focus and tension tracks were nearly inaudible under it.
- Faces no longer render white on Unreal Engine 5.8, and a focused colleague keeps their skin material.
- Walk-up lines no longer end in "?.".
RESULTS_FIXES

## Verified for this release
RESULTS_VERIFIED

## Install
Run `Riskbound-Setup-1.1.0.exe` (no admin rights needed) or unzip `Riskbound-1.1.0-Win64-Portable.zip` and run `Riskbound.exe`. Check downloads against the `.sha256` files. Saves are kept in `%LOCALAPPDATA%\Riskbound` when you uninstall, and 1.0.0 saves load unchanged.

## Known issues
- **Unsigned build.** Windows SmartScreen shows "Unknown publisher": choose **More info, then Run anyway**. Code signing is planned (Docs/CodeSigning.md).
- Voices are synthetic (Kokoro) and cover the fixed lines and conversations; generated or modded text has lip-sync without a voice.
- The crisis clock is session state: reloading a save restarts it.
- Controller play, resolutions other than 1920x1080, and the Analyst and Executive difficulties have been tested by automated checks, not by hand.
- No one has listened to the final mix on speakers: the levels come from loudness measurements (Docs/QAReport.md).
- More: Docs/KnownIssues.md. Credits: in game (Credits) and Docs/AssetCredits.md.
