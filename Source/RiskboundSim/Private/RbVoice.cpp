#include "RbVoice.h"

namespace rb
{
std::string NormalizeVoiceLine(const std::string& Utf8Text)
{
	std::string Out;
	Out.reserve(Utf8Text.size());
	bool bSpace = false;
	for (const char C : Utf8Text)
	{
		if (C == ' ' || C == '\t' || C == '\n' || C == '\r')
		{
			bSpace = !Out.empty();
			continue;
		}
		if (bSpace) Out.push_back(' ');
		bSpace = false;
		Out.push_back(C);
	}
	return Out;
}

uint32_t VoiceLineHash(const std::string& Utf8Text)
{
	uint32_t H = 2166136261u;
	for (const char C : NormalizeVoiceLine(Utf8Text))
	{
		H ^= static_cast<uint8_t>(C);
		H *= 16777619u;
	}
	return H;
}

std::string VoiceLineKey(const std::string& PersonId, const std::string& Utf8Text)
{
	if (PersonId.empty() || NormalizeVoiceLine(Utf8Text).empty()) return std::string();
	static const char Hex[] = "0123456789abcdef";
	const uint32_t H = VoiceLineHash(Utf8Text);
	std::string Key = PersonId + "_";
	for (int Shift = 28; Shift >= 0; Shift -= 4) Key.push_back(Hex[(H >> Shift) & 0xFu]);
	return Key;
}
} // namespace rb
