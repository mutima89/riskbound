# Voice-over

Colleagues speak their lines aloud. The clips are made offline and shipped as ordinary imported sounds:
no speech engine, model or network is used while the game runs.

## How a line finds its clip
- Every spoken line goes through `ARbHUD::Speak(PersonId, Line)` -> `URbPersonComponent::Say(Text)`.
- `Say` asks `URbAudioSubsystem::FindVoice` for `/Game/Riskbound/Audio/Voice/VO_<key>`, where
  `<key> = rb::VoiceLineKey(PersonId, Line)` (Source/RiskboundSim/Public/RbVoice.h): the speaker id plus a
  32-bit FNV-1a hash of the line with whitespace collapsed. Scripts/BuildVoices.py computes the same key;
  `Voice_KeyIsStableAndMatchesTheBuildScript` (core) and the script's own check pin both to the same vectors.
- With a clip, the sound plays from the speaker's head (full volume at conversation distance, fading across a
  room) and the existing text-driven viseme track is stretched to the clip's length, so the mouth starts and
  stops with the voice. Without one (an edited line not yet rebuilt, or a line built at run time such as
  Priya's reminder with the current objective), the person speaks silently with text-timed lip-sync as before.
- Closing the dialogue, the next line, or the person walking off stops the clip.

## Which lines are recorded
Scripts/BuildVoices.py collects:
- conversation lines (Data/Dialogue.json);
- interviews: evidence with a `person` in Data/Missions/*.json (the body is what they say);
- walk-ups: "Have you got a minute? <subject>" for team, politics and story messages from office colleagues
  (Data/Events.json, same rules as `ARbHUD::TickApproach`), plus "You're busy. It's in your inbox.";
- fixed lines that live in game code (Mission 001 briefing with and without hints, self-test lines), copied
  into Data/VoiceCast.json `fixed`. `Riskbound.Voice.FixedLinesHaveClips` fails if the code and the copy drift.

## Casting
Data/VoiceCast.json maps each character to a Kokoro voice (or a weighted blend), British or American
pronunciation, and pace. `pronounce` holds spoken-form fixes for the TTS input only (R-014 -> "R oh 1 4",
1.2m -> "1.2 million"); acronyms are spelled out letter by letter. The key always uses the line as written.

## Rebuilding after a content change
```
pip install kokoro-onnx soundfile
python Scripts/BuildVoices.py                 # speaks only new or changed lines; removes stale clips
set RB_AUDIO_GROUPS=Voice
UnrealEditor-Cmd.exe Riskbound.uproject -run=pythonscript -script=Scripts/ImportAudio.py -unattended -nopause
```
`--list` prints every line, its key and the spoken form without generating anything. The model
(Kokoro-82M v1.0, Apache 2.0, ~350 MB) downloads once to Data/Assets/raw/voice and is checked by SHA-256.
It runs on a CPU (about 2 seconds per line); no GPU needed. Licences: Docs/AssetCredits.md "Voices".
