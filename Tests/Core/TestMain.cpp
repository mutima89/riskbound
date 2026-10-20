#include "RbTest.h"

#include <cstring>

#if defined(_MSC_VER)
#include <crtdbg.h>
#include <cstdlib>
#endif

int main(int argc, char** argv)
{
#if defined(_MSC_VER)
	// Never block on a modal dialog: Debug CRT assertions (e.g. vector subscript out of range)
	// print to stderr and the process exits with a failure code instead.
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
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
