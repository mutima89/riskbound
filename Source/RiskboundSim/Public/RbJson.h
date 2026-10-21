// Minimal strict JSON reader for content files (Data/). No exceptions; errors carry line:column.
// Supports objects, arrays, strings (with \uXXXX -> UTF-8), numbers, true/false/null.
#pragma once

#include "RbApi.h"

#include <string>
#include <utility>
#include <vector>

namespace rb
{
enum class EJsonType
{
	Null,
	Bool,
	Number,
	String,
	Array,
	Object
};

struct JsonValue
{
	EJsonType Type = EJsonType::Null;
	bool Bool = false;
	double Number = 0.0;
	std::string String;
	std::vector<JsonValue> Items;                           // Array
	std::vector<std::pair<std::string, JsonValue>> Members; // Object, in file order
	int Line = 0;                                           // where the value starts (1-based)

	RB_API const JsonValue* Find(const std::string& Key) const; // Object member or nullptr
	bool IsObject() const { return Type == EJsonType::Object; }
	bool IsArray() const { return Type == EJsonType::Array; }
	bool IsString() const { return Type == EJsonType::String; }
	bool IsNumber() const { return Type == EJsonType::Number; }
	bool IsBool() const { return Type == EJsonType::Bool; }
};

// Parses a whole document. On failure returns false and sets OutError to "line L, column C: message".
// Duplicate object keys are an error (content typos must not silently override each other).
RB_API bool ParseJson(const std::string& Text, JsonValue& Out, std::string& OutError);
} // namespace rb
