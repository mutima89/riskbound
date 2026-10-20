// All credits in one place: the creator, the engine attribution, third-party assets (generated
// from Data/AssetManifest.json) and development assistance. Used by the Credits screen and tests.
#pragma once

#include "CoreMinimal.h"

namespace RbCredits
{
extern const TCHAR* const CreatorName;     // also used by the in-world easter eggs
extern const TCHAR* const CreatorEmail;
extern const TCHAR* const CreatorLinkedIn; // opened only by an explicit button press

struct FAssetCredit
{
	const TCHAR* Name;
	const TCHAR* Role;
	const TCHAR* Source;
	const TCHAR* Authors;
	const TCHAR* Licence;
};
const TArray<FAssetCredit>& Assets();

struct FSection
{
	FString Title;
	TArray<FString> Lines;
};
TArray<FSection> Sections();

// Every section as plain text (credits screen fallback and tests).
FString AsPlainText();
} // namespace RbCredits
