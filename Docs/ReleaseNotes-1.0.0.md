# Riskbound 1.0.0

From graduate to Chief Risk Officer, one real decision at a time. Offline, single-player, Windows 10/11 64-bit.

## What's new since 0.5.1-alpha

**A living company**
- Colleagues remember you: 15 characters with relationships, conversations with tones, and an inbox of requests, favours and regulator letters with reply deadlines.
- Stress and burnout, office politics, a share price and a news crawl that react to your decisions.
- Weak answers leave latent risk that can surface weeks later; the incident report names the decision. Crises say what caused them and escalate if you wait.
- *The Corvane Ledger*: a fraud story told through ten clues across your career.

**Real risk work in the office**
- Interview people where they work and find evidence in the rooms it lives in.
- Board papers are presented in person at the boardroom lectern.
- Numeric questions: type the figure you calculated and get scored on how close you are.
- Six new case studies inspired by real events (39 missions, 12 crises, CRO capstone).

**Your career, your way**
- Four starting backgrounds, ten skills, promotion and CRO ceremonies in the boardroom.
- Eight endings, a performance review and an exportable certificate, and what-if replays of past decisions.
- Survival, Daily and Classroom modes with a local leaderboard, a 40-term glossary, photo mode (F12) and mission mods.

**Look, sound and comfort**
- Realistic office cast with natural idle animations (Microsoft Rocketbox, MIT).
- Adaptive music by Kevin MacLeod, office ambience, footsteps and interface sounds.
- Controller support, TSR upscaling, frame cap, vsync, invert look, colour-safe palette. Reduced motion now cuts to scenes instead of moving the camera.
- Optional AI colleagues: off by default; with your own Anthropic API key you can talk to colleagues in your own words. The rest of the game never goes online.

## Fixes
- Every promotion, from level 3 to Chief Risk Officer, plays its boardroom ceremony (it was missing with reduced motion on).
- Debrief and journal text no longer runs past its box.
- The CRO lectern scrolls to Submit.

## Verified for this release
95 simulation tests, 16 Unreal automation tests, packaged self-tests (play, resume, room tour) and the full scripted walkthrough from a new game to the CRO capstone on the Shipping build. Details: Docs/Progress.md.

## Install
Run `Riskbound-Setup-1.0.0.exe` (no admin rights needed) or unzip `Riskbound-1.0.0-Win64-Portable.zip` and run `Riskbound.exe`. The build is unsigned: on the SmartScreen warning choose **More info, then Run anyway**. Check downloads against the `.sha256` files. Saves are kept in `%LOCALAPPDATA%\Riskbound` when you uninstall.

Known issues: Docs/KnownIssues.md. Credits: in game (Credits) and Docs/AssetCredits.md.
