#include "RbJson.h"

#include <cstdlib>
#include <cstring>

namespace rb
{
const JsonValue* JsonValue::Find(const std::string& Key) const
{
	if (Type != EJsonType::Object) return nullptr;
	for (const auto& M : Members)
	{
		if (M.first == Key) return &M.second;
	}
	return nullptr;
}

namespace
{
constexpr int MaxDepth = 64;

class Parser
{
public:
	explicit Parser(const std::string& InText) : Text(InText) {}

	bool Document(JsonValue& Out)
	{
		// Optional UTF-8 BOM (editors on Windows add it).
		if (Text.size() >= 3 && static_cast<unsigned char>(Text[0]) == 0xEF && static_cast<unsigned char>(Text[1]) == 0xBB &&
			static_cast<unsigned char>(Text[2]) == 0xBF)
		{
			Pos = 3;
		}
		SkipSpace();
		if (!Value(Out, 0)) return false;
		SkipSpace();
		if (Pos != Text.size()) return Fail("unexpected text after the document");
		return true;
	}

	std::string Error;

private:
	const std::string& Text;
	size_t Pos = 0;
	int Line = 1;
	size_t LineStart = 0;

	bool Fail(const std::string& Msg)
	{
		if (Error.empty())
		{
			Error = "line " + std::to_string(Line) + ", column " + std::to_string(Pos - LineStart + 1) + ": " + Msg;
		}
		return false;
	}

	void SkipSpace()
	{
		while (Pos < Text.size())
		{
			const char C = Text[Pos];
			if (C == '\n')
			{
				++Line;
				LineStart = Pos + 1;
			}
			else if (C != ' ' && C != '\t' && C != '\r')
			{
				break;
			}
			++Pos;
		}
	}

	bool Literal(const char* Word)
	{
		const size_t N = std::strlen(Word);
		if (Text.compare(Pos, N, Word) != 0) return false;
		Pos += N;
		return true;
	}

	bool Value(JsonValue& Out, int Depth)
	{
		if (Depth > MaxDepth) return Fail("nesting too deep");
		if (Pos >= Text.size()) return Fail("unexpected end of file");
		Out = JsonValue();
		Out.Line = Line;
		const char C = Text[Pos];
		if (C == '{') return Object(Out, Depth);
		if (C == '[') return Array(Out, Depth);
		if (C == '"')
		{
			Out.Type = EJsonType::String;
			return String(Out.String);
		}
		if (C == '-' || (C >= '0' && C <= '9')) return NumberValue(Out);
		if (Literal("true"))
		{
			Out.Type = EJsonType::Bool;
			Out.Bool = true;
			return true;
		}
		if (Literal("false"))
		{
			Out.Type = EJsonType::Bool;
			return true;
		}
		if (Literal("null")) return true;
		return Fail(std::string("unexpected character '") + C + "'");
	}

	bool Object(JsonValue& Out, int Depth)
	{
		Out.Type = EJsonType::Object;
		++Pos; // {
		SkipSpace();
		if (Pos < Text.size() && Text[Pos] == '}')
		{
			++Pos;
			return true;
		}
		while (true)
		{
			SkipSpace();
			if (Pos >= Text.size() || Text[Pos] != '"') return Fail("expected a quoted key");
			std::string Key;
			if (!String(Key)) return false;
			if (Out.Find(Key)) return Fail("duplicate key \"" + Key + "\"");
			SkipSpace();
			if (Pos >= Text.size() || Text[Pos] != ':') return Fail("expected ':' after key \"" + Key + "\"");
			++Pos;
			SkipSpace();
			JsonValue V;
			if (!Value(V, Depth + 1)) return false;
			Out.Members.emplace_back(std::move(Key), std::move(V));
			SkipSpace();
			if (Pos < Text.size() && Text[Pos] == ',')
			{
				++Pos;
				continue;
			}
			if (Pos < Text.size() && Text[Pos] == '}')
			{
				++Pos;
				return true;
			}
			return Fail("expected ',' or '}' in object");
		}
	}

	bool Array(JsonValue& Out, int Depth)
	{
		Out.Type = EJsonType::Array;
		++Pos; // [
		SkipSpace();
		if (Pos < Text.size() && Text[Pos] == ']')
		{
			++Pos;
			return true;
		}
		while (true)
		{
			SkipSpace();
			JsonValue V;
			if (!Value(V, Depth + 1)) return false;
			Out.Items.push_back(std::move(V));
			SkipSpace();
			if (Pos < Text.size() && Text[Pos] == ',')
			{
				++Pos;
				continue;
			}
			if (Pos < Text.size() && Text[Pos] == ']')
			{
				++Pos;
				return true;
			}
			return Fail("expected ',' or ']' in array");
		}
	}

	static void AppendUtf8(std::string& S, unsigned Cp)
	{
		if (Cp < 0x80)
		{
			S += static_cast<char>(Cp);
		}
		else if (Cp < 0x800)
		{
			S += static_cast<char>(0xC0 | (Cp >> 6));
			S += static_cast<char>(0x80 | (Cp & 0x3F));
		}
		else if (Cp < 0x10000)
		{
			S += static_cast<char>(0xE0 | (Cp >> 12));
			S += static_cast<char>(0x80 | ((Cp >> 6) & 0x3F));
			S += static_cast<char>(0x80 | (Cp & 0x3F));
		}
		else
		{
			S += static_cast<char>(0xF0 | (Cp >> 18));
			S += static_cast<char>(0x80 | ((Cp >> 12) & 0x3F));
			S += static_cast<char>(0x80 | ((Cp >> 6) & 0x3F));
			S += static_cast<char>(0x80 | (Cp & 0x3F));
		}
	}

	bool Hex4(unsigned& Out)
	{
		if (Pos + 4 > Text.size()) return Fail("truncated \\u escape");
		Out = 0;
		for (int i = 0; i < 4; ++i)
		{
			const char H = Text[Pos++];
			Out <<= 4;
			if (H >= '0' && H <= '9') Out |= static_cast<unsigned>(H - '0');
			else if (H >= 'a' && H <= 'f') Out |= static_cast<unsigned>(H - 'a' + 10);
			else if (H >= 'A' && H <= 'F') Out |= static_cast<unsigned>(H - 'A' + 10);
			else return Fail("invalid \\u escape");
		}
		return true;
	}

	bool String(std::string& Out)
	{
		++Pos; // opening quote
		Out.clear();
		while (Pos < Text.size())
		{
			const char C = Text[Pos++];
			if (C == '"') return true;
			if (static_cast<unsigned char>(C) < 0x20) return Fail("control character in string (use \\n)");
			if (C != '\\')
			{
				Out += C;
				continue;
			}
			if (Pos >= Text.size()) break;
			const char E = Text[Pos++];
			switch (E)
			{
			case '"': Out += '"'; break;
			case '\\': Out += '\\'; break;
			case '/': Out += '/'; break;
			case 'b': Out += '\b'; break;
			case 'f': Out += '\f'; break;
			case 'n': Out += '\n'; break;
			case 'r': Out += '\r'; break;
			case 't': Out += '\t'; break;
			case 'u':
			{
				unsigned Cp = 0;
				if (!Hex4(Cp)) return false;
				if (Cp >= 0xD800 && Cp <= 0xDBFF)
				{
					unsigned Lo = 0;
					if (Text.compare(Pos, 2, "\\u") != 0) return Fail("unpaired surrogate");
					Pos += 2;
					if (!Hex4(Lo) || Lo < 0xDC00 || Lo > 0xDFFF) return Fail("invalid surrogate pair");
					Cp = 0x10000 + ((Cp - 0xD800) << 10) + (Lo - 0xDC00);
				}
				AppendUtf8(Out, Cp);
				break;
			}
			default: return Fail(std::string("invalid escape '\\") + E + "'");
			}
		}
		return Fail("unterminated string");
	}

	bool NumberValue(JsonValue& Out)
	{
		const size_t Start = Pos;
		if (Text[Pos] == '-') ++Pos;
		auto Digits = [&]() {
			const size_t D = Pos;
			while (Pos < Text.size() && Text[Pos] >= '0' && Text[Pos] <= '9') ++Pos;
			return Pos > D;
		};
		if (!Digits()) return Fail("invalid number");
		if (Pos < Text.size() && Text[Pos] == '.')
		{
			++Pos;
			if (!Digits()) return Fail("invalid number");
		}
		if (Pos < Text.size() && (Text[Pos] == 'e' || Text[Pos] == 'E'))
		{
			++Pos;
			if (Pos < Text.size() && (Text[Pos] == '+' || Text[Pos] == '-')) ++Pos;
			if (!Digits()) return Fail("invalid number");
		}
		const std::string Token = Text.substr(Start, Pos - Start);
		Out.Type = EJsonType::Number;
		Out.Number = std::strtod(Token.c_str(), nullptr);
		return true;
	}
};
} // namespace

bool ParseJson(const std::string& Text, JsonValue& Out, std::string& OutError)
{
	Parser P(Text);
	JsonValue V;
	if (!P.Document(V))
	{
		OutError = P.Error;
		return false;
	}
	Out = std::move(V);
	OutError.clear();
	return true;
}
} // namespace rb
