// UTF-8 std::string <-> FString at the boundary between RiskboundSim and Unreal.
#pragma once

#include "CoreMinimal.h"

#include <string>
#include <vector>

namespace RbConvert
{
inline FString ToFString(const std::string& S)
{
	const FUTF8ToTCHAR Conv(S.data(), static_cast<int32>(S.size()));
	return FString(Conv.Length(), Conv.Get());
}

inline std::string ToStd(const FString& S)
{
	const FTCHARToUTF8 Conv(*S, S.Len());
	return std::string(Conv.Get(), Conv.Length());
}

inline TArray<FString> ToFStrings(const std::vector<std::string>& In)
{
	TArray<FString> Out;
	Out.Reserve(static_cast<int32>(In.size()));
	for (const std::string& S : In)
	{
		Out.Add(ToFString(S));
	}
	return Out;
}

inline std::vector<std::string> ToStdVector(const TArray<FString>& In)
{
	std::vector<std::string> Out;
	Out.reserve(In.Num());
	for (const FString& S : In)
	{
		Out.push_back(ToStd(S));
	}
	return Out;
}
} // namespace RbConvert
