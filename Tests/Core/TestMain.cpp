#include "RbTest.h"

#include <cstring>

int main(int argc, char** argv)
{
	const char* Filter = argc > 1 ? argv[1] : nullptr;
	int Ran = 0;
	int FailedCases = 0;
	for (const rbtest::Case& C : rbtest::Registry())
	{
		if (Filter && std::strstr(C.Name, Filter) == nullptr) continue;
		const int Before = rbtest::Failures();
		C.Fn();
		++Ran;
		const bool bOk = rbtest::Failures() == Before;
		if (!bOk) ++FailedCases;
		std::printf("[%s] %s\n", bOk ? " OK " : "FAIL", C.Name);
	}
	std::printf("\n%d test cases, %d failed, %d failed checks\n", Ran, FailedCases, rbtest::Failures());
	return FailedCases == 0 && Ran > 0 ? 0 : 1;
}
