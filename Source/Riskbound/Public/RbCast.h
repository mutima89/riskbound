#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class UAnimSequence;

// How Valtren's people look in the office: each named person has a fixed avatar (Microsoft Rocketbox,
// MIT licence, imported by Scripts/ImportCharacters.py); background staff draw from the rest.
// Falls back to the engine mannequin when the imported avatars are missing.
namespace RbCast
{
struct FLook
{
	USkeletalMesh* Mesh = nullptr;
	UAnimSequence* Idle = nullptr;
	float MeshYaw = -90.0f;  // turns the mesh to face the actor's forward axis
	float Scale = 1.0f;
	bool bFallback = true;   // the mannequin stand-in (callers dress it in an outfit material)
	bool bFemale = false;    // picks the f_/m_ animation set
};

// A Rocketbox animation for one body (Name without the f_/m_ prefix, e.g. "gestic_talk_neutral_01"),
// or null when it was not imported. The mannequin stand-in has none.
UAnimSequence* Anim(bool bFemale, const TCHAR* Name);

// A named person (People.json id). Idle: 0 standing, 1 listening (interviews), 2 relaxed breathing.
FLook ForPerson(const FString& PersonId, int32 Idle = 0);
// Background staff member number Index (stable per index, varied idles).
FLook ForStaff(int32 Index);
}
