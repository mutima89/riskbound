#include "RbTest.h"

#include "RbVoice.h"

using namespace rb;

TEST(Voice_KeyIsStableAndMatchesTheBuildScript)
{
	// Published FNV-1a 32-bit vectors.
	CHECK(VoiceLineHash("") == 0x811c9dc5u);
	CHECK(VoiceLineHash("a") == 0xe40c292cu);
	CHECK(VoiceLineHash("foobar") == 0xbf9cf968u);
	// Layout and spacing do not change the clip; words and punctuation do. The same vectors are
	// checked by Scripts/BuildVoices.py before it generates anything.
	CHECK(NormalizeVoiceLine("  Two cases in.\n\nHow are you?  ") == "Two cases in. How are you?");
	CHECK(VoiceLineKey("priya", "  Two cases in.\n\nHow are you?  ") == "priya_4001ff99");
	CHECK(VoiceLineKey("priya", "Two cases in. How are you?") == "priya_4001ff99");
	CHECK(VoiceLineKey("priya", "It\xE2\x80\x99s fine.") == "priya_175ecf54");
	CHECK(VoiceLineKey("priya", "Two cases in. How are you!") != "priya_4001ff99");
	CHECK(VoiceLineKey("", "Hello.").empty());
	CHECK(VoiceLineKey("priya", " \n ").empty());
}
