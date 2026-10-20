@echo off
rem Builds and runs the simulation-core unit tests on Windows (needs CMake + Visual Studio C++ tools).
setlocal
set ROOT=%~dp0..
cmake -S "%ROOT%" -B "%ROOT%\build" || exit /b 1
cmake --build "%ROOT%\build" --config Debug || exit /b 1
"%ROOT%\build\Debug\RiskboundCoreTests.exe" %*
