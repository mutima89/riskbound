// Minimal dependency-free test harness for the simulation core (runs without Unreal).
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace rbtest
{
struct Case
{
	const char* Name;
	std::function<void()> Fn;
};

inline std::vector<Case>& Registry()
{
	static std::vector<Case> R;
	return R;
}

inline int& Failures()
{
	static int F = 0;
	return F;
}

struct Registrar
{
	Registrar(const char* Name, std::function<void()> Fn) { Registry().push_back({Name, std::move(Fn)}); }
};

inline void Fail(const char* File, int Line, const std::string& Msg)
{
	++Failures();
	std::printf("    FAIL %s:%d: %s\n", File, Line, Msg.c_str());
}
} // namespace rbtest

#define RB_CAT2(a, b) a##b
#define RB_CAT(a, b) RB_CAT2(a, b)
#define TEST(Name) \
	static void Name(); \
	static rbtest::Registrar RB_CAT(Reg_, Name)(#Name, Name); \
	static void Name()

#define CHECK(Cond) \
	do { if (!(Cond)) rbtest::Fail(__FILE__, __LINE__, #Cond); } while (0)

// Like CHECK, but ends the test case on failure (use before indexing into a container).
#define REQUIRE(Cond) 	do { if (!(Cond)) { rbtest::Fail(__FILE__, __LINE__, "required: " #Cond); return; } } while (0)

#define CHECK_EQ(A, B) \
	do { const auto _a = (A); const auto _b = (B); \
		if (!(_a == _b)) rbtest::Fail(__FILE__, __LINE__, std::string(#A " == " #B " (got ") + std::to_string(_a) + " vs " + std::to_string(_b) + ")"); } while (0)

#define CHECK_NEAR(A, B, Tol) \
	do { const double _a = (A); const double _b = (B); \
		if (std::fabs(_a - _b) > (Tol)) rbtest::Fail(__FILE__, __LINE__, std::string(#A " ~= " #B " (got ") + std::to_string(_a) + " vs " + std::to_string(_b) + ")"); } while (0)
