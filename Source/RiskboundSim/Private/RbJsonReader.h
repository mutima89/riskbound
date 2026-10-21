// Internal helper for strict content readers (missions, achievements, people, events, story).
// Not part of the public API.
#pragma once

#include "RbJson.h"
#include "RbMissionData.h"
#include "RbPeople.h"

#include <cmath>
#include <set>
#include <string>
#include <vector>

namespace rb
{
namespace json
{
// Reads one JSON object into a struct, recording errors and rejecting keys it was not asked about.
class Reader
{
public:
	Reader(const JsonValue& InObj, const std::string& InWhere, std::vector<std::string>& InErrors)
		: Obj(InObj), Where(InWhere), Errors(InErrors)
	{
		if (!Obj.IsObject()) Error(Obj.Line, "expected an object");
	}

	~Reader()
	{
		if (!Obj.IsObject()) return;
		for (const auto& M : Obj.Members)
		{
			if (!Seen.count(M.first)) Error(M.second.Line, "unknown key \"" + M.first + "\"");
		}
	}

	void Error(int Line, const std::string& Msg) { Errors.push_back(Where + " (line " + std::to_string(Line) + "): " + Msg); }

	const JsonValue* Get(const char* Key, bool bRequired)
	{
		Seen.insert(Key);
		const JsonValue* V = Obj.Find(Key);
		if (!V && bRequired) Error(Obj.Line, std::string("missing \"") + Key + "\"");
		return V;
	}

	void Str(const char* Key, std::string& Out, bool bRequired)
	{
		const JsonValue* V = Get(Key, bRequired);
		if (!V) return;
		if (V->IsString()) Out = V->String;
		else Error(V->Line, std::string("\"") + Key + "\" must be a string");
	}

	// A string, or an array of strings joined as paragraphs.
	void Text(const char* Key, std::string& Out, bool bRequired)
	{
		const JsonValue* V = Get(Key, bRequired);
		if (!V) return;
		if (V->IsString())
		{
			Out = V->String;
			return;
		}
		if (V->IsArray())
		{
			Out.clear();
			for (const JsonValue& P : V->Items)
			{
				if (!P.IsString())
				{
					Error(P.Line, std::string("\"") + Key + "\" paragraphs must be strings");
					continue;
				}
				if (!Out.empty()) Out += "\n\n";
				Out += P.String;
			}
			return;
		}
		Error(V->Line, std::string("\"") + Key + "\" must be a string or an array of strings");
	}

	void Int(const char* Key, int& Out, bool bRequired)
	{
		const JsonValue* V = Get(Key, bRequired);
		if (!V) return;
		if (V->IsNumber() && std::floor(V->Number) == V->Number && std::fabs(V->Number) < 1e9) Out = static_cast<int>(V->Number);
		else Error(V->Line, std::string("\"") + Key + "\" must be a whole number");
	}

	void Dbl(const char* Key, double& Out, bool bRequired)
	{
		const JsonValue* V = Get(Key, bRequired);
		if (!V) return;
		if (V->IsNumber()) Out = V->Number;
		else Error(V->Line, std::string("\"") + Key + "\" must be a number");
	}

	void Bool(const char* Key, bool& Out)
	{
		const JsonValue* V = Get(Key, false);
		if (!V) return;
		if (V->IsBool()) Out = V->Bool;
		else Error(V->Line, std::string("\"") + Key + "\" must be true or false");
	}

	void Strings(const char* Key, std::vector<std::string>& Out)
	{
		const JsonValue* V = Get(Key, false);
		if (!V) return;
		if (!V->IsArray())
		{
			Error(V->Line, std::string("\"") + Key + "\" must be an array of strings");
			return;
		}
		for (const JsonValue& S : V->Items)
		{
			if (S.IsString()) Out.push_back(S.String);
			else Error(S.Line, std::string("\"") + Key + "\" items must be strings");
		}
	}

	// {"Reputation": -2, "Cash": -5000}
	void Effects(const char* Key, std::vector<MetricEffect>& Out)
	{
		const JsonValue* V = Get(Key, false);
		if (!V) return;
		if (!V->IsObject())
		{
			Error(V->Line, std::string("\"") + Key + "\" must be an object of metric: change");
			return;
		}
		for (const auto& M : V->Members)
		{
			EOrgMetric Metric;
			if (!MetricFromName(M.first, Metric)) Error(M.second.Line, "unknown metric \"" + M.first + "\"");
			else if (!M.second.IsNumber()) Error(M.second.Line, "effect on " + M.first + " must be a number");
			else Out.push_back({Metric, M.second.Number});
		}
	}

	// {"priya": 5, "hollis": -10}
	void Relations(const char* Key, std::vector<RelationEffect>& Out)
	{
		const JsonValue* V = Get(Key, false);
		if (!V) return;
		if (!V->IsObject())
		{
			Error(V->Line, std::string("\"") + Key + "\" must be an object of character: change");
			return;
		}
		for (const auto& M : V->Members)
		{
			if (!M.second.IsNumber() || std::floor(M.second.Number) != M.second.Number) Error(M.second.Line, "relation change for " + M.first + " must be a whole number");
			else Out.push_back({M.first, static_cast<int>(M.second.Number)});
		}
	}
	const JsonValue& Obj;
	std::string Where;
	std::vector<std::string>& Errors;

private:
	std::set<std::string> Seen;
};

// Shared by mission options and inbox event choices (defined in RbMissionData.cpp).
void ReadConsequence(const JsonValue& V, const std::string& Where, ConsequenceDef& C, std::vector<std::string>& Errors);
} // namespace json
} // namespace rb
