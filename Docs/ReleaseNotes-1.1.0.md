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
- Walking looks natural. Colleagues ease into and out of their stride, round corners, turn on the spot, and walk at their animation's own stride, so their feet don't slide. Before, the body surged ahead and snapped back once a step: the game looked up the skeleton's bones by the wrong spelling, so the hips were never held in place. The same fix lets people turn their head and eyes to you, and puts each voice at the speaker's head.
- Idle colleagues fidget instead of standing frozen.
- Music and room tone now play at their intended level. The fade-in applied the volume a second time, which left the music 12-25 LU under the voices. Each music track now has its own level, set from its measured loudness, so every mood sits about 8 LU under dialogue.
- Faces no longer render white on Unreal Engine 5.8, and a focused colleague keeps their skin material.
- Walk-up lines no longer end in "?.".

## Verified for this release
97 simulation tests and 17 Unreal automation tests. Packaged Shipping self-tests: play (21 checks), resume (16 checks) and people (11 people, all with face shapes; a colleague walk-up; a walking burst at 121 cm/s, 58-60 fps). The full scripted walkthrough ran on the Shipping build from a new game to the CRO capstone, with save, quit and Continue. Installer and portable zip each passed their self-test. Details: Docs/QAReport.md.

## Install
Run `Riskbound-Setup-1.1.0.exe` (no admin rights needed) or unzip `Riskbound-1.1.0-Win64-Portable.zip` and run `Riskbound.exe`. Check downloads against the `.sha256` files. Saves are kept in `%LOCALAPPDATA%\Riskbound` when you uninstall, and 1.0.0 saves load unchanged.

## Known issues
- **Unsigned build.** Windows SmartScreen shows "Unknown publisher": choose **More info, then Run anyway**. Code signing is planned (Docs/CodeSigning.md).
- Voices are synthetic (Kokoro) and cover the fixed lines and conversations; generated or modded text has lip-sync without a voice.
- The crisis clock is session state: reloading a save restarts it.
- Controller play, resolutions other than 1920x1080, and the Analyst and Executive difficulties have been tested by automated checks, not by hand.
- No one has listened to the final mix on speakers: the levels come from loudness measurements (Docs/QAReport.md). Inside the server room the server hum is about 4 LU under a voice.
- One document (the M017 Corvane price note) measures 7 px wider than its box in the automated text check. Nothing visibly runs off.
- More: Docs/KnownIssues.md. Credits: in game (Credits) and Docs/AssetCredits.md.
