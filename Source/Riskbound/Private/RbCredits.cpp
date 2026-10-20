#include "RbCredits.h"

namespace RbCredits
{
const TCHAR* const CreatorName = TEXT("M A Almahlaki");
const TCHAR* const CreatorEmail = TEXT("m.awwadh.k@gmail.com");
const TCHAR* const CreatorLinkedIn = TEXT("https://www.linkedin.com/in/mohammed-almahlaki001/");

const TArray<FAssetCredit>& Assets()
{
	static const TArray<FAssetCredit> List = {
#include "Generated/RbAssetCredits.inl"
	};
	return List;
}

TArray<FSection> Sections()
{
	TArray<FSection> Out;
	Out.Add({TEXT("Created by"), {CreatorName, FString::Printf(TEXT("Email: %s"), CreatorEmail), FString::Printf(TEXT("LinkedIn: %s"), CreatorLinkedIn)}});
	Out.Add({TEXT("Built with Unreal Engine 5.8"),
		{TEXT("Riskbound uses the Unreal® Engine. Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere."),
		 TEXT("Unreal® Engine, Copyright 1998-2026, Epic Games, Inc. All rights reserved.")}});
	FSection AssetSection{TEXT("Assets"), {}};
	for (const FAssetCredit& A : Assets())
	{
		AssetSection.Lines.Add(FString::Printf(TEXT("%s - %s (%s, %s, %s)"), A.Name, A.Role, A.Authors, A.Source, A.Licence));
	}
	AssetSection.Lines.Add(TEXT("Screen interfaces, whiteboard and wall prints: original Riskbound artwork."));
	AssetSection.Lines.Add(TEXT("Mannequin body for Priya Okafor: Epic Games template content."));
	Out.Add(AssetSection);
	Out.Add({TEXT("Development assistance"), {TEXT("Claude Code by Anthropic")}});
	Out.Add({TEXT("Fictional names"), {TEXT("All companies, people and products in Riskbound are invented. Any resemblance to real organisations is coincidental.")}});
	return Out;
}

FString AsPlainText()
{
	FString Text;
	for (const FSection& S : Sections())
	{
		Text += S.Title + TEXT("\n");
		for (const FString& L : S.Lines) Text += TEXT("  ") + L + TEXT("\n");
		Text += TEXT("\n");
	}
	return Text;
}
} // namespace RbCredits
