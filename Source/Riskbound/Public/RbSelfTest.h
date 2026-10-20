// Packaged-build self check.
//   -RbSelfTest=play    new game, plays Mission 001 through the HUD/interactable calls the UI uses, saves, quits.
//   -RbSelfTest=resume  loads that save the way Continue does, checks it, writes Saved/RbSelfTest.txt, quits.
//   -RbSelfTest=tour    visits every room without player input, renders a screenshot per room
//                       (Saved/Screenshots/<platform>/Tour_<room>.png) and writes frame timings to Saved/RbTour.txt.
// Uses its own save slot ("selftest") so the player's saves are never touched.
#pragma once

#include "CoreMinimal.h"
#include "RbTypes.h"

class URbGameSubsystem;
class UWorld;

namespace RbSelfTest
{
extern const TCHAR* const SlotName;

bool IsRequested();
FString RequestedMode();
void Run(UWorld* World);

// A complete, well-evidenced answer to the R-014 assessment (what a strong player submits).
FRbSubmissionInput StrongAssessment();

// Direct subsystem play-through used by automation tests.
void PlayMission001(URbGameSubsystem* Game);
} // namespace RbSelfTest
