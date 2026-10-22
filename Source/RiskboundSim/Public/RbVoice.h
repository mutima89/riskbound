// Recorded voice lines: which clip belongs to a spoken line. Clips are made offline by
// Scripts/BuildVoices.py (Kokoro TTS) and imported as /Game/Riskbound/Audio/Voice/VO_<key>.
// The key is the speaker id and a hash of the line, so a clip follows its exact words: an edited
// line simply has no clip until the voices are rebuilt, and is then spoken with text-timed lip-sync.
// The same hash is implemented in Scripts/BuildVoices.py; Tests/Core/TestVoice.cpp pins both.
#pragma once

#include "RbApi.h"

#include <cstdint>
#include <string>

namespace rb
{
// The line as it is hashed: whitespace runs (spaces, tabs, newlines) collapsed to one space, trimmed.
RB_API std::string NormalizeVoiceLine(const std::string& Utf8Text);
// 32-bit FNV-1a over the bytes of the normalized line.
RB_API uint32_t VoiceLineHash(const std::string& Utf8Text);
// "<person>_<8 lower-case hex digits>", e.g. "priya_1a2b3c4d"; empty if either part is empty.
RB_API std::string VoiceLineKey(const std::string& PersonId, const std::string& Utf8Text);
} // namespace rb
