// Hands-on-style walkthrough of the whole game through the real UI (-RbSelfTest=walkthrough, then
// -RbSelfTest=walkthrough2 in a new process). Unlike -RbSelfTest=play, it never calls game rules
// directly: it injects real Slate input (mouse moves and clicks hit-tested on the widgets on screen,
// key presses routed through Enhanced Input, raw mouse deltas for looking) and walks the office.
// It audits every screen for clipped text and writes Saved/RbWalkthrough_<phase>.txt plus
// screenshots (Saved/Screenshots/<platform>/Walk_*). Input is only injected while the game window is
// the active application; if focus is lost it waits, then stops and reports.
#pragma once

#include "CoreMinimal.h"

class ARbHUD;
class UWorld;

namespace RbWalkthrough
{
// Phase 1: main menu, new game, Mission 001 in the office, risk tools, campaign to Team Lead (with a
// failed-and-retried mission and a deliberately weak decision until an incident opens a crisis),
// settings, save and quit. Phase 2: Continue from the main menu, rest of the campaign to the CRO
// capstone, journal.
void Start(UWorld* World, ARbHUD* Hud, int32 Phase);
}
